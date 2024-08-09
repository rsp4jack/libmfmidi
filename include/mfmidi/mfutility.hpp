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

#include <bit>
#include <cassert>
#include <cctype>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>

namespace mfmidi {
    using namespace std::literals;
    using std::int16_t;
    using std::int32_t;
    using std::int64_t;
    using std::int8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::uint8_t;
}

namespace mfmidi {
    template <std::integral T>
    constexpr T byteswapbe(T val) noexcept
    {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }

    /// Return a storage type that can storage T byte(s).
    template <size_t T>
    using typein = decltype([]<size_t N>() {
        if constexpr (N <= 1) {
            return uint8_t{};
        } else if constexpr (N <= 2) {
            return uint16_t{};
        } else if constexpr (N <= 4) {
            return uint32_t{};
        } else {
            static_assert(N <= 8);
            return uint64_t{};
        }
    }.template operator()<T>());

    /// \brief A function to connect bytes,
    /// \code {.cpp}
    /// rawcat(0xFF, 0xFA) -> 0xFFFA
    /// \endcode
    template <class... T>
    constexpr auto rawcat(uint8_t fbyte, T... bytes)
    {
        // clang-format off
        typein<sizeof...(bytes) + 1> result =
            static_cast<decltype(result)>( // mute warning
                static_cast<decltype(result)>(fbyte) << (sizeof...(bytes) * 8) // integral promotion
            );
        // clang-format on
        if constexpr (sizeof...(bytes) > 0) {
            result |= rawcat(bytes...);
        }
        return result;
    }

    constexpr std::string dump_span(const void* memory, std::size_t size)
    {
        std::string result;
        result.reserve(size * 2 - 1);
        char buffer[2]{};
        bool ins = false;
        for (const auto& dat : std::span<const uint8_t>(static_cast<const uint8_t*>(memory), size)) {
            buffer[0] = 0;
            buffer[1] = 0;

            auto fmt  = std::to_chars(buffer, buffer + 2, static_cast<uint8_t>(dat), 16);
            buffer[0] = std::toupper(buffer[0]);
            buffer[1] = std::toupper(buffer[1]);

            if (fmt.ptr == &buffer[1]) {
                buffer[1] = buffer[0];
                buffer[0] = '0';
            }

            if (ins) {
                result.push_back(' ');
            } else {
                ins = true;
            }
            result.insert(result.end(), buffer, buffer + 2);
        }
        return result;
    }
}