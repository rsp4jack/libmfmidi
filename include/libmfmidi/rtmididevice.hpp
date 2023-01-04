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

/// \file rtmididevice.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief RtMidi MIDIDevice support

#pragma once

#include "rtmidi/RtMidi.h"
#include "libmfmidi/abstractmididevice.hpp"
#include <string>
#include <utility>
#include <unordered_set>
#include <memory>

namespace libmfmidi::platform {
    class RtMidiInDevice : public AbstractMIDIDevice {
    public:
        explicit RtMidiInDevice(unsigned int id, std::string name = "libmfmidi RtMidiInDevice") noexcept
            : mid(id)
            , mnm(std::move(name))
        {
            initiazle();
        }

        explicit RtMidiInDevice(std::string virtualPortName) noexcept
            : mnm(std::move(virtualPortName))
            , mvirtual(true)
        {
            initiazle();
        }

        ~RtMidiInDevice() noexcept override = default;

        MF_DEFAULT_MOVE_CTOR(RtMidiInDevice);
        MF_DISABLE_MOVE_ASGN(RtMidiInDevice);
        MF_DISABLE_COPY(RtMidiInDevice);

        bool open() override
        {
            if (mvirtual) {
                min.openVirtualPort(mnm);
            } else {
                min.openPort(mid, mnm);
            }
            return min.isPortOpen();
        }

        bool stop() override
        {
            min.closePort();
            return !min.isPortOpen();
        }

        [[nodiscard]] bool isOpen() const noexcept override
        {
            return min.isPortOpen();
        }

        [[nodiscard]] constexpr bool inputAvailable() const noexcept override
        {
            return true;
        }

        [[nodiscard]] constexpr bool outputAvailable() const noexcept override
        {
            return false;
        }

        void sendMsg(const MIDIMessage& /*msg*/) override {}

    private:
        static void rtcallback(double /*timeStamp*/, std::vector<unsigned char>* message, void* userData)
        {
            reinterpret_cast<RtMidiInDevice*>(userData)->mcb(MIDIMessage(*message));
        }

        void initiazle() noexcept
        {
            min.setCallback(rtcallback, this);
        }

        RtMidiIn     min;
        unsigned int mid{};
        std::string  mnm;
        bool         mvirtual = false;
    };

    class RtMidiOutDevice : public AbstractMIDIDevice {
    public:
        explicit RtMidiOutDevice(unsigned int id, std::string name = "libmfmidi RtMidiOutDevice") noexcept
            : mid(id)
            , mnm(std::move(name))
        {
        }

        explicit RtMidiOutDevice(std::string virtualPortName) noexcept
            : mnm(std::move(virtualPortName))
            , mvirtual(true)
        {
        }

        ~RtMidiOutDevice() noexcept override = default;
        MF_DEFAULT_MOVE_CTOR(RtMidiOutDevice);
        MF_DISABLE_MOVE_ASGN(RtMidiOutDevice);
        MF_DISABLE_COPY(RtMidiOutDevice);

        bool open() override
        {
            if (mvirtual) {
                min.openVirtualPort(mnm);
            } else {
                min.openPort(mid, mnm);
            }
            return min.isPortOpen();
        }

        bool stop() override
        {
            min.closePort();
            return !min.isPortOpen();
        }

        [[nodiscard]] bool isOpen() const noexcept override
        {
            return min.isPortOpen();
        }

        [[nodiscard]] constexpr bool inputAvailable() const noexcept override
        {
            return false;
        }

        [[nodiscard]] constexpr bool outputAvailable() const noexcept override
        {
            return true;
        }

        void sendMsg(const MIDIMessage& msg) override
        {
            min.sendMessage(&msg.data());
        }

    private:
        RtMidiOut    min;
        unsigned int mid{};
        std::string  mnm;
        bool         mvirtual = false;
    };

    class RtMidiMIDIDeviceProvider {
    public:
        unsigned int inputCount()
        {
            return auxin.getPortCount();
        }

        unsigned int outputCount()
        {
            return auxout.getPortCount();
        }

        std::string inputName(unsigned int idx)
        {
            return auxin.getPortName(idx);
        }

        std::string outputName(unsigned int idx)
        {
            return auxout.getPortName(idx);
        }

        static std::unique_ptr<RtMidiInDevice> buildupInputDevice(unsigned int idx, const std::string& name = "libmfmidi RtMidiMIDIDeviceProvider IN")
        {
            return std::make_unique<RtMidiInDevice>(idx, name);
        }

        static std::unique_ptr<RtMidiOutDevice> buildupOutputDevice(unsigned int idx, const std::string& name = "libmfmidi RtMidiMIDIDeviceProvider OUT"){
            return std::make_unique<RtMidiOutDevice>(idx, name);
        }

        static RtMidiMIDIDeviceProvider& instance() noexcept
        {
            static RtMidiMIDIDeviceProvider ins; // Meyers' Singleton
            return ins;
        }

    private:
        RtMidiMIDIDeviceProvider() noexcept = default;

        RtMidiIn  auxin;  // for device info
        RtMidiOut auxout; // too

        
    };
}
