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

#pragma once

#include "mfmidi/midi_message.hpp"
#include <functional>
#include <tl/expected.hpp>
#include <utility>

namespace mfmidi {
    class midi_device {
    public:
        midi_device() noexcept = default;

        midi_device(midi_device&&) noexcept                 = default;
        midi_device& operator=(midi_device&&) noexcept      = default;
        midi_device(const midi_device&) noexcept            = default;
        midi_device& operator=(const midi_device&) noexcept = default;

        virtual ~midi_device() noexcept = default;

        [[nodiscard]] virtual bool isOpen() const noexcept = 0;

        [[nodiscard]] virtual constexpr bool inputAvailable() const noexcept  = 0;
        [[nodiscard]] virtual constexpr bool outputAvailable() const noexcept = 0;

        virtual bool open()  = 0;
        virtual bool close() = 0;

        virtual tl::expected<void, const char*> sendMsg(std::span<const uint8_t> msg) noexcept = 0;
    };

    inline void sendAllSoundsOff(midi_device* dev)
    {
        for (uint8_t channel : std::views::iota(0U, 16U)) {
            uint8_t msg[]{static_cast<uint8_t>(MIDIMsgStatus::CONTROL_CHANGE | channel), MIDICCNumber::ALL_SOUND_OFF, 0};
            dev->sendMsg(msg);
        }
    }
}
