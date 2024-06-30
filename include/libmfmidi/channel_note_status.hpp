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

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include "libmfmidi/midimessage.hpp"
#include "libmfmidi/midinotifier.hpp"
#include "libmfmidi/midiutils.hpp"
#include <array>
#include <bitset>
#include <cstdint>

namespace libmfmidi {
    /// \brief A matrix to keep note and pedal
    ///
    struct channel_note_status : protected NotifyUtils<channel_note_status> {
    public:
        channel_note_status() noexcept = default;

        struct key_status {
            bool    on         = false;
            uint8_t velocity   = 0; ///< note on and off velocity
            uint8_t afterTouch = 0;
        };

        template <std::ranges::range T>
        void process(T&& msg, uint8_t port = 1)
        {
            if (msg.isChannelMsg()) {
            }
            if (msg.isAllNotesOff() || msg.isAllSoundsOff()) {
                clearChannel(port, chn);
            } else if (msg.isImplicitNoteOn()) {
                noteOn(port, chn, msg.note(), msg.velocity());
            } else if (msg.isImplicitNoteOff()) {
                noteOff(port, chn, msg.note(), msg.velocity());
            } else if (msg.isPolyPressure()) {
                polyPressure(port, chn, msg.note(), msg.pressure());
            }
        }

        void clear()
        {
            _keys = {};
        }

        std::array<key_status, 128> _keys;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
