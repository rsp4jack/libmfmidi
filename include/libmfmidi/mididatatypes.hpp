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
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Standard MIDI Data Types

#pragma once

#include "libmfmidi/mfutils.hpp"
#include <type_traits>
#include "preshing/bitfields.hpp"

namespace libmfmidi {
    class MIDITempo final {
    public:
        constexpr MIDITempo() noexcept = default;

        /// MSPQ: MicroSecPerQuarter
        constexpr static MIDITempo fromMSPQ(uint32_t usec) noexcept
        {
            return MIDITempo{usec};
        }

        template <class T>
            requires std::is_arithmetic_v<T>
        constexpr static MIDITempo fromBPM(T bpm) noexcept
        {
            return MIDITempo{uspermin / bpm};
        }

        [[nodiscard]] constexpr double bpmFP() const noexcept
        {
            if (mtempo == 0) {
                return 0;
            }
            return static_cast<double>(uspermin) / mtempo;
        }

        [[nodiscard]] constexpr unsigned int bpm() const noexcept
        {
            if (mtempo == 0) {
                return 0;
            }
            return uspermin / mtempo;
        }

        [[nodiscard]] constexpr uint32_t mspq() const noexcept
        {
            return mtempo;
        }

        explicit constexpr operator bool() const noexcept
        {
            return mtempo != 0;
        }

    private:
        static constexpr unsigned int uspermin = 60'000'000;

        explicit constexpr MIDITempo(uint32_t raw) noexcept
            : mtempo(raw)
        {
        }

        uint32_t mtempo; // microseconds (us) per a quarter note
    };

    static_assert(sizeof(MIDITempo) == 4);

    /// \brief SMF Division
    /// TPF: Ticks per frame
    /// You can directly \c reinterpret_cast a 16-bit integer to this class
    class MIDIDivision final {
    public:
        constexpr MIDIDivision() noexcept = default;

        constexpr explicit MIDIDivision(uint16_t val) noexcept
            : mval{val}
        {
        }

        /// \param fps Positive FPS like 25, 29(means 29.97)...
        constexpr MIDIDivision(uint8_t fps, uint8_t tpf) noexcept
            : mval((-fps << 8) & tpf)
        {
        }

        [[nodiscard]] constexpr bool isSMPTE() const noexcept
        {
            return (mval >> 15 & 1U) != 0U;
        }

        [[nodiscard]] constexpr bool isPPQ() const noexcept
        {
            return !isSMPTE();
        }

        [[nodiscard]] constexpr uint8_t tpf() const noexcept
        {
            return mval & 0xFF;
        }

        [[nodiscard]] constexpr uint16_t ppq() const noexcept
        {
            return mval;
        }

        [[nodiscard]] constexpr uint8_t fps() const noexcept
        {
            return -(mval >> 8U);
        }

        constexpr void setPPQ(uint16_t ppq) noexcept
        {
            mval = ppq;
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

        constexpr operator uint16_t() const noexcept
        {
            return mval;
        }

        constexpr operator bool() const noexcept
        {
            if (isPPQ()) {
                return mval != 0;
            }
            return mval != 0 && fps() != 0 && tpf() != 0;
        }

    private:
        uint16_t mval;
    };

    static_assert(sizeof(MIDIDivision) == 2);
    static_assert(static_cast<MIDIDivision>(0xE250).fps() == 30);
    static_assert(static_cast<MIDIDivision>(0xE250).tpf() == 80);
}
