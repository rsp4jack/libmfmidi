/// \file midiutils.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDI utils
/// Rewrite requested.

// FIXME: Rewrite it.

#pragma once

#include "libmfmidi/mfconfig.hpp"
#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <iterator>

namespace libmfmidi::MIDIUtils {
    using MIDIClockTime = uint32_t;
    static_assert(sizeof(MIDIClockTime) == 4);
    constexpr uint8_t       NUM_CHANNELS  = 16;
    constexpr uint8_t       NUM_PORTS     = 16;
    constexpr uint16_t      NUM_TRACKS    = std::numeric_limits<uint16_t>::max();
    constexpr MIDIClockTime MIDICLKTM_MAX = std::numeric_limits<MIDIClockTime>::max(); ///< \brief Sometimes this means invalid value
    using MIDIVarNum                      = uint32_t;
    constexpr MIDIVarNum MIDIVARNUM_MAX   = std::numeric_limits<MIDIVarNum>::max();

    namespace MIDINumSpace {
        // Yes. That make it like enum class but with implicit conversion
        /// \brief MIDI Message Status Code
        /// \sa MIDICCNumber, MIDIRPNNumber, MIDIMetaNumber
        enum MIDIMsgStatus : uint8_t {
            // From jdksmidi
            NOTE_OFF         = 0x80, ///< Note off message with velocity
            NOTE_ON          = 0x90, ///< Note on message with velocity or Note off if velocity is 0
            POLY_PRESSURE    = 0xA0, ///< Polyphonic key pressure/aftertouch with note and pressure
            CONTROL_CHANGE   = 0xB0, ///< Control change message with controller number and 7 bit value
            PROGRAM_CHANGE   = 0xC0, ///< Program change message with 7 bit program number
            CHANNEL_PRESSURE = 0xD0, ///< Channel pressure/aftertouch with pressure
            PITCH_BEND       = 0xE0, ///< Channel bender with 14 bit bender value
            SYSEX_START      = 0xF0, ///< Start of a MIDI Normal System-Exclusive message
            MTC              = 0xF1, ///< Start of a two byte MIDI Time Code message
            SONG_POSITION    = 0xF2, ///< Start of a three byte MIDI Song Position message
            SONG_SELECT      = 0xF3, ///< Start of a two byte MIDI Song Select message
            TUNE_REQUEST     = 0xF6, ///< Single byte tune request message
            SYSEX_END        = 0xF7, ///< End of a MIDI Normal System-Exclusive message
            TIMING_CLOCK     = 0xF8, ///< 24 timing clocks per beat

            // MEASURE_END = 0xF9,      ///< Measure end byte

            START        = 0xFA, ///< Sequence start message
            CONTINUE     = 0xFB, ///< Sequence continue message
            STOP         = 0xFC, ///< Sequence stop message
            ACTIVE_SENSE = 0xFE, ///< Active sense message
            RESET        = 0xFF, ///< reset byte
            META_EVENT   = 0xFF  ///< meta event
        };

        /// \brief MIDI CC Controller Number
        /// \sa MIDIMsgStatus, MIDIRPNNumber, MIDIMetaNumber
        enum MIDICCNumber : uint8_t {
            BANK              = 0, ///< bank select msb
            MODULATION        = 1, ///< mod wheel
            BREATH            = 2, ///< breath controller
            FOOT              = 4, ///< foot pedal
            PORTA_TIME        = 5, ///< portamento time
            DATA_ENTRY        = 6, ///< data entry msb
            VOLUME            = 7,
            BALANCE           = 8,  ///< stereo balance
            PAN               = 10, ///< pan position (most common)
            EXPRESSION        = 11,
            EFFECT_CTRL_1     = 12, ///< effect control 1
            EFFECT_CTRL_2     = 13, ///< effect control 2
            GENERAL_1         = 16, ///< general purpose controller 1
            GENERAL_2         = 17, ///< general purpose controller 2
            GENERAL_3         = 18, ///< general purpose controller 3
            GENERAL_4         = 19, ///< general purpose controller 4
            BANK_LSB          = 32, // TODO: Fine this
            MODULATION_LSB    = 33,
            BREATH_LSB        = 34,
            FOOT_LSB          = 36,
            PORTA_TIME_LSB    = 37,
            DATA_ENTRY_LSB    = 38,
            VOLUME_LSB        = 39,
            BALANCE_LSB       = 40,
            PAN_LSB           = 42,
            EXPRESSION_LSB    = 43,
            EFFECT_CTRL_1_LSB = 44,
            EFFECT_CTRL_2_LSB = 45,
            SUSTAIN           = 64,
            PORTA             = 65,
            SOSTENUTO         = 66,
            SOFT              = 67,
            LEGATO            = 68,
            HOLD_2            = 69,
            SOUND_CTRL_1      = 70,
            SOUND_CTRL_2      = 71,
            SOUND_CTRL_3      = 72,
            SOUND_CTRL_4      = 73,
            SOUND_CTRL_5      = 74,
            SOUND_CTRL_6      = 75,
            SOUND_CTRL_7      = 76,
            SOUND_CTRL_8      = 77,
            SOUND_CTRL_9      = 78,
            SOUND_CTRL_10     = 79,
            GENERAL_5         = 80,
            GENERAL_6         = 81,
            GENERAL_7         = 82,
            GENERAL_8         = 83,
            EFFECT_1          = 91,
            EFFECT_2          = 92,
            EFFECT_3          = 93,
            EFFECT_4          = 94,
            EFFECT_5          = 95,
            DATA_INC          = 96,
            DATA_DEC          = 97,
            NRPN_LSB          = 98,
            NRPN_MSB          = 99,
            RPN_LSB           = 100,
            RPN_MSB           = 101,
            ALL_SOUND_OFF     = 120,
            RESET_CC          = 121,
            LOCAL_MODE        = 122,
            ALL_NOTE_OFF      = 123,
            OMNI_OFF          = 124,
            OMNI_ON           = 125,
            MONO              = 126,
            POLY              = 127
        };

