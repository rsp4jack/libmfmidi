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

#include "mfmidi/mfutility.hpp"

namespace mfmidi {
    class tempo {
    public:
        constexpr tempo() noexcept = default;

        /// MSPQ: Micro Seconds Per Quarter
        constexpr static tempo from_mspq(uint32_t usec) noexcept
        {
            return tempo{usec};
        }

        template <class T>
            requires std::is_arithmetic_v<T>
        constexpr static tempo from_bpm(T bpm) noexcept
        {
            return tempo(us_per_min / bpm);
        }

        [[nodiscard]] constexpr double bpm_fp() const noexcept
        {
            if (_mspq == 0) {
                return 0;
            }
            return static_cast<double>(us_per_min) / _mspq;
        }

        [[nodiscard]] constexpr unsigned int bpm() const noexcept
        {
            if (_mspq == 0) {
                return 0;
            }
            return us_per_min / _mspq;
        }

        [[nodiscard]] constexpr uint32_t mspq() const noexcept
        {
            return _mspq;
        }

        [[nodiscard]] constexpr auto&& data(this auto&& self)
        {
            return std::forward<decltype(self)>(self)._mspq;
        }

        constexpr explicit operator bool() const noexcept
        {
            return _mspq != 0;
        }

    private:
        static constexpr unsigned int us_per_min = 60'000'000;

        explicit constexpr tempo(uint32_t raw) noexcept
            : _mspq(raw)
        {
        }

        uint32_t _mspq; // microseconds (us) per a quarter note
    };

    namespace literals {
        consteval tempo operator""_bpm(unsigned long long int bpm)
        {
            return tempo::from_bpm(bpm);
        }
    }
}
