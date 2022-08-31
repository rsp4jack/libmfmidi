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

/// \file multithreadedtrackplayer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief A very simple multithreaded MIDITrack player

#pragma once

#include "miditrack.hpp"
#include <future>
#include <condition_variable>
#include <functional>
#include <mutex>
#include "abstractmididevice.hpp"

// Together for a std::shared_future!

namespace libmfmidi {
    class MultithreadedTrackPlayer {
    public:
        void play() {}

        void pause() {}

        void goZero() {}

        bool isPlaying() const noexcept
        {
        }

    private:
        AbstractMIDIDevice* drv;
        std::reference_wrapper<MIDITrack> mtrk;
        MIDITrack::iterator cur;
        std::mutex mcvlock;
        std::condition_variable mcv;
        bool mbcv = false;
        std::future<void>       mplayerasync;
        bool                              mplay = false;
        uint32_t                          tempobpm;
        int16_t division;

        void _playerthread()
        {
            std::unique_lock<std::mutex> unilock(mcvlock);
            while (true) {
                if (!mplay) {
                    mcv.wait(unilock, [&]() {
                        return mbcv;
                    });
                } else { // TODO: we must have a test about remove this line
                    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<unsigned long long>(divisionToSec(division, tempobpm) * 1000000)));
                    
                }
            }
        }
    };
}
