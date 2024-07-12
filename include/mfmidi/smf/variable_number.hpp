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

#include "mfmidi/mfutility.hpp"

#include <bit>
#include <cassert>
#include <climits>
#include <ranges>

namespace mfmidi {
    template <std::unsigned_integral T, std::regular V>
        requires(sizeof(V) * CHAR_BIT == 8)
    class smf_variable_length_number_view : public std::ranges::view_interface<smf_variable_length_number_view<T, V>> {
        T _data;

        class iterator {
            friend smf_variable_length_number_view;
            T      _data{};
            size_t _representation_bytes{};

        protected:
            constexpr explicit iterator(T data)
                : _data(data)
                , _representation_bytes((std::bit_width(std::max(_data, T{1U})) + 6) / 7)
            {
            }

        public:
            using value_type = V;

            constexpr iterator() = default;

            constexpr V operator*() const
            {
                return static_cast<V>((_data >> (_representation_bytes - 1) * 7) | (static_cast<unsigned>(_representation_bytes > 1) << 7U));
            }

            constexpr iterator& operator++()
            {
                assert(_representation_bytes > 0);
                --_representation_bytes;
                _data &= (1U << (_representation_bytes * 7 + 1)) - 1;
                return *this;
            }

            constexpr iterator operator++(int)
            {
                iterator orig = *this;
                ++(*this);
                return orig;
            }

            constexpr friend bool operator==(const iterator& lhs, std::default_sentinel_t /*unused*/)
            {
                return lhs._representation_bytes == 0;
            }

            constexpr friend std::ptrdiff_t operator-(const iterator& lhs, const iterator& rhs)
            {
                return rhs._representation_bytes - lhs._representation_bytes;
            }

            constexpr friend std::ptrdiff_t operator-(const iterator& lhs, std::default_sentinel_t /*unused*/)
            {
                return -lhs._representation_bytes;
            }
        };

    public:
        constexpr explicit smf_variable_length_number_view(T data)
            : _data(data)
        {
        }

        [[nodiscard]] constexpr auto begin() const
        {
            return iterator{_data};
        }

        [[nodiscard]] constexpr auto end() const
        {
            return std::default_sentinel;
        }

        [[nodiscard]] constexpr size_t size() const
        {
            return (std::bit_width(std::max(_data, 1)) + 6) / 7;
        }
    };

    template <class It>
    struct _read_smf_variable_length_number_result {
        uint32_t     result;
        It           it;
        uint_fast8_t size;
    };

    template <std::ranges::input_range R, class Proj = std::identity>
        requires(sizeof(std::iter_value_t<std::projected<std::ranges::iterator_t<R>, Proj>>) * CHAR_BIT == 8)
    constexpr _read_smf_variable_length_number_result<std::ranges::borrowed_iterator_t<R>> read_smf_variable_length_number(R&& r, Proj proj = {})
    {
        uint32_t     result{};
        auto         it  = proj(std::ranges::begin(r));
        auto         end = proj(std::ranges::end(r));
        uint_fast8_t sz{};

        for (; it != end; ++it) {
            std::uint8_t val = *it;
            ++sz;
            result |= val & 0x7Fu;
            if ((val & 0x80u) == 0) {
                ++it;
                return _read_smf_variable_length_number_result<std::ranges::iterator_t<R>>{result, std::move(it), sz};
            }
            if (sz == 3) {
                throw std::range_error{"read_smf_variable_length_number: overflow in 28 bits"};
            }
            result <<= 7;
        }
        throw std::domain_error{"read_smf_variable_lengrh_number: early END"};
    }

    template <std::output_iterator<uint8_t> OutIt>
    constexpr size_t writeVarNumIt(uint32_t data, OutIt iter)
    {
        uint32_t buf;
        size_t   cnt = 0;
        buf          = data & 0x7F;
        while ((data >>= 7) > 0) {
            buf <<= 8;
            buf |= 0x80;
            buf += data & 0x7F;
        }
        while (true) {
            *iter = buf & 0xFF;
            ++iter;
            ++cnt;
            if ((buf & 0x80) != 0U) {
                buf >>= 8;
            } else {
                break;
            }
        }
        return cnt;
    }
}

template <class T, class V>
constexpr bool std::ranges::enable_borrowed_range<mfmidi::smf_variable_length_number_view<T, V>> = true;