        /// \brief MIDI RPN Number
        /// \sa MIDIMsgStatus, MIDICCNumber, MIDIMetaNumber
        enum MIDIRPNNumber : uint8_t {
            PITCH_BEND_RANGE = 0,    ///< Pitch bend range,
            FINE_TUNE        = 0x01, ///< fine tuning
            COARSE_TUNE      = 0x02, ///< coarse tuning
            PROGRAM_TUNE     = 0x03, ///< Tuning Program Change
            BANK_TUNE        = 0x04, ///< Tuning Bank Select
            MODULATION_DEPTH = 0x05  ///< Modulation Depth Range
        };
    }

    namespace MIDIMetaNumSpace {
        /// \brief MIDI Meta Message Number
        /// \sa MIDIMsgStatus, MIDICCNumber, MIDIRPNNumber
        enum MIDIMetaNumber {
            // From jdksmidi

            // Sequence Number
            // This meta event defines the pattern number of a Type 2 MIDI file
            // or the number of a sequence in a Type 0 or Type 1 MIDI file.
            // This meta event should always have a delta time of 0 and come before
            // all MIDI Channel Events and non-zero delta time events.
            SEQUENCE_NUMBER = 0x00,

            GENERIC_TEXT    = 0x01,
            COPYRIGHT       = 0x02,
            TRACK_NAME      = 0x03, // Sequence/Track Name
            INSTRUMENT_NAME = 0x04,
            LYRIC_TEXT      = 0x05,
            MARKER_TEXT     = 0x06,
            CUE_POINT       = 0x07,
            // PROGRAM_NAME = 0x08, // Not in SMF Spec
            // DEVICE_NAME = 0x09, // Too.
            // GENERIC_TEXT_A = 0x0A,
            // GENERIC_TEXT_B = 0x0B,
            // GENERIC_TEXT_C = 0x0C,
            // GENERIC_TEXT_D = 0x0D,
            // GENERIC_TEXT_E = 0x0E,
            // GENERIC_TEXT_F = 0x0F,

            CHANNEL_PREFIX = 0x20, // This meta event associates a MIDI channel with following meta events.
                                   // It's effect is terminated by another MIDI Channel Prefix event or any non-Meta event.
                                   // It is often used before an Instrument Name Event to specify which channel an instrument name represents.

            OUTPUT_PORT = 0x21, // may be MIDI Output Port, data length = 1 byte
            // TRACK_LOOP = 0x2E,
            END_OF_TRACK = 0x2F,

            TEMPO        = 0x51,
            SMPTE_OFFEST = 0x54, // SMPTE Offset: SMPTE time to denote playback offset from the beginning
            TIMESIG      = 0x58,
            KEYSIG       = 0x59,

            SEQUENCER_SPECIFIC = 0x7F // This meta event is used to specify information specific to a hardware or
                                      // software sequencer. The first Data byte (or three bytes if the first byte is 0) specifies the manufacturer's
                                      // ID and the following bytes contain information specified by the manufacturer.
        };
    }

