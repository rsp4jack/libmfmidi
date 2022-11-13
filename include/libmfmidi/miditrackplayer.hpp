/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// \file miditrackplayer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDITrack lightweight player

#pragma once

#include "abstractmididevice.hpp"
#include "miditrack.hpp"
#include "midistate.hpp"
#include "abstracttimer.hpp"
#include <memory>
#include <map>
#include <thread>
#include <stop_token>
#include <mutex>
#include <condition_variable>
#include "nanosleep.hpp"
#include <atomic>

namespace libmfmidi {
    /// \brief MIDITrack player powerful
    /// Support navigation, MIDIState calc and recovery
    class MIDITrackPlayer {
    public:
        static constexpr int TICK_PER_CACHE = 2048000;

        explicit MIDITrackPlayer() noexcept
        {
            mstproc.setNotifier([&](NotifyType type) {
                if (type == NotifyType::C_Tempo) {
                    reCalcDivns();
                }
            });
        }

        ~MIDITrackPlayer() noexcept
        {
            mcv.notify_all(); // wake up sleeping thread to stop
        }

        void pause()
        {
            mplaying = false;
        }

        void play()
        {
            if (!mthread.joinable()) {
                initThread();
            }
            if (!mplaying) {
                mplaying = true;
                mcv.notify_all();
                // UNUSED(mpausebarrier.arrive());
            }
        }

        [[nodiscard]] MIDIClockTime tickTime() const noexcept
        {
            return mabsTime;
        }

        void reset() noexcept
        {
            mrelckltime = 0;
            mabsTime    = 0;
            mstate.resetAll();
        }

        void setDivision(MIDIDivision div) noexcept
        {
            mdiv = div;
            reCalcDivns();
        }

        void setTrack(const MIDITrack& trk)
        {
            mtrk   = &trk;
            mcurit = mtrk->cbegin();
            if (museCache) {
                generateCahce();
            }
        }

        void setDriver(AbstractMIDIDevice* dev)
        {
            mdev = dev;
        }

        void setMsgProcessor(MIDIProcessorFunction func)
        {
            mprocfunc = std::move(func);
        }

        void goTo(MIDIClockTime clktime) noexcept
        {
            if (museCache && !mcache.empty()) {
                auto it      = mcache.begin();
                auto lit     = it;
                bool matched = false;
                while (it != mcache.end()) {
                    if (it->first == clktime) {
                        matched = true;
                        revertSnapshot(it->second);
                        return;
                    }
                    if (it->first > clktime) {
                        matched = true;
                        if (lit != it) { // not begin
                            revertSnapshot(it->second);
                            // directGoTo to pad remaining
                        } else {
                            // first cache is later; fallback to directGoTo
                            revertSnapshot(defaultSnapshot());
                        }
                        break;
                    }
                    lit = it;
                    ++it;
                }
                if (!matched) {
                    if (lit->second.absTime < clktime) {
                        revertSnapshot(lit->second);
                    }
                }
            } else {
                revertSnapshot(defaultSnapshot());
            }
            directGoTo(clktime);
            revertSt();
        }

    private:
        struct Snapshot {
            MIDIClockTime             absTime;
            MIDIClockTime             relckltime;
            MIDIState                 state;
            MIDITrack::const_iterator curit;
        };

        void revertSnapshot(const Snapshot& sht) noexcept
        {
            mabsTime    = sht.absTime;
            mrelckltime = sht.relckltime;
            mstate      = sht.state;
            mcurit      = sht.curit;
        }

        Snapshot defaultSnapshot() const noexcept
        {
            return {0, 0, {}, mtrk->cbegin()};
        }

        void initThread()
        {
            mthread = std::jthread([&](const std::stop_token& tok) {
                playerthread(tok);
            });
        }

        void reCalcDivns()
        {
            mdivns = static_cast<unsigned long long>(divisionToSec(mdiv, mstate.tempo) * 1000 * 1000 * 1000);
        }

        void revertSt() noexcept
        {
            auto rst = reportMIDIState(mstate, false);
            for (auto& i : rst) {
                mdev->sendMsg(i);
            }
        }

        void generateCahce() noexcept
        {
            mcache.clear();
            MIDIClockTime      absTime      = 0;
            MIDIClockTime      relTime      = 0;
            MIDIClockTime      relCacheTime = 0;
            MIDIState          state;
            MIDIStateProcessor stateProc{state};
            auto               curit = mtrk->cbegin();
            while (true) {
                if (curit == mtrk->cend()) {
                    break;
                }
                if (relCacheTime >= TICK_PER_CACHE) {
                    mcache[absTime] = Snapshot{absTime, relTime, state, curit};
                }
                if (relTime >= curit->deltaTime()) {
                    stateProc.process(*curit);
                    ++curit;
                    relTime = 0;
                }
                ++relTime;
                ++relCacheTime;
                ++absTime;
            }
        }

        void directGoTo(MIDIClockTime clktime)
        {
            while (mabsTime < clktime) {
                if (mcurit == mtrk.cend()) {
                    throw mf_;
                }
                if (mrelckltime >= mcurit->deltaTime()) {
                    mstproc.process(*mcurit);
                    mrelckltime = 0;
                    ++mcurit;
                }
                ++mabsTime;
                ++mrelckltime;
            }
        }

        void playerthread(const std::stop_token& tok)
        {
            while (!tok.stop_requested()) {
                if (!mplaying) {
                    std::unique_lock<std::mutex> ulk{mcvmutex};
                    mcv.wait(ulk, [&] { return mplaying; });
                }
                MIDITimedMessage tempmsg;
                while (true) { // for repeated 0 delta time msg
                    if (mcurit == mtrk->cend()) {
                        mplaying = false;
                        break;
                    }
                    if (mrelckltime >= mcurit->deltaTime()) {
                        tempmsg = *mcurit; // TODO: state change(like divns)
                        mstproc.process(tempmsg);
                        if (mprocfunc(tempmsg) && !tempmsg.isMFMarker()) {
                            mdev->sendMsg(tempmsg);
                        }
                        mrelckltime = 0;
                        ++mcurit;
                        continue;
                    }

                    break;
                }
                nanosleep(mdivns);
                ++mabsTime;
                ++mrelckltime;
            }
        }

        // Multi-thread
        std::jthread            mthread;
        std::mutex              mcvmutex;
        std::condition_variable mcv;

        // Settings
        bool museCache = true; // use MIDIState cache

        const MIDITrack* mtrk{};

        // Playing info
        MIDIDivision       mdiv   = 0;
        unsigned long long mdivns = 0; // division to nanosec
        bool               mplaying{false};

        // Snapshot-able
        MIDIClockTime             mabsTime    = 0; // Current abs tick time
        MIDIClockTime             mrelckltime = 0;
        MIDIState                 mstate;
        MIDITrack::const_iterator mcurit;

        MIDIStateProcessor    mstproc{mstate};
        MIDIProcessorFunction mprocfunc;
        AbstractMIDIDevice*   mdev{};

        // Cache
        std::map<MIDIClockTime, Snapshot> mcache;
    };
}
