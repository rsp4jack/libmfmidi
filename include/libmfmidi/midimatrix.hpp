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
        MIDIMatrix() noexcept
        {
        }

        bool process(const MIDITimedMessage& msg, uint8_t port = 1)
        {
            if (msg.isChannelMsg()) {
                auto chn = msg.channel();
                auto note = msg.note();

                if (msg.isAllNotesOff() || msg.isAllSoundsOff()) {
#ifdef ENABLE_UNSPEC_FEATURES
                    clearChannel(port, chn);
#else
                    clearChannel(chn);
#endif
                } else if (msg.isImplicitNoteOn()) {
#ifdef ENABLE_UNSPEC_FEATURES
                    noteOn(port, chn, note);
#else
                    noteOn(chn, note);
#endif
                } else if (msg.isImplicitNoteOff()) {
#ifdef ENABLE_UNSPEC_FEATURES
                    noteOff(port, chn, note);
#else
                    noteOff(chn, note);
#endif
                } else if (msg.isCCSustainOn()) {
#ifdef ENABLE_UNSPEC_FEATURES
                    holdOn(port, chn);
#else
                    holdOn(chn);
#endif
                } else if (msg.isCCSustainOff()) {
#ifdef ENABLE_UNSPEC_FEATURES
                    holdOff(port, chn);
#else
                    holdOff(chn);
#endif
                }
            }
        }

        void clear()
        {
            notes = {};
        }

        [[nodiscard]] int totalCount() const
        {
            int result = 0;
            for (const auto& ar : notes) {
                for (const auto& bs : ar) {
                    result += bs.count();
                }
            }
            return result;
        }
#ifdef ENABLE_UNSPEC_FEATURES
        int portCount(uint8_t port) const
        {
            int result = 0;
            for (const auto& bs : notes.at(port - 1)) {
                result += bs.count();
            }
            return result;
        }

        [[nodiscard]] int channelCount(uint8_t port, uint8_t channel) const
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
#endif
        [[nodiscard]] int channelCount(uint8_t channel) const
        {
            return notes[0].at(channel - 1).count();
        }

        [[nodiscard]] bool isNoteOn(uint8_t channel, uint8_t note) const
        {
            return notes[0].at(channel - 1).test(note);
        }

        bool isHold(uint8_t channel) const
        {
            return pedals[0].test(channel - 1);
        }

    protected:
#ifdef ENABLE_UNSPEC_FEATURES
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
#endif

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
        std::array<std::array<std::bitset<128>, 16>, // 16 Channels' one more bit
#ifdef ENABLE_UNSPEC_FEATURES
                   16>
            notes; // 16 Ports (Unspec)
#else
                   1>
            notes;
#endif
        std::array<std::bitset<16>,
#ifdef ENABLE_UNSPEC_FEATURES
                   16>
            pedals;
#else
                   1>
            pedals;
#endif
    };
}
