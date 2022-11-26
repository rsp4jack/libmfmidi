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
#include <format>
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
            using namespace std::literals;
            pause();
            mthread.request_stop();
            while (!mthreadexit) {
                mcv.notify_all(); // wake up sleeping thread to stop
                std::this_thread::sleep_for(2ms);
            }
        }

        void initThread()
        {
            if (!mthread.joinable()) {
                mthread = std::jthread([&](const std::stop_token& tok) {
                    playerthread(tok);
                });
            }
        }

        void pause() noexcept
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
            }
        }

        [[nodiscard]] bool isPlaying() const noexcept
        {
            return mplaying;
        }

        [[nodiscard]] MIDIClockTime tickTime() const noexcept
        {
            return mabsTime;
        }

        void reset() noexcept
        {
            revertSnapshot(defaultSnapshot());
        }

        void setDivision(MIDIDivision div) noexcept
        {
            mdiv = div;
            reCalcDivns();
        }

        void setTrack(const MIDITrack& trk) noexcept
        {
            mtrk = &trk;
            revertSnapshot(defaultSnapshot());
            if (museCache) {
                generateCahce();
            }
        }

        void setDriver(AbstractMIDIDevice* dev) noexcept
        {
            mdev = dev;
        }

        void setMsgProcessor(MIDIProcessorFunction func) noexcept
        {
            mprocfunc = std::move(func);
        }

        /// \exception std::out_of_range \a clktime is out of range
        /// Exception safety: \b Basic
        /// Like \a goTo(MIDIClockTime,std::nothrow_t) , it will goTo the end if out range
        /// \sa goTo(MIDIClockTime,std::nothrow_t)
        void goTo(MIDIClockTime clktime)
        {
            if (!goTo(clktime, std::nothrow)) {
                throw std::out_of_range(std::format("clktime {} is out of range", clktime));
            }
        }

        /// \brief nothrow version of goTo
        /// it will goTo the end if out range.
        /// \sa goTo(MIDIClockTime)
        bool goTo(MIDIClockTime clktime, std::nothrow_t /*unused*/) noexcept
        {
            bool const toPlay = mplaying;
            pause(); // avoid data race

            if (museCache && !mcache.empty()) {
                auto it      = mcache.begin();
                auto lit     = it;
                bool matched = false;
                while (it != mcache.end()) {
                    if (it->first == clktime) {
                        matched = true;
                        revertSnapshot(it->second);

                        if (toPlay) {
                            play();
                        }
                        return true;
                    }
                    if (it->first > clktime) {
                        matched = true;
                        if (lit != it) { // not begin
                            revertSnapshot(it->second);
                            // directGoTo for the rest
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
                        revertSnapshot(lit->second); // revert to the latest cache
                    } else {
                        std::unreachable();
                    }
                }
            } else {
                revertSnapshot(defaultSnapshot()); // cant use cache
            }

            const bool result = directGoTo(clktime); // for the rest
            revertState();

            if (toPlay) {
                play();
            }
            return result;
        }

        /// \brief If you changed \a mtrk , call this.
        void regenerateAllSnapshots()
        {
            bool const toPlay = mplaying;
            pause();
            MIDIClockTime const clktime = mabsTime;
            revertSnapshot(defaultSnapshot());
            if (museCache) {
                generateCahce();
            }
            goTo(clktime, std::nothrow);
            if (toPlay) {
                play();
            }
        }

        std::jthread::native_handle_type nativeHandle() noexcept
        {
            return mthread.native_handle();
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

        void reCalcDivns() noexcept
        {
            mdivns = static_cast<unsigned long long>(divisionToSec(mdiv, mstate.tempo) * 1000 * 1000 * 1000);
        }

        void revertState() noexcept
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

        bool directGoTo(MIDIClockTime clktime) noexcept
        {
            while (mabsTime < clktime) {
                if (mrelckltime >= mcurit->deltaTime()) {
                    mstproc.process(*mcurit);
                    mrelckltime = 0;
                    if (mcurit == mtrk->end() - 1) {
                        return false;
                    }
                    ++mcurit;
                }
                ++mabsTime;
                ++mrelckltime;
            }
            return true;
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
            mthreadexit = true;
        }

        // Multi-thread
        std::jthread            mthread;
        std::mutex              mcvmutex;
        std::condition_variable mcv;
        bool                    mthreadexit = false;

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
