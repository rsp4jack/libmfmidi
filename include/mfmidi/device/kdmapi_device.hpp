/*
 * This file is a part of mfmidi.
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

#if !defined(_WIN32)
#warning "kdmapi_device on not win32"
#else

#include "mfmidi/midi_device.hpp"
#include "mfmidi/midi_message.hpp"
#include "omnimidi/OmniMIDI.h"

namespace mfmidi {
    class kdmapi_device final : public midi_device {
    public:
        explicit kdmapi_device(bool force = false) noexcept
            : available(force)
        {
            if (IsKDMAPIAvailable() == 0) {
                // todo: handle it
                std::cerr << "Warning: IsKDMAPIAvailable returns false" << std::endl;
            } else {
                available = true;
            }
        }

        ~kdmapi_device() noexcept override
        {
            kdmapi_device::close();
        }

        kdmapi_device(const kdmapi_device&) noexcept            = delete;
        kdmapi_device& operator=(const kdmapi_device&) noexcept = delete;
        kdmapi_device(kdmapi_device&&) noexcept                 = default;
        kdmapi_device& operator=(kdmapi_device&&) noexcept      = default;

        bool open() override
        {
            bool result = InitializeKDMAPIStream() != 0;
            ison |= result; // if on, fail on success on; if off, fail off success on
            return result;
        }

        bool close() override
        {
            bool result = TerminateKDMAPIStream() != 0;
            ison &= !result; // if on, fail on success off; if off, fail off success off
            return result;
        }

        [[nodiscard]] bool isOpen() const noexcept override
        {
            return ison;
        }

        [[nodiscard]] constexpr bool inputAvailable() const noexcept override
        {
            return false;
        }

        [[nodiscard]] constexpr bool outputAvailable() const noexcept override
        {
            return available;
        }

        tl::expected<void, const char*> sendMsg(std::span<const uint8_t> msgbuf) noexcept override
        {
            if (!ison) {
                return tl::unexpected{"Device is not open"};
            }
            midi_message_owning_view<std::span<const uint8_t>> msg{auto{msgbuf}};
            if (msg.empty()) {
                return {};
            }
            UINT result;

            if (msg.isSysEx()) {
                // prepare sysex header
                std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(msg.size());
                std::copy(msg.cbegin(), msg.cend(), buffer.get()); // copy because it need not const
                MIDIHDR sysex{};                                   // initiazle to avoid exception
                sysex.lpData         = reinterpret_cast<LPSTR>(buffer.get());
                sysex.dwBufferLength = static_cast<DWORD>(msg.size());
                sysex.dwFlags        = 0;
                result               = PrepareLongData(&sysex, sizeof(MIDIHDR));
                if (result != MMSYSERR_NOERROR) {
                    std::cout << "Error: kdmapi_device: error preparing sysex header" << '\n';
                }

                result = SendDirectLongData(&sysex, sizeof(MIDIHDR));
                if (result != MMSYSERR_NOERROR) {
                    return tl::unexpected{"kdmapi_device: error sending sysex messsage"};
                }

                while (UnprepareLongData(&sysex, sizeof(MIDIHDR)) == MIDIERR_STILLPLAYING) {
                    Sleep(1);
                }
            } else {
                if (msg.size() == sizeof(DWORD)) {
                    SendDirectData(*reinterpret_cast<const DWORD*>(msg.base().data()));
                    return {};
                }

                if (msg.size() > 3) {
                    return tl::unexpected("kdmapi_device: message size > 3 bytes");
                }

                DWORD packet{};
                std::copy(msg.cbegin(), msg.cend(), reinterpret_cast<uint8_t*>(&packet));

                SendDirectData(packet); // no result
            }
            return {};
        }

    private:
        bool ison = false;
        bool available;
    };

}

#endif