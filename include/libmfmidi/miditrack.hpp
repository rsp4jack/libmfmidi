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

#include <vector>
#include <algorithm>
#include "libmfmidi/midimessage.hpp"

#undef max

namespace libmfmidi {
    /// \brief SMF Track
    /// \note Jack use chunks to avoid memory fragmentation.
    /// \note But It is 21th. This project is not for small embedded systems.
    /// inherit from std::vector<miditimedmessage>
    class MIDITrack : public std::vector<MIDITimedMessage> {
    public:
        [[nodiscard]] constexpr bool isSorted() const
        {
            return std::is_sorted(cbegin(), cend());
        }

        void sort()
        {
            std::stable_sort(begin(), end());
        }

        constexpr void merge(const MIDITrack& lhs, const MIDITrack& rhs)
        {
            clear();
            *this = lhs;
            insert(cend(), rhs.cbegin(), rhs.cend());

            while (true) {
                int           count     = 0;
                MIDIClockTime min       = MIDICLKTM_MAX;
                size_t        min_index = 0;
                for (size_t i = 0; i < size(); ++i) {
                    if (at(i).isEndOfTrack()) {
                        ++count;
                        if (at(i).deltaTime() < min) {
                            min       = at(i).deltaTime();
                            min_index = i;
                        }
                    }
                }
                if (count > 1) {
                    erase(cbegin() + min_index);
                } else {
                    break;
                }
            }
        }

        [[nodiscard]] constexpr MIDITimedMessage& lastTimeEvent()
        {
            return *std::max_element(begin(), end());
        }

        [[nodiscard]] constexpr const MIDITimedMessage& lastTimeEvent() const
        {
            return *std::max_element(cbegin(), cend());
        }

        [[nodiscard]] constexpr MIDIClockTime lastTimeEventTime() const
        {
            return lastTimeEvent().deltaTime();
        }

        [[nodiscard]] constexpr bool haveEOT() const
        {
            return std::any_of(cbegin(), cend(), [](const MIDITimedMessage& msg) {
                return msg.isEndOfTrack();
            });
        }
    };

    constexpr void toAbsTimeTrack(MIDITrack& trk)
    {
        MIDIClockTime time = 0;
        for (auto& event : trk) {
            event.setDeltaTime(time += event.deltaTime());
        }
    }

    constexpr void toRelTimeTrack(MIDITrack& trk)
    {
        MIDIClockTime time = 0;
        for (auto& event : trk) {
            MIDIClockTime tmp = event.deltaTime();
            event.setDeltaTime(tmp - time);
            time = tmp;
        }
    }
}
