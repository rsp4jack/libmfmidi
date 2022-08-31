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

/// \file midistate.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIState

#pragma once

#include "mfconcepts.hpp"
#include "midiutils.hpp"
#include "midimatrix.hpp"
#include <array>
#include <tuple>

namespace libmfmidi {
    using MIDIUtils::MIDIClockTime;

    class MIDIChannelState {
    public:
        MIDIChannelState() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            program    = -1;
            volume     = 0xFF00;
            expression = 0xFF00;
            pan        = 0x4000;
            balance    = 0x4000;
        }

        void resetAll() noexcept
        {
            reset();
        }

        int      program{}; ///< Current Program Change (it will never bigger than uint8_t or smaller than -1)
        uint16_t volume{};  // msb and lsb
        uint16_t expression{};
        uint16_t pan{}; // most use
        uint16_t balance{};
    };

    /// \brief A struct to hold MIDI Status
    struct MIDIState {
        MIDIState() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            tempo       = 120;
            numerator   = 4;
            denominator = 4;
            matrix.clear();
        }

        void resetAll() noexcept
        {
            reset();
            for (auto& port : channels) {
                for (auto& chstate : port) {
                    chstate.reset();
                }
            }
        }

        uint32_t                                                          tempo{}; // in bpm
        uint8_t                                                           numerator{};
        uint8_t                                                           denominator{};
        MIDIMatrix                                                        matrix;
        std::array<std::array<MIDIChannelState, NUM_CHANNELS>, NUM_PORTS> channels;
    };

    /// \brief A state machine (Maybe?) Emulate state by MIDI eventS.
    class MIDIStateProcessor {
    public:
        explicit MIDIStateProcessor(MIDIState& st) noexcept
            : mst(st)
        {
        }

        bool process(const MIDITimedMessage& msg, uint8_t port = 1)
        {
            using namespace MIDINumSpace;
            switch (msg.type()) {
            case NOTE_ON:
                mst.matrix.noteOn(port, msg.channel(), msg.note());
                break;
            case NOTE_OFF:
                mst.matrix.noteOff(port, msg.channel(), msg.note());
                break;
            case PROGRAM_CHANGE:
                mst.channels.at(port - 1).at(msg.channel() - 1).program = msg.programChangeValue();
                break;
            case CONTROL_CHANGE: {
                auto& chst = mst.channels.at(port - 1).at(msg.channel() - 1);
                switch (msg.controller()) {
                case BALANCE:
                    chst.balance = MLSBtoU16(msg.controllerValue(), U16toMLSB(chst.balance).second);
                    break;
                case PAN:
                    chst.pan = MLSBtoU16(msg.controllerValue(), U16toMLSB(chst.pan).second);
                    break;
                case BALANCE_LSB:
                    chst.balance = MLSBtoU16(U16toMLSB(chst.balance).first, msg.controllerValue());
                    break;
                case PAN_LSB:
                    chst.pan = MLSBtoU16(U16toMLSB(chst.pan).first, msg.controllerValue());
                    break;
                case VOLUME:
                    chst.volume = MLSBtoU16(msg.controllerValue(), U16toMLSB(chst.volume).second);
                    break;
                case VOLUME_LSB:
                    chst.volume = MLSBtoU16(U16toMLSB(chst.volume).first, msg.controllerValue());
                    break;
                case EXPRESSION:
                    chst.expression = MLSBtoU16(msg.controllerValue(), U16toMLSB(chst.expression).second);
                    break;
                case EXPRESSION_LSB:
                    chst.expression = MLSBtoU16(U16toMLSB(chst.expression).first, msg.controllerValue());
                    break;
                    // TODO: add more...
                }
                break;
            }
            case META_EVENT:
                switch (msg.metaType()) {
                case MIDIMetaNumSpace::TEMPO:
                    mst.tempo = msg.bpm();
                    break;
                case MIDIMetaNumSpace::TIMESIG:
                    mst.denominator = msg.timeSigDenominator();
                    mst.numerator   = msg.timeSigNumerator();
                    break;
                }
            }
            return true;
        }

    private:
        MIDIState& mst;
    };

    static_assert(MIDIProcessor<MIDIStateProcessor>); // For coding

    /// \brief Revert MIDIState to MIDI messages
    ///
    /// \param st \a MIDIState
    /// \param forFile Enable meta events
    /// \param defaultProgram -1 to not revert program change when st.program == -1, < -1 to not revert program change at all, > 0 to revert program to \a defaultProgram when st.program == -1
    /// \param port lookup state in the port
    /// \return std::vector<MIDIMessage>
    ///
    /// \warning This function will NOT revert Channel Prefix and Port Prefix!
    ///
    inline std::vector<MIDIMessage> reportMIDIState(const MIDIState& st, bool forFile = true, int defaultProgram = 0, uint8_t port = 1) noexcept
    {
        std::vector<MIDIMessage> res;
        MIDIMessage              tmp;
        if (forFile) {
            tmp.setupBPM(st.tempo);
            res.push_back(std::move(tmp));
            tmp.setupTimeSignature(st.numerator, st.denominator, 24, 8);
            res.push_back(std::move(tmp));
        }
        auto doChannel = [&](const MIDIChannelState& st, uint8_t chn) {
            uint8_t lsb;
            uint8_t msb;

            std::tie(msb, lsb) = U16toMLSB(st.balance);
            tmp.setupControlChange(chn, MIDICCNumber::BALANCE, msb);
            res.push_back(std::move(tmp));

            tmp.setupControlChange(chn, MIDICCNumber::BALANCE_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.pan);
            tmp.setupControlChange(chn, MIDICCNumber::PAN, msb);
            res.push_back(std::move(tmp));

            tmp.setupControlChange(chn, MIDICCNumber::PAN_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.expression);
            tmp.setupControlChange(chn, MIDICCNumber::EXPRESSION, msb);
            res.push_back(std::move(tmp));

            tmp.setupControlChange(chn, MIDICCNumber::EXPRESSION_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.volume);
            tmp.setupControlChange(chn, MIDICCNumber::VOLUME, msb);
            res.push_back(std::move(tmp));

            tmp.setupControlChange(chn, MIDICCNumber::VOLUME_LSB, lsb);
            res.push_back(std::move(tmp));

            if (defaultProgram >= -1 && (st.program != -1 || defaultProgram != -1)) {
                uint8_t rprog = (st.program == -1 ? defaultProgram : st.program);
                tmp.setupProgramChange(chn, rprog);
                res.push_back(std::move(tmp));
            }
        };

        for (int i = 1; i <= NUM_CHANNELS; ++i) {
            doChannel(st.channels.at(port-1).at(i-1), i);
        }
        return res;
    }
}