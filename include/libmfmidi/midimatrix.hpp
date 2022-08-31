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
                auto note = msg.note();

                if (msg.isAllNotesOff() || msg.isAllSoundsOff()) {
                    clearChannel(port, chn);
                } else if (msg.isImplicitNoteOn()) {
                    noteOn(port, chn, note);
                } else if (msg.isImplicitNoteOff()) {
                    noteOff(port, chn, note);
                } else if (msg.isCCSustainOn()) {
                    holdOn(port, chn);
                } else if (msg.isCCSustainOff()) {
                    holdOff(port, chn);
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

        void noteOn(uint8_t port, uint8_t channel, uint8_t note)
        {
            notes.at(port - 1).at(channel - 1).set(note);
        }

        void noteOff(uint8_t port, uint8_t channel, uint8_t note)
        {
            notes.at(port - 1).at(channel - 1).reset(note);
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
        std::array<               // ports
            std::array<           // channels
                std::bitset<128>, // notes
                NUM_CHANNELS>,
            NUM_PORTS>
                                                         notes;
        std::array<std::bitset<NUM_CHANNELS>, NUM_PORTS> pedals;
    };
}
