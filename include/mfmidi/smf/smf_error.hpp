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

#include <stdexcept>

namespace mfmidi {
    struct smf_error : public std::runtime_error {
        enum class error_type : unsigned char {
            error_eof,
            error_file_header,
            error_track_header,
            error_smf_type,
            error_event_type,
            error_running_status,
            error_division
        };

        explicit smf_error(error_type type);

        [[nodiscard]] error_type code() const { return _type; }

    private:
        error_type _type;
    };
}