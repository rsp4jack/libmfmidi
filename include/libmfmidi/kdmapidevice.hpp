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

/// \file kdmapidevice.hpp
/// \brief KDMAPI MIDIDevice

#ifdef _WIN32

#include "libmfmidi/abstractmididevice.hpp" // "max" macro
#include "omnimidi/OmniMIDI.h"

namespace libmfmidi::platform {
    class KDMAPIDevice : public AbstractMIDIDevice {
    public:
        explicit KDMAPIDevice(bool force = false) noexcept
            : available(force)
        {
            if (IsKDMAPIAvailable() == 0) {
                std::cerr << "Warning: IsKDMAPIAvailable returns false" << std::endl;
            } else {
                available = true;
            }
        }

        ~KDMAPIDevice() noexcept override
        {
            close();
        }

        MF_DISABLE_COPY(KDMAPIDevice);
        MF_DEFAULT_MOVE(KDMAPIDevice);

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

        tl::expected<void, const char*> sendMsg(const MIDIMessage& msg) noexcept override 
        {
            if (!ison) {
                return tl::unexpected{"Device is not open"};
            }
            if (msg.empty()) {
                return {};
            }
            UINT result;
            if (msg.isSysEx()) {
                // prepare sysex header
                std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(msg.size());
                std::copy(msg.cbegin(), msg.cend(), buffer.get()); // copy because it need not const
                MIDIHDR sysex{}; // initiazle to avoid exception
                sysex.lpData = reinterpret_cast<LPSTR>(buffer.get());
                sysex.dwBufferLength = static_cast<DWORD>(msg.size());
                sysex.dwFlags        = 0;
                result               = PrepareLongData(&sysex, sizeof(MIDIHDR));
                if (result != MMSYSERR_NOERROR) {
                    std::cout << "Error: KDMAPIDevice: error preparing sysex header" << '\n';
                }

                result = SendDirectLongData(&sysex, sizeof(MIDIHDR));
                if (result != MMSYSERR_NOERROR) {
                    return tl::unexpected{"KDMAPIDevice: error sending sysex messsage"};
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
                    return tl::unexpected("KDMAPIDevice: message size > 3 bytes");
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