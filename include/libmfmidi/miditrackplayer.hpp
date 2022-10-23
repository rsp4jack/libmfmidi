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
#include <barrier>
#include "nanosleep.hpp"
#include <atomic>

namespace libmfmidi {
    /// \brief MIDITrack player powerful
    /// Support navigation, MIDIState calc and recovery
    class MIDITrackPlayer {
    public:
        static constexpr int TICK_PER_CACHE = 2048;

        explicit MIDITrackPlayer() noexcept = default;

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
                mpausebarrier.arrive_and_wait();
            }
        }

        void reset() noexcept
        {
            mrelckltime = 0;
            mabsTime    = 0;
            mstate.resetAll();
        }

        void setDivision(MIDIDivision div) noexcept
        {
            mdivns = static_cast<unsigned long long>(divisionToSec(div, mstate.tempo) * 1000 * 1000 * 1000);
        }

        void setTrack(const MIDITrack& trk)
        {
            mtrk   = &trk;
            mcurit = mtrk->cbegin();
            if (museCache && mrevertState) {
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
            if (mrevertState) {
                mrelckltime = 0;
                if (museCache && !mcache.empty()) {
                    auto it  = mcache.begin();
                    auto lit = it;
                    while (it != mcache.end()) {
                        if (it->first == clktime) {
                            mabsTime = clktime;
                            mstate   = it->second;
                            return;
                        }
                        if (it->first > clktime) {
                            if (lit != it) { // not begin
                                mabsTime = lit->first;
                                mstate   = lit->second;
                                // directGoTo to pad remaining
                            } else {
                                mabsTime = 0; // first cache is bigger; fallback to directGoTo
                                mstate.resetAll();
                            }
                            break;
                        }
                        lit = it;
                        ++it;
                    }
                }
                if (clktime > mabsTime) {
                    mabsTime = 0; // begin from initial
                    mstate.resetAll();
                }
                directGoTo(clktime);
            } else {
                mstate.resetAll();
                mabsTime = clktime;
            }
        }

    private:
        void initThread()
        {
            mthread = std::jthread([&](const std::stop_token& tok) {
                playerthread(tok);
            });
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
            MIDIClockTime      absTime = 0;
            MIDIClockTime      relTime = 0;
            MIDIState          state;
            MIDIStateProcessor stateProc{state};
            for (const auto& msg : *mtrk) {
                absTime += msg.deltaTime();
                relTime += msg.deltaTime();
                stateProc.process(msg);
                if (relTime >= TICK_PER_CACHE) {
                    mcache[absTime] = state;
                    relTime -= TICK_PER_CACHE;
                }
            }
        }

        void directGoTo(MIDIClockTime clktime) noexcept
        {
            if (mrevertState) {
                while (mabsTime < clktime) {
                    while (true) { // for repeated 0 delta time msg
                        if (mrelckltime >= mcurit->deltaTime()) {
                            mstproc.process(*mcurit);
                            mrelckltime = 0;
                            ++mcurit;
                            continue;
                        }
                        break;
                    }
                    ++mabsTime;
                    ++mrelckltime;
                }
            }
        }

        void playerthread(const std::stop_token& tok)
        {
            while (!tok.stop_requested()) {
                if (!mplaying) {
                    mpausebarrier.arrive_and_wait();
                }
                MIDITimedMessage tempmsg;
                while (true) { // for repeated 0 delta time msg
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

        std::jthread                       mthread;
        const MIDITrack*                   mtrk{};
        unsigned long long                 mdivns = 0; // division to nanosec
        std::barrier<>                     mpausebarrier{2};
        MIDIClockTime                      mabsTime    = 0; // Current abs tick time
        MIDIClockTime                      mrelckltime = 0;
        MIDIState                          mstate;
        MIDIStateProcessor                 mstproc{mstate};
        MIDIProcessorFunction              mprocfunc;
        AbstractMIDIDevice*                mdev{};
        bool                               mplaying{false};
        MIDITrack::const_iterator          mcurit;
        bool                               museCache    = true; // use MIDIState cache
        bool                               mrevertState = true; // revert MIDIState when navigate
        std::map<MIDIClockTime, MIDIState> mcache;
        std::vector<MIDIClockTime>         mtrkabsimes;
    };
}
