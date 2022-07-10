/// \file miditrack.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMF Track

#pragma once

#include <vector>
#include <algorithm>
#include "midimessage.hpp"

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

        constexpr void sort()
        {
            std::stable_sort(begin(), end());
        }

        constexpr void merge(const MIDITrack& lhs, const MIDITrack& rhs)
        {
            clear();
            *this = lhs;
            insert(cend(), rhs.cbegin(), rhs.cend());

            while (true) {
                int           count = 0;
                MIDIClockTime min   = std::numeric_limits<MIDIClockTime>::max();
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
}
