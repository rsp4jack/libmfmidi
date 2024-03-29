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

#ifdef _MSC_VER
#pragma warning(push) 
#pragma warning(disable: 4244)
#endif

#include "libmfmidi/midimessage.hpp"
#include "libmfmidi/midiutils.hpp"
#include <array>
#include <bitset>
#include <cstdint>
#include "libmfmidi/midinotifier.hpp"

namespace libmfmidi {
    /// \brief A matrix to keep note and pedal
    ///
    class MIDIMatrix : protected NotifyUtils<MIDIMatrix> {
    public:
        MIDIMatrix() noexcept = default;

        struct NoteState {
            bool on = false;
            uint8_t velocity = 0; ///< note on and off velocity
            uint8_t afterTouch = 0;
        };

        using NotifyUtils::addNotifier;
        friend class NotifyUtils;

        template<class T>
        bool process(const MIDIBasicMessage<T>& msg, uint8_t port = 1)
        {
            if (msg.isChannelMsg()) {
                auto chn  = msg.channel();

                if (msg.isAllNotesOff() || msg.isAllSoundsOff()) {
                    clearChannel(port, chn);
                    notify(NotifyType::TR_All);
                } else if (msg.isImplicitNoteOn()) {
                    noteOn(port, chn, msg.note(), msg.velocity());
                    notify(NotifyType::TR_Note);
                } else if (msg.isImplicitNoteOff()) {
                    noteOff(port, chn, msg.note(), msg.velocity());
                    notify(NotifyType::TR_Note);
                } else if (msg.isCCSustainOn()) {
                    holdOn(port, chn);
                    notify(NotifyType::TR_CC);
                } else if (msg.isCCSustainOff()) {
                    holdOff(port, chn);
                    notify(NotifyType::TR_CC);
                } else if (msg.isPolyPressure()) {
                    polyPressure(port, chn, msg.note(), msg.pressure());
                    notify(NotifyType::TR_AfterTouch);
                } else if (msg.isChannelPressure()) {
                    channelPressure(port, chn, msg.pressure());
                    notify(NotifyType::TR_AfterTouch);
                }
            }
            return true;
        }

        void clear()
        {
            notes = {};
        }

        [[nodiscard]] int totalNoteCount() const
        {
            int result = 0;
            for (const auto& ar : notes) {
                for (const auto& bs : ar) {
                    result += bs.count();
                }
            }
            return result;
        }

        [[nodiscard]] int portNoteCount(uint8_t port) const
        {
            int result = 0;
            for (const auto& bs : notes.at(port - 1)) {
                result += bs.count();
            }
            return result;
        }

        [[nodiscard]] int channelNoteCount(uint8_t port, uint8_t channel) const
        {
            return notes.at(port - 1).at(channel - 1).count();
        }

        [[nodiscard]] bool isNoteOn(uint8_t port, uint8_t channel, uint8_t note) const
        {
            return notes.at(port - 1).at(channel - 1).test(note);
        }

        [[nodiscard]] bool isHold(uint8_t port, uint8_t channel) const
        {
            return pedals.at(port - 1).test(channel - 1);
        }

        [[nodiscard]] int channelNoteCount(uint8_t channel) const
        {
            return notes[0].at(channel - 1).count();
        }

        [[nodiscard]] bool isNoteOn(uint8_t channel, uint8_t note) const
        {
            return notes[0].at(channel - 1).test(note);
        }

        [[nodiscard]] bool isHold(uint8_t channel) const
        {
            return pedals[0].test(channel - 1);
        }

        [[nodiscard]] NoteState noteState(uint8_t port, uint8_t channel, uint8_t note)
        {
            return notes.at(port-1).at(channel-1).at(note);
        }

        void noteOn(uint8_t port, uint8_t channel, uint8_t note, uint8_t velocity)
        {
            notes.at(port - 1).at(channel - 1).at(note).on = true;
            notes.at(port - 1).at(channel - 1).at(note).velocity = velocity;
        }

        void noteOff(uint8_t port, uint8_t channel, uint8_t note, uint8_t velocity)
        {
            notes.at(port - 1).at(channel - 1).at(note).on = false;
            notes.at(port - 1).at(channel - 1).at(note).velocity = velocity;
        }

        void polyPressure(uint8_t port, uint8_t channel, uint8_t note, uint8_t pressure)
        {
            notes.at(port-1).at(channel-1).at(note).afterTouch = pressure;
        }

        void channelPressure(uint8_t port, uint8_t channel, uint8_t pressure)
        {
            for (auto& note : notes.at(port - 1).at(channel - 1)) {
                note.afterTouch = pressure;
            }
        }

        void clearPort(uint8_t port)
        {
            for (auto& bs : notes.at(port - 1)) {
                bs.reset();
            }
        }

        void clearChannel(uint8_t port, uint8_t channel)
        {
            notes.at(port - 1).at(channel - 1).reset();
        }

        void holdOn(uint8_t port, uint8_t channel)
        {
            pedals.at(port - 1).set(channel - 1);
        }

        void holdOff(uint8_t port, uint8_t channel)
        {
            pedals.at(port - 1).reset(channel - 1);
        }

        void noteOn(uint8_t channel, uint8_t note)
        {
            notes[0].at(channel - 1).set(note);
        }

        void noteOff(uint8_t channel, uint8_t note)
        {
            notes[0].at(channel - 1).reset(note);
        }

        void clearChannel(uint8_t channel)
        {
            notes[0].at(channel - 1).reset();
        }

        void holdOn(uint8_t channel)
        {
            pedals[0].set(channel - 1);
        }

        void holdOff(uint8_t channel)
        {
            pedals[0].reset(channel - 1);
        }

    private:
        class NoteStateArray : public std::array<NoteState, 128> {
        public:
            [[nodiscard]] constexpr inline long long count() const noexcept
            {
                return std::count_if(cbegin(), cend(), [](const NoteState& lhs) {
                    return lhs.on;
                });
            }

            [[nodiscard]] constexpr inline bool test(size_type index) const
            {
                return at(index).on;
            }

            constexpr inline void set(size_type idx, bool on = true)
            {
                at(idx).on = on;
            }

            constexpr inline void reset()
            {
                for (auto& stat : *this) {
                    stat.on = false;
                    stat.afterTouch = 0;
                    stat.velocity = 64; // noteoff velocity
                }
            }

            constexpr inline void reset(size_type index)
            {
                at(index) = {};
            }
        };

        std::array<               // ports
            std::array<           // channels
                NoteStateArray, // notes
                NUM_CHANNELS>,
            NUM_PORTS>
                                                         notes;
        std::array<std::bitset<NUM_CHANNELS>, NUM_PORTS> pedals;
    };
}

#ifdef _MSC_VER
#pragma warning(pop) 
#endif
