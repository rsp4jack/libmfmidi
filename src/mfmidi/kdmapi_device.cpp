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

#include "mfmidi/device/kdmapi_device.hpp"
#include "omnimidi/OmniMIDI.h"

namespace mfmidi {
    kdmapi_device::kdmapi_device(bool force) noexcept
        : available(force)
    {
        if (IsKDMAPIAvailable() == 0) {
            // todo: handle it
            std::cerr << "Warning: IsKDMAPIAvailable returns false" << std::endl;
        } else {
            available = true;
        }
    }

    bool kdmapi_device::open()
    {
        bool result = InitializeKDMAPIStream() != 0;
        ison |= result; // if on, fail on success on; if off, fail off success on
        return result;
    }

    bool kdmapi_device::close()
    {
        bool result = TerminateKDMAPIStream() != 0;
        ison &= !result; // if on, fail on success off; if off, fail off success off
        return result;
    }

    std::expected<void, const char*> kdmapi_device::send_msg(std::span<const uint8_t> msgbuf) noexcept
    {
        if (!ison) {
            return std::unexpected{"Device is not open"};
        }
        midi_message_owning_view msg{auto{msgbuf}};
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
                return std::unexpected{"kdmapi_device: error sending sysex messsage"};
            }

            result = SendDirectLongData(&sysex, sizeof(MIDIHDR));
            if (result != MMSYSERR_NOERROR) {
                return std::unexpected{"kdmapi_device: error sending sysex messsage"};
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
                return std::unexpected("kdmapi_device: message size > 3 bytes");
            }

            DWORD packet{};
            std::copy(msg.cbegin(), msg.cend(), reinterpret_cast<uint8_t*>(&packet));

            SendDirectData(packet); // no result
        }
        return {};
    }
}
