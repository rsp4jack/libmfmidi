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

/// \file midistatus.hpp
/// \brief MIDIStatus

#pragma once

#include "libmfmidi/channel_note_status.hpp"
#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/midinotifier.hpp"
#include "libmfmidi/midiutils.hpp"
#include <array>
#include <optional>
#include <tuple>

namespace libmfmidi {
    struct channel_status {
        channel_status() noexcept
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
            aftertouch = 0;
            pitchbend  = 0x4000;
        }

        void resetAll() noexcept
        {
            reset();
        }

        std::optional<uint8_t>  program;    ///< Current Program Change (it will never bigger than uint8_t or smaller than -1)
        std::optional<uint16_t> volume;     // = 0xFF00
        std::optional<uint16_t> expression; // = 0xFF00
        std::optional<uint16_t> pan;        // = 0x4000
        std::optional<uint16_t> balance;    // = 0x4000
        std::optional<uint8_t>  aftertouch; // = 0
        std::optional<int16_t>  pitchbend;  // = 0
        // TODO: Control Change map for every CC
    };

    /// \brief A struct to hold MIDI Status
    struct MIDIStatus {
        MIDIStatus() noexcept
        {
            reset();
        }

        void reset() noexcept
        {
            tempo       = MIDITempo::fromBPM(120);
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

        MIDITempo                                                       tempo{}; // in bpm
        uint8_t                                                         numerator{};
        uint8_t                                                         denominator{};
        channel_note_status                                             matrix;
        std::array<std::array<channel_status, NUM_CHANNELS>, NUM_PORTS> channels;
    };

    /// \brief Emulate MIDIStatus
    class MIDIStatusProcessor : protected NotifyUtils<MIDIStatusProcessor> {
    public:
        static std::unordered_multimap<MIDIStatus*, MIDIStatusProcessor*> mstmap;
        explicit                                                          MIDIStatusProcessor(MIDIStatus& st, bool processNote = false) noexcept
            : mst(st)
            , mprocessNote(processNote)
        {
            mstmap.emplace(&st, this);
        }

        ~MIDIStatusProcessor() noexcept
        {
            auto eqr = mstmap.equal_range(&mst.get());
            for (auto it = eqr.first; it != eqr.second; ++it) {
                if (it->second == this) {
                    mstmap.erase(it);
                    break;
                }
            }
        }

        using NotifyUtils::addNotifier;
        friend class NotifyUtils;

        template <class T>
        bool process(const midi_message_owning_view<T>& msg, uint8_t port = 1)
        {
            //! Notify after change data
            switch (msg.type()) {
            case NOTE_ON:
            case NOTE_OFF:
            case CHANNEL_PRESSURE:
            case POLY_PRESSURE:
                if (mprocessNote) {
                    mst.get().matrix.process(msg, port);
                }
                // notify by MIDIMatrix
                break;
            case PROGRAM_CHANGE:
                mst.get().channels.at(port - 1).at(msg.channel() - 1).program = msg.program();
                notify(NotifyType::TR_PG);
                break;
            case PITCH_BEND:
                mst.get().channels.at(port - 1).at(msg.channel() - 1).pitchbend = msg.pitch_bend();
                notify(NotifyType::TR_PitchBend);
                break;
            case CONTROL_CHANGE: {
                auto& chst = mst.get().channels.at(port - 1).at(msg.channel() - 1);
                switch (msg.controller()) {
                case SUSTAIN:
                    mst.get().matrix.process(msg, port);
                    break;
                case BALANCE:
                    chst.balance = MLSBtoU16(msg.controller_value(), U16toMLSB(chst.balance).second);
                    break;
                case PAN:
                    chst.pan = MLSBtoU16(msg.controller_value(), U16toMLSB(chst.pan).second);
                    break;
                case BALANCE_LSB:
                    chst.balance = MLSBtoU16(U16toMLSB(chst.balance).first, msg.controller_value());
                    break;
                case PAN_LSB:
                    chst.pan = MLSBtoU16(U16toMLSB(chst.pan).first, msg.controller_value());
                    break;
                case VOLUME:
                    chst.volume = MLSBtoU16(msg.controller_value(), U16toMLSB(chst.volume).second);
                    break;
                case VOLUME_LSB:
                    chst.volume = MLSBtoU16(U16toMLSB(chst.volume).first, msg.controller_value());
                    break;
                case EXPRESSION:
                    chst.expression = MLSBtoU16(msg.controller_value(), U16toMLSB(chst.expression).second);
                    break;
                case EXPRESSION_LSB:
                    chst.expression = MLSBtoU16(U16toMLSB(chst.expression).first, msg.controller_value());
                    break;
                    // TODO: add more...
                }
                notify(NotifyType::TR_CC);
                break;
            }
            case META_EVENT:
                switch (msg.meta_type()) {
                case MIDIMetaNumber::TEMPO:
                    mst.get().tempo = msg.tempo();
                    notify(NotifyType::C_Tempo);
                    break;
                case MIDIMetaNumber::TIMESIG:
                    mst.get().denominator = msg.time_signature_denominator();
                    mst.get().numerator   = msg.time_signature_numerator();
                    notify(NotifyType::C_TimeSig);
                    break;
                }
            }
            return true;
        }

        template <class T>
        bool process(const MIDIBasicTimedMessage<T>& msg, uint8_t port = 1)
        {
            return process<T>(midi_message_owning_view<T>{msg}, port);
        }

    protected:
        void notify(NotifyType type) const noexcept
        {
            auto eqr = mstmap.equal_range(&mst.get());
            for (auto it = eqr.first; it != eqr.second; ++it) {
                it->second->NotifyUtils::notify(type);
            }
        }

    private:
        using enum MIDIMsgStatus;
        using enum MIDICCNumber;

        void doAddNotifier(const MIDINotifierFunctionType& func)
        {
            mst.get().matrix.addNotifier(func);
        }

        std::reference_wrapper<MIDIStatus> mst;
        bool                               mprocessNote;
    };

    static_assert(MIDIProcessorClass<MIDIStatusProcessor>); // For coding

    /// \brief Revert MIDIStatus to MIDI messages
    ///
    /// \param st \a MIDIStatus
    /// \param forFile Enable meta events
    /// \param programSetting -1 to not revert program change when st.program == -1, < -1 to not revert program change at all, > 0 to revert program to \a defaultProgram when st.program == -1
    /// \param port lookup state in the port
    /// \return std::vector<MIDIMessage>
    ///
    /// \warning This function will NOT revert Channel Prefix and Port Prefix!
    ///
    [[nodiscard]] inline std::vector<MIDIMessage> reportMIDIStatus(const MIDIStatus& st, bool forFile = true, int programSetting = 0, uint8_t port = 1) noexcept
    {
        std::vector<MIDIMessage> res;
        MIDIMessage              tmp;
        if (forFile) {
            tmp.setupTempo(st.tempo);
            res.push_back(std::move(tmp));
            tmp.setupTimeSignature(st.numerator, st.denominator, 24, 8);
            res.push_back(std::move(tmp));
        }
        auto doChannel = [&](const channel_status& st, uint8_t chn) {
            uint8_t lsb;
            uint8_t msb;

            std::tie(msb, lsb) = U16toMLSB(st.balance);
            tmp.setup_control_change(chn, MIDICCNumber::BALANCE, msb);
            res.push_back(std::move(tmp));

            tmp.setup_control_change(chn, MIDICCNumber::BALANCE_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.pan);
            tmp.setup_control_change(chn, MIDICCNumber::PAN, msb);
            res.push_back(std::move(tmp));

            tmp.setup_control_change(chn, MIDICCNumber::PAN_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.expression);
            tmp.setup_control_change(chn, MIDICCNumber::EXPRESSION, msb);
            res.push_back(std::move(tmp));

            tmp.setup_control_change(chn, MIDICCNumber::EXPRESSION_LSB, lsb);
            res.push_back(std::move(tmp));

            std::tie(msb, lsb) = U16toMLSB(st.volume);
            tmp.setup_control_change(chn, MIDICCNumber::VOLUME, msb);
            res.push_back(std::move(tmp));

            tmp.setup_control_change(chn, MIDICCNumber::VOLUME_LSB, lsb);
            res.push_back(std::move(tmp));

            if (programSetting >= -1 && (st.program != -1 || programSetting != -1)) {
                auto rprog = static_cast<uint8_t>(st.program == -1 ? programSetting : st.program);
                tmp.setup_program_change(chn, rprog);
                res.push_back(std::move(tmp));
            }
        };

        for (int i = 1; i <= NUM_CHANNELS; ++i) {
            doChannel(st.channels.at(port - 1).at(i - 1), static_cast<uint8_t>(i));
        }
        return res;
    }
}
