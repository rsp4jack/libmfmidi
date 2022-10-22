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

/// \file midimessagefdc.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDI message F2D and D2F (FDC)

#include "midimessage.hpp"

namespace libmfmidi {
    /// \brief F2D MIDIProcessor
    /// F2D: SMF message to MIDI Device message
    class MIDIMessageF2D {
    public:
        bool process(MIDITimedMessage& msg)
        {
            // TODO: fine this
            if (msg.isTempo() && msg.strictVaild()) {
                auto bpm = msg.bpm();
                msg.setupMFMarker(MFMessageMark::Tempo, static_cast<uint8_t>((bpm >> 24) & 0xFF), static_cast<uint8_t>((bpm >> 16) & 0xFF), static_cast<uint8_t>((bpm >> 8) & 0xFF), static_cast<uint8_t>(bpm & 0xFF));
                return true;
            }
            return !msg.isMetaEvent();
        }
    };

    /// \brief D2F MIDIProcessor
    /// D2F: Device message to SMF message
    class MIDIMessageD2F {
    public:
        bool process(MIDITimedMessage& msg)
        {
            return !msg.isSystemMessage();
        }
    };
}
