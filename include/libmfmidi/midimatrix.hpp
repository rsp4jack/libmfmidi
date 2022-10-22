/// \file midimatrix.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIMatrix

#pragma once

#include "midimessage.hpp"
#include "midiutils.hpp"
#include <array>
#include <bitset>
#include <cstdint>

namespace libmfmidi {
    using std::uint8_t;

    /// \brief A matrix to keep note and pedal
    ///
    class MIDIMatrix {
    public:
        MIDIMatrix() noexcept = default;

        bool process(const MIDITimedMessage& msg, uint8_t port = 1)
        {
            if (msg.isChannelMsg()) {
                auto chn  = msg.channel();

                if (msg.isAllNotesOff() || msg.isAllSoundsOff()) {
                    clearChannel(port, chn);
                } else if (msg.isImplicitNoteOn()) {
                    noteOn(port, chn, msg.note(), msg.velocity());
                } else if (msg.isImplicitNoteOff()) {
                    noteOff(port, chn, msg.note(), msg.velocity());
                } else if (msg.isCCSustainOn()) {
                    holdOn(port, chn);
                } else if (msg.isCCSustainOff()) {
                    holdOff(port, chn);
                } else if (msg.isPolyPressure()) {
                    polyPressure(port, chn, msg.note(), msg.pressure());
                } else if (msg.isChannelPressure()) {
                    channelPressure(port, chn, msg.pressure());
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
        struct NoteState {
            bool on = false;
            uint8_t velocity = 64; ///< note on and off velocity
            uint8_t afterTouch = 0;
        };

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
