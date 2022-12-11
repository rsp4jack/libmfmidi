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
#include "preshing/bitfield.hpp"

namespace libmfmidi {
    class MIDITempo final {
    public:
        MIDITempo() noexcept = default;

        static MIDITempo fromMicroSecPerQuarter(uint32_t usec) noexcept
        {
            return MIDITempo{usec};
        }

        template <class T>
            requires std::is_arithmetic_v<T>
        static MIDITempo fromBPM(T bpm) noexcept
        {
            return MIDITempo{uspermin / bpm};
        }

        [[nodiscard]] double bpmFP() const noexcept
        {
            if (mtempo == 0) {
                return 0;
            }
            return static_cast<double>(uspermin) / mtempo;
        }

        [[nodiscard]] unsigned int bpm() const noexcept
        {
            if (mtempo == 0) {
                return 0;
            }
            return uspermin / mtempo;
        }

    private:
        static constexpr unsigned int uspermin = 60'000'000;

        explicit MIDITempo(uint32_t raw) noexcept
            : mtempo(raw)
        {
        }

        uint32_t mtempo = 0; // microseconds (us) per a quarter note
    };

    static_assert(sizeof(MIDITempo) == 4);

    class MIDIDivision final {
    public:
        MIDIDivision() noexcept = default;

        MIDIDivision(int16_t val) noexcept
            : mval{.value=val}
        {
        }

        MIDIDivision(uint8_t fps, uint8_t ppf) noexcept
            : mval{.ppf = {ppf}, .fps = {fps}, .flag = {1}}
        {
        }

        /// You can directly \c reinterpret_cast
        /// 
        union MIDIDivisionData {
            int16_t value;
            using StorageType = int16_t;
            BitFieldMember<StorageType, 0, 8>  ppf;
            BitFieldMember<StorageType, 8, 7>  fps;
            BitFieldMember<StorageType, 15, 1> flag;
        };

    private:
        MIDIDivisionData mval;
    };

    static_assert(sizeof(MIDIDivision) == 2);
}
