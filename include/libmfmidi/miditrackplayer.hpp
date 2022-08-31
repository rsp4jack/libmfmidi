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
#include <unordered_map>

namespace libmfmidi {
    /// \brief Simple MIDITrack player but powerful
    /// Very fast
    class MIDITrackPlayer {
    public:
        explicit MIDITrackPlayer() noexcept {}

    private:
        bool stopTimer()
        {
            return mtimer->stop();
        }
        void tick()
        {
            if (mnstat == Pause) {
                stopTimer();
                return;
            }
            
        }

        // Navigate status
        enum NavStatus {
            Normal,
            Forward,
            Backward,
            Pause
        };

        MIDIClockTime                       mabsTime = 0;
        std::unique_ptr<AbstractTimer>      mtimer;
        std::unique_ptr<AbstractMIDIDevice> mdev;
        NavStatus                           mnstat;
        MIDIClockTime                       mtargetTime; // navigate target (only forward and backward)
        MIDITrack::iterator                 mcurit;
        MIDIClockTime                       metctick = 0; // wait metctick to next event
        bool                                museCache = false;
        bool                                mrevertState = false;
        std::unordered_map<MIDIClockTime, MIDIState> mcache;
    };
}