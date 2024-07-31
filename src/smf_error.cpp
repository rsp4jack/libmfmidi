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

#include "mfmidi/smf/smf_error.hpp"
#include <string_view>
#include <utility>

namespace mfmidi {
    namespace {
        std::string_view description(smf_errc type)
        {
            using enum smf_errc;
            switch (type) {
            case error_eof:
                return "Unexpected EOF";
            case error_file_header:
                return "Invalid MThd header";
            case error_track_header:
                return "Invalid Mtrk header";
            case error_smf_type:
                return "Invalid SMF type";
            case error_event_type:
                return "Invalid event type";
            case error_running_status:
                return "Running status without status";
            case error_division:
                return "Invalid division";
            }
            std::unreachable();
        }
    }

    class _smf_category : public std::error_category {
    public:
        [[nodiscard]] const char* name() const noexcept override
        {
            return "smf";
        }

        [[nodiscard]] std::string message(int condition) const override
        {
            return std::string{description(static_cast<smf_errc>(condition))};
        }
    };

    const std::error_category& smf_category() noexcept
    {
        static _smf_category category;
        return category;
    }

    smf_error::smf_error(smf_errc errc)
        : std::runtime_error{description(errc).data()}
        , _code{errc}
    {
    }

}