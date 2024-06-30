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

#pragma once

#include "midiutils.hpp"

#include <bit>
#include <climits>
#include <concepts>
#include <cstddef>
#include <ranges>

namespace libmfmidi {
    template <std::unsigned_integral T, std::regular V>
        requires(sizeof(V) * CHAR_BIT == 8)
    class smf_variable_length_number_view {
        T _data;

        class iterator {
            friend smf_variable_length_number_view;
            T      _data;
            size_t _representation_bytes;

        protected:
            iterator(T data)
                : _data(data)
            {
                _representation_bytes = (std::bit_width(_data) + 6) / 7;
            }

        public:
            iterator() = default;

            V operator*() const
            {
                if (std::bit_width(_data) >)
            }
        };

    public:
        smf_variable_length_number_view(T data)
            : _data(data)
        {
        }
    };
}
