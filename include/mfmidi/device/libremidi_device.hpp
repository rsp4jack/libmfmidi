/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
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

#include "libremidi/libremidi.hpp"
#include "mfmidi/midi_device.hpp"
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace mfmidi {
#pragma region rtmidi
    /* class RtMidiInDevice : public midi_device {
    public:
        explicit RtMidiInDevice(unsigned int id, std::string name = "mfmidi RtMidiInDevice") noexcept
            : mid(id)
            , mnm(std::move(name))
        {
            initialize();
        }

        explicit RtMidiInDevice(std::string virtualPortName) noexcept
            : mnm(std::move(virtualPortName))
            , mvirtual(true)
        {
            initialize();
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

        bool close() override
        {
            min.closePort();
            return !min.isPortOpen();
        }

        [[nodiscard]] bool is_open() const noexcept override
        {
            return min.isPortOpen();
        }

        [[nodiscard]] constexpr bool input_available() const noexcept override
        {
            return true;
        }

        [[nodiscard]] constexpr bool output_available() const noexcept override
        {
            return false;
        }

        tl::expected<void, const char*> send_msg(const MIDIMessage& msg) noexcept override
        {
            return tl::unexpected("Output unavailable");
        }

    private:
        static void rtcallback(double timeStamp, std::vector<unsigned char>* message, void* userData)
        {
            reinterpret_cast<RtMidiInDevice*>(userData)->mcb(MIDIMessage(*message));
        }

        void initialize() noexcept
        {
            min.setCallback(rtcallback, this);
        }

        RtMidiIn     min;
        unsigned int mid{};
        std::string  mnm;
        bool         mvirtual = false;
    }; */
#pragma endregion

    class libremidi_out_device : public midi_device {
    public:
        explicit libremidi_out_device(libremidi::output_port id, std::string name = "mfmidi RtMidiOutDevice") noexcept
            : mid(std::move(id))
            , mnm(std::move(name))
        {
        }

        explicit libremidi_out_device(std::string virtualPortName) noexcept
            : mnm(std::move(virtualPortName))
            , mvirtual(true)
        {
        }

        ~libremidi_out_device() noexcept override = default;

        libremidi_out_device(libremidi_out_device&&) noexcept                 = default;
        libremidi_out_device& operator=(libremidi_out_device&&) noexcept      = delete;
        libremidi_out_device(const libremidi_out_device&) noexcept            = delete;
        libremidi_out_device& operator=(const libremidi_out_device&) noexcept = delete;

        bool open() override
        {
            if (mvirtual) {
                min.open_virtual_port(mnm);
            } else {
                min.open_port(mid, mnm);
            }
            return min.is_port_open();
        }

        bool close() override
        {
            min.close_port();
            return !min.is_port_open();
        }

        [[nodiscard]] bool is_open() const noexcept override
        {
            return min.is_port_open();
        }

        [[nodiscard]] constexpr bool input_available() const noexcept override
        {
            return false;
        }

        [[nodiscard]] constexpr bool output_available() const noexcept override
        {
            return true;
        }

        std::expected<void, const char*> send_msg(std::span<const uint8_t> msg) noexcept override
        {
            try {
                min.send_message(msg);
            } catch (std::exception& err) {
                return std::unexpected{err.what()};
            }

            return {};
        }

    private:
        libremidi::midi_out    min;
        libremidi::output_port mid{};
        std::string            mnm;
        bool                   mvirtual = false;
    };
}