    // let them be visible in clangd
    // NOLINTBEGIN(misc-unused-using-decls)
    using MIDIMetaNumSpace::MIDIMetaNumber;
    using MIDINumSpace::MIDICCNumber;
    using MIDINumSpace::MIDIMsgStatus;
    using MIDINumSpace::MIDIRPNNumber;
    // NOLINTEND(misc-unused-using-decls)

    /// \brief LUT to MIDI message length
    /// \code{.cpp}
    /// int length = LUT_MSGLEN[status>>4]
    /// \endcode
    constexpr std::array<int, 16> LUT_MSGLEN = {
        0, 0, 0, 0, 0, 0, 0, 0,
        3, // 0x80=note off, 3 bytes
        3, // 0x90=note on, 3 bytes
        3, // 0xA0=poly pressure, 3 bytes
        3, // 0xB0=control change, 3 bytes
        2, // 0xC0=program change, 2 bytes
        2, // 0xD0=channel pressure, 2 bytes
        3, // 0xE0=pitch bend, 3 bytes
        -1 // 0xF0=other things. may vary.
    };

    /// \brief LUT to MIDI System Message message length
    /// \note This is \b System \b Message, not SysEx.
    /// \code{.cpp}
    /// int length = LUT_SMSGLEN[status-0xF0]
    /// \endcode
    constexpr std::array<int, 16> LUT_SMSGLEN = {
        // System Common Messages
        -1, // 0xF0=Normal SysEx Events start. may vary
        2,  // 0xF1=MIDI Time Code. 2 bytes
        3,  // 0xF2=MIDI Song position. 3 bytes
        2,  // 0xF3=MIDI Song Select. 2 bytes.
        0,  // 0xF4=undefined. (Reserved)
        0,  // 0xF5=undefined. (Reserved)
        1,  // 0xF6=TUNE Request. 1 byte
        0,  // 0xF7=Normal or Divided SysEx Events end.
        // -1, // 0xF7=Divided or Authorization SysEx Events. may vary

        // System Real-Time Messages
        1, // 0xF8=timing clock. 1 byte
        1, // 0xF9=proposed measure end? (Reserved)
        1, // 0xFA=start. 1 byte
        1, // 0xFB=continue. 1 byte
        1, // 0xFC=stop. 1 byte
        0, // 0xFD=undefined. (Reserved)
        1, // 0xFE=active sensing. 1 byte
        /// \warning META is not System Message.
        1, // 0xFF=reset. 1 byte

        /// \warning RESET message will NOT be in file, META event will NOT be when device.
        // 3 // 0xFF=not reset, but a META-EVENT, which is always 3 bytes
        //-1 // 0xFF=reset or meta, 1 or ??? byte.
    };

    /// \brief LUT to white keys in MIDI pitch number
    /// \code{.cpp}
    /// bool isWhite = LUT_WHITEKEY[pitch]
    /// \endcode
    constexpr std::array<bool, 12> LUT_WHITEKEY = {
        // C  C#     D     D#     E     F     F#     G     G#     A     A#     B
        true, false, true, false, true, true, false, true, false, true, false, true};

    /// \brief Get the expected MIDI message length
    /// \warning The return value may be \b negative .
    /// \param[in] status MIDI message status code
    /// \return int
    inline constexpr int getExpectedMessageLength(uint8_t status)
    {
        return LUT_MSGLEN.at(status >> 4U);
    }

    /// \brief Get the Expected System Message Length
    /// \warning The return value may be \b negative .
    /// \param[in] status Message Status
    /// \return int
    inline constexpr int getExpectedSMessageLength(uint8_t status)
    {
        return LUT_SMSGLEN.at(status - 0xF0);
    }

    /// \brief Get expected Meta event length (entrie message)
    /// \c -1 : Variable length
    /// \c -2 : unknown
    /// \param type
    /// \return constexpr int
    inline constexpr int getExpectedMetaLength(uint8_t type)
    {
        auto unwrapped = [&]() {
            switch (type) {
            case 0x00:
                return 2;
            case 0x20:
            case 0x21:
                return 1;
            case 0x2F:
                return 0;
            case 0x51:
                return 3;
            case 0x54:
                return 5;
            case 0x58:
                return 4;
            case 0x59:
                return 2;
            case 0x7F:
                return -1;
            default:
                if (type >= 0x01 && type <= 0x07) {
                    return -1;
                }
                return -2;
            }
        };
        auto result = unwrapped();
        if (result < 0) {
            return result;
        }
        return result + 3;
    }

    /// \brief Checking if the note is a white key.
    /// \param[in] pitch Note pitch
    /// \return true
    /// \return false
    inline constexpr bool isNoteWhite(uint8_t pitch)
    {
        return LUT_WHITEKEY.at(pitch % 12);
    }

