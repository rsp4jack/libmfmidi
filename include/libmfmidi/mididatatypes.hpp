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

/// \file mididatatypes.hpp
/// \brief Standard MIDI Data Types

#pragma once

#include "libmfmidi/mfutils.hpp"
#include <type_traits>

namespace libmfmidi {
    class MIDITempo {
    public:
        constexpr MIDITempo() noexcept = default;

        /// MSPQ: Micro Seconds Per Quarter
        constexpr static MIDITempo fromMSPQ(uint32_t usec) noexcept
        {
            return MIDITempo{usec};
        }

        template <class T>
            requires std::is_arithmetic_v<T>
        constexpr static MIDITempo fromBPM(T bpm) noexcept
        {
            return MIDITempo{us_per_min / bpm};
        }

        [[nodiscard]] constexpr double bpmFP() const noexcept
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

        explicit constexpr MIDITempo(uint32_t raw) noexcept
            : _mspq(raw)
        {
        }

        uint32_t _mspq; // microseconds (us) per a quarter note
    };

    static_assert(sizeof(MIDITempo) == 4);

    /// \brief SMF Division
    /// TPF: Ticks per frame
    /// You can directly \c reinterpret_cast a 16-bit integer to this class
    class MIDIDivision {
    public:
        constexpr MIDIDivision() noexcept = default;

        constexpr explicit MIDIDivision(uint16_t val) noexcept
            : _val{val}
        {
        }

        /// \param fps Positive FPS like 25, 29(means 29.97)...
        constexpr MIDIDivision(uint8_t fps, uint8_t tpf) noexcept
            : _val((-fps << 8) & tpf)
        {
        }

        [[nodiscard]] constexpr bool isSMPTE() const noexcept
        {
            return (_val >> 15 & 1U) != 0U;
        }

        [[nodiscard]] constexpr bool isPPQ() const noexcept
        {
            return !isSMPTE();
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

        constexpr void setPPQ(uint16_t ppq) noexcept
        {
            _val = ppq;
        }

        /// \param fps Positive FPS like 25, 29(means 29.97)...
        constexpr void setFPS(uint8_t fps) noexcept
        {
            *this = MIDIDivision(fps, tpf());
        }

        constexpr void setTPF(uint8_t tpf) noexcept
        {
            *this = MIDIDivision(fps(), tpf);
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
            if (isPPQ()) {
                return _val != 0;
            }
            return _val != 0 && fps() != 0 && tpf() != 0;
        }

    private:
        uint16_t _val;
    };

    static_assert(sizeof(MIDIDivision) == 2);
    static_assert(static_cast<MIDIDivision>(0xE250).fps() == 30);
    static_assert(static_cast<MIDIDivision>(0xE250).tpf() == 80);
}
