/*
 * This file is a part of mfmidi.
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

/// \file midi_status.hpp
/// \brief MIDIStatus

#pragma once

#include <array>
#include <concepts>
#include <optional>

namespace mfmidi {
    struct channel_voice_control_status {
        std::optional<uint8_t> program;    ///< Current Program Change (it will never bigger than uint8_t or smaller than -1)
        std::optional<uint8_t> aftertouch; // = 0
        std::optional<int16_t> pitchbend;  // = 0

        // TODO: Control Change map for every CC
        std::array<std::optional<uint8_t>, 120> controllers;
    };

    struct channel_voice_key_status {
        struct key_status {
            bool    on         = false;
            uint8_t velocity   = 0; ///< note on and off velocity
            uint8_t afterTouch = 0;
        };
        std::array<key_status, 128> keys;
    };

    struct channel_voice_status : channel_voice_control_status
        , channel_voice_key_status {};

    struct channel_mode_status {
        bool omni  : 1;
        bool poly  : 1;
        bool local : 1;
    };

    struct system_common_status {
        // todo: implement that
    };

    // todo: shared_ptr manage status processor

    template <class T>
    struct status_traits { // std::is_pointer_interconvertible_base_of
        constexpr static bool is_channel_voice_control_status = std::convertible_to<T&, channel_voice_control_status&>;
        constexpr static bool is_channel_voice_key_status     = std::convertible_to<T&, channel_voice_key_status&>;
        constexpr static bool is_channel_mode_status          = std::convertible_to<T&, channel_mode_status>;
    };
}