    /// \brief A negation of \c isNoteWhite .
    /// \param[in] pitch Note pitch
    /// \return true
    /// \return false
    inline constexpr bool isNoteBlack(uint8_t pitch)
    {
        return !isNoteWhite(pitch);
    }

    /// \brief Get the octave of the note.
    /// \param pitch Note pitch
    /// \return int
    inline constexpr int getNoteOctave(uint8_t pitch)
    {
        return (pitch / 12) - 1;
    }

    inline constexpr std::pair<MIDIVarNum, size_t> readVarNum(std::istream* ise)
    {
        auto readU8 = [&]() {
            uint8_t d;
            ise->read(reinterpret_cast<char*>(&d), 1);
            return d;
        };
        uint32_t value;
        uint8_t  c   = readU8();
        size_t   cnt = 1;
        value        = c;
        if ((value & 0x80) != 0U) {
            value &= 0x7F;
            do {
                c = readU8();
                ++cnt;
                value = (value << 7) + (c & 0x7F);
            } while ((c & 0x80) != 0);
        }
        return {value, cnt};
    }

    template <std::forward_iterator FwdIt>
    inline constexpr std::pair<MIDIVarNum, size_t> readVarNumIt(FwdIt iter, FwdIt end)
    {
        auto readU8 = [&]() {
            uint8_t d = *iter;
            ++iter;
            return d;
        };
        uint32_t value;
        uint8_t  c   = readU8();
        size_t   cnt = 1;

        value = c;
        if ((value & 0x80) != 0U) {
            value &= 0x7F;
            do {
                if (iter >= end) {
                    return {-1, -1};
                }
                c = readU8();
                ++cnt;
                value = (value << 7) + (c & 0x7F);
            } while ((c & 0x80) != 0);
        }
        return {value, cnt};
    }

    inline constexpr size_t writeVarNum(MIDIVarNum data, std::ostream* ose)
    {
        MIDIVarNum buf;
        size_t     cnt = 0;
        buf            = data & 0x7F;
        while ((data >>= 7) > 0) {
            buf <<= 8;
            buf |= 0x80;
            buf += data & 0x7F;
        }
        while (true) {
            ose->put(buf & 0xFF);
            ++cnt;
            if ((buf & 0x80) != 0U) {
                buf >>= 8;
            } else {
                break;
            }
        }
        return cnt;
    }

    template <std::forward_iterator FwdIt>
    inline constexpr size_t writeVarNum(MIDIVarNum data, FwdIt iter)
        requires std::is_same_v<std::iterator_traits<FwdIt>::value_type, uint8_t>
    {
        MIDIVarNum buf;
        size_t     cnt = 0;
        buf            = data & 0x7F;
        while ((data >>= 7) > 0) {
            buf <<= 8;
            buf |= 0x80;
            buf += data & 0x7F;
        }
        while (true) {
            *iter = buf & 0xFF;
            ++iter;

            ++cnt;
            if ((buf & 0x80) != 0U) {
                buf >>= 8;
            } else {
                break;
            }
        }
        return cnt;
    }

    inline size_t varNumLen(MIDIVarNum data)
    {
        MIDIVarNum buf;
        size_t     cnt = 0;
        buf            = data & 0x7F;
        while ((data >>= 7) > 0) {
            buf <<= 8;
            buf |= 0x80;
            buf += data & 0x7F;
        }
        while (true) {
            // ose->put(buf & 0xFF);
            ++cnt;
            if ((buf & 0x80) != 0U) {
                buf >>= 8;
            } else {
                break;
            }
        }
        return cnt;
    }

    inline uint16_t MLSBtoU16(uint8_t msb, uint8_t lsb) noexcept
    {
        return (msb << 7) | (lsb & 0x7F);
    }

    inline std::pair<uint8_t, uint8_t> U16toMLSB(uint16_t val) noexcept
    {
        return {(val >> 7) & 0x7F, val & 0x7F};
    }

    inline double divisionToSec(int16_t val, uint32_t bpm)
    {
        if (val > 0) {
            // MIDI always 4/4
            return 60.0 / (val * bpm);
        }
        if (val < 0) {
            uint8_t fps;
            uint8_t ppf;
            ppf = val & 0xFF;
            fps = (val >> 8) & 0x7F; // TODO: negative?
                                     // 24 25 29(.97) 30
            double realfps = fps;
            if (fps == 29) {
                realfps = 29.97;
            }
            return 1 / (realfps * ppf);
        }
    }
}