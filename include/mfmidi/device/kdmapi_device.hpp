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

#if !defined(_WIN32)
#warning "kdmapi_device on not win32"
#else

#include "mfmidi/midi_device.hpp"
#include "mfmidi/midi_message.hpp"

#include <expected>

namespace mfmidi {
    class kdmapi_device final : public midi_device {
    public:
        explicit kdmapi_device(bool force = false) noexcept;

        ~kdmapi_device() noexcept override
        {
            kdmapi_device::close();
        }

        kdmapi_device(const kdmapi_device&) noexcept            = delete;
        kdmapi_device& operator=(const kdmapi_device&) noexcept = delete;
        kdmapi_device(kdmapi_device&&) noexcept                 = default;
        kdmapi_device& operator=(kdmapi_device&&) noexcept      = default;

        bool open() override;
        bool close() override;

        [[nodiscard]] bool is_open() const noexcept override
        {
            return ison;
        }

        [[nodiscard]] constexpr bool input_available() const noexcept override
        {
            return false;
        }

        [[nodiscard]] constexpr bool output_available() const noexcept override
        {
            return available;
        }

        std::expected<void, const char*> send_msg(std::span<const uint8_t> msgbuf) noexcept override;

    private:
        bool ison = false;
        bool available;
    };

}

#endif