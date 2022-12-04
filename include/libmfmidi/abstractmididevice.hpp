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

/// \file abstractmididevice.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIDriver

#pragma once

#include "libmfmidi/midimessage.hpp"
#include <functional>
#include <utility>

namespace libmfmidi {
    /// \brief A class that abstracted MIDI device.
    class AbstractMIDIDevice {
    public:
        AbstractMIDIDevice() noexcept = default;

        AbstractMIDIDevice(const AbstractMIDIDevice&)            = default;
        AbstractMIDIDevice(AbstractMIDIDevice&&)                 = default;
        AbstractMIDIDevice& operator=(const AbstractMIDIDevice&) = default;
        AbstractMIDIDevice& operator=(AbstractMIDIDevice&&)      = default;
        using callback_type                                      = std::function<void(const MIDIMessage& msg)>;

        virtual ~AbstractMIDIDevice() noexcept = default;

        [[nodiscard]] virtual bool isOpen() const noexcept = 0;

        [[nodiscard]] virtual constexpr bool inputAvailable() const noexcept = 0;
        [[nodiscard]] virtual constexpr bool outputAvailable() const noexcept = 0;

        virtual bool open() = 0;
        virtual bool stop() = 0;

        virtual void sendMsg(const MIDIMessage& msg) = 0;

        virtual void setCallback(callback_type usercb) noexcept
        {
            mcb = std::move(usercb);
        }

    protected:
        callback_type mcb;

    };

    inline void sendAllSoundsOff(AbstractMIDIDevice* dev)
    {
        dev->sendMsg({MIDIMsgStatus::CONTROL_CHANGE, MIDICCNumber::ALL_SOUND_OFF, 0});
    }
}
