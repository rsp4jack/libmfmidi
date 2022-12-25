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

#include "libmfmidi/abstractmididevice.hpp"
#include "libmfmidi/miditrack.hpp"
#include "libmfmidi/midistatus.hpp"
#include <format>
#include <memory>
#include <map>
#include <thread>
#include <stop_token>
#include <mutex>
#include <condition_variable>
#include "platformapi.hpp"
#include <atomic>

namespace libmfmidi {
    /// \brief MIDITrack player powerful
    /// Support navigation, MIDIStatus calc and recovery
    class MIDITrackPlayer {
    public:
        static constexpr int TICK_PER_CACHE = 2048000;

        explicit MIDITrackPlayer() noexcept
        {
            mstproc.setNotifier([&](NotifyType type) {
                if (type == NotifyType::C_Tempo) {
                    reCalcDivus();
                }
                notify(type);
            });
        }

        ~MIDITrackPlayer() noexcept
        {
            using namespace std::literals;
            pause();
            mthread.request_stop();
            mwakeup = true;
            while (!mthreadexit) {
                mcv.notify_all(); // wake up sleeping thread to stop
                std::this_thread::sleep_for(2ms);
            }
        }

        void notify(NotifyType type)
        {
            if (mnotifier) {
                mnotifier(type);
            }
        }

        [[nodiscard]] bool useCache() const noexcept
        {
            return museCache;
        }

        void setUseCache(bool use) noexcept
        {
            museCache = use;
        }

        void setNotifier(MIDINotifierFunctionType func)
        {
            mnotifier = std::move(func);
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
                mwakeup  = true;
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
            reCalcDivus();
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
        bool goTo(MIDIClockTime target, std::nothrow_t /*unused*/) noexcept
        {
            // should play after goto
            const bool toPlay = mplaying;
            pause(); // avoid data race

            if (museCache && !mcache.empty()) {
                auto it      = mcache.begin(); // current msg
                auto lit     = it; // last msg
                bool matched = false;
                while (it != mcache.end()) {
                    if (it->first == target) { // there is a cache time==target
                        matched = true;
                        revertSnapshot(it->second);

                        if (toPlay) { // play if it is playing before goto
                            play();
                        }
                        return true;
                    }
                    if (it->first > target) { // this cache is later than target, revert to pervious cache (lit)
                        matched = true;
                        if (lit != it) { // not the first cache
                            revertSnapshot(lit->second);
                            // directGoTo for the rest
                        } else {
                            // first cache is later than target; fallback to directGoTo
                            revertSnapshot(defaultSnapshot());
                        }
                        break; // jump to directGoTo
                    }
                    lit = it;
                    ++it;
                }
                if (!matched) {
                    if (lit->second.absTime < target) { // safe check if lastest cache is earlier than target, should always be true
                        revertSnapshot(lit->second); // all cache is eariler than target, revert to the latest cache
                    } else {
                        std::unreachable();
                    }
                }
            } else {
                revertSnapshot(defaultSnapshot()); // cannot use cache, go to 0 and directGoTo
            }

            const bool result = directGoTo(target); // for the rest
            revertState(); // send state to midi device

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
            MIDIClockTime             compensation;
            MIDIStatus                 state;
            MIDITrack::const_iterator curit;
        };

        void revertSnapshot(const Snapshot& sht) noexcept
        {
            mabsTime    = sht.absTime;
            mcompensation = sht.compensation;
            mstate      = sht.state;
            mcurit      = sht.curit;
        }

        Snapshot defaultSnapshot() const noexcept
        {
            return {0, 0, {}, mtrk->cbegin()};
        }

        void reCalcDivus() noexcept
        {
            mdivns = static_cast<unsigned long long>(divisionToSec(mdiv, mstate.tempo) * 1000 * 1000 * 1000);
        }

        void revertState() noexcept
        {
            auto rst = reportMIDIStatus(mstate, false);
            for (auto& i : rst) {
                mdev->sendMsg(i);
            }
        }

        void generateCahce() noexcept
        {
            mcache.clear();
            MIDIClockTime      absTime      = 0;
            MIDIClockTime      relCacheTime = 0;
            MIDIStatus          state;
            MIDIStatusProcessor stateProc{state};
            auto               curit = mtrk->cbegin();
            while (true) {
                if (curit == mtrk->cend()) {
                    break;
                }
                if (relCacheTime >= TICK_PER_CACHE) {
                    mcache[absTime] = Snapshot{absTime, 0, state, curit};
                    relCacheTime = 0;
                }
                stateProc.process(*curit);
                relCacheTime += curit->deltaTime();
                absTime += curit->deltaTime();
                ++curit;
            }
        }

        bool directGoTo(MIDIClockTime clktime) noexcept
        {
            while (mabsTime < clktime) {
                if (mabsTime + mcurit->deltaTime() > clktime) {
                    mcompensation = clktime - mabsTime;
                    return true;
                }
                mstproc.process(*mcurit);
                if (mcurit == mtrk->end() - 1) {
                    return false;
                }
                ++mcurit;
                mabsTime += mcurit->deltaTime();
            }
            return true;
        }

        void playerthread(const std::stop_token& tok)
        {
            while (!tok.stop_requested()) {
                if (!mplaying) {
                    notify(NotifyType::T_Mode);
                    std::unique_lock<std::mutex> ulk{mcvmutex};
                    mcv.wait(ulk, [&] { return mwakeup; });
                    mwakeup = false;
                    if(mplaying){
                        notify(NotifyType::T_Mode);
                    }
                }
                if (mcurit == mtrk->cend()) {
                    mplaying = false;
                    notify(NotifyType::T_EndOfSong);
                    break;
                }

                MIDITimedMessage tempmsg;
                tempmsg = *mcurit;
                mstproc.process(tempmsg);
                if (mprocfunc(tempmsg) && !tempmsg.isMFMarker()) {
                    if (mcompensation != 0) {
                        if (mcompensation < mcurit->deltaTime()) {
                            nanosleep((mcurit->deltaTime() - mcompensation) * mdivns);
                            mcompensation = 0;
                        } else {
                            mcompensation -= mcurit->deltaTime();
                        }
                    } else {
                        nanosleep(mdivns * mcurit->deltaTime());
                    }
                    mdev->sendMsg(tempmsg);
                    mabsTime += tempmsg.deltaTime();
                }
                ++mcurit;
            }
            mthreadexit = true;
        }

        // Multi-thread
        std::jthread            mthread{};
        std::mutex              mcvmutex{};
        std::condition_variable mcv{};
        bool                    mthreadexit = false;
        bool                    mwakeup     = false;

        // Settings
        bool museCache = true; // use MIDIStatus cache

        const MIDITrack* mtrk{};

        // Playing info
        MIDIDivision       mdiv{};
        unsigned long long mdivns = 0; // division to microsec
        bool               mplaying{false};

        // Snapshot-able
        MIDIClockTime             mabsTime      = 0; // Current abs tick time
        MIDIClockTime             mcompensation = 0; // sleep less time
        MIDIStatus                 mstate{};
        MIDITrack::const_iterator mcurit{};

        MIDIStatusProcessor       mstproc{mstate};
        MIDIProcessorFunction    mprocfunc{};
        AbstractMIDIDevice*      mdev{};
        MIDINotifierFunctionType mnotifier{};

        // Cache
        std::map<MIDIClockTime, Snapshot> mcache{};
    };
}
