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
#include "mfmidi/midi_tempo.hpp"

namespace mfmidi {
    class division {
    public:
        constexpr division() noexcept = default;

        constexpr explicit division(uint16_t val) noexcept
            : _val{val}
        {
        }

        /// \param fps Positive FPS like 25, 29(means 29.97)...
        constexpr division(uint8_t fps, uint8_t tpf) noexcept
            : _val((-fps << 8) & tpf)
        {
        }

        [[nodiscard]] constexpr bool is_smpte() const noexcept
        {
            return (_val >> 15 & 1U) != 0U;
        }

        [[nodiscard]] constexpr bool is_ppq() const noexcept
        {
            return !is_smpte();
        }

        [[nodiscard]] constexpr uint8_t tpf() const noexcept
        {
            return _val & 0xFF;
        }

        [[nodiscard]] constexpr uint16_t ppq() const noexcept
        {
            return _val;
        }

        [[nodiscard]] constexpr uint8_t fps() const noexcept
        {
            return -(_val >> 8U);
        }

        constexpr void set_ppq(uint16_t ppq) noexcept
        {
            _val = ppq;
        }

        /// \param fps Positive FPS like 25, 29(means 29.97)...
        constexpr void set_fps(uint8_t fps) noexcept
        {
            *this = division(fps, tpf());
        }

        constexpr void set_tpf(uint8_t tpf) noexcept
        {
            *this = division(fps(), tpf);
        }

        [[nodiscard]] constexpr auto&& data(this auto&& self)
        {
            return std::forward<decltype(self)>(self)._val;
        }

        constexpr explicit operator uint16_t() const noexcept
        {
            return _val;
        }

        constexpr explicit operator bool() const noexcept
        {
            if (is_ppq()) {
                return _val != 0;
            }
            return _val != 0 && fps() != 0 && tpf() != 0;
        }

    private:
        uint16_t _val;
    };

    static_assert(std::is_standard_layout_v<division>);

    inline namespace literals {
        consteval division operator""_ppq(unsigned long long int ppq)
        {
            return division(ppq);
        }
    }

    constexpr std::chrono::nanoseconds division_to_duration(division val, tempo bpm) noexcept
    {
        using result_type = std::chrono::nanoseconds;
        if (!val || !bpm) {
            return result_type{};
        }
        if (val.is_ppq()) {
            // MIDI always 4/4
            return result_type{static_cast<result_type::rep>(60.0 / (val.ppq() * bpm.bpm_fp()) * 1'000'000'000)};
        }
        // 24 25 29(.97) 30
        double realfps = val.fps();
        if (val.fps() == 29) {
            realfps = 29.97;
        }
        return result_type{static_cast<result_type::rep>(1 / (realfps * val.tpf()) * 1'000'000'000)};
    }
}