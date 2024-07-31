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

#include <stdexcept>
#include <system_error>

namespace mfmidi {
    enum class smf_errc : unsigned char {
        error_eof,
        error_file_header,
        error_track_header,
        error_smf_type,
        error_event_type,
        error_running_status,
        error_division
    };
}

template <>
struct std::is_error_code_enum<mfmidi::smf_errc> : std::true_type {};

namespace mfmidi {
    [[nodiscard]] const std::error_category& smf_category() noexcept;

    inline std::error_code make_error_code(smf_errc errc) noexcept
    {
        return {static_cast<int>(errc), smf_category()};
    }

    struct smf_error : std::runtime_error {
        explicit smf_error(smf_errc errc);

        [[nodiscard]] const std::error_code& code() const
        {
            return _code;
        }

    private:
        std::error_code _code;
    };
}