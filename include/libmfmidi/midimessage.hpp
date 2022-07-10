/// \file midimessage.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIMessage class

#pragma once

#include "mfutils.hpp"
#include "midiutils.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace libmfmidi {
    using std::int16_t;
    using std::int8_t;
    using std::uint8_t;
    using namespace Utils;
    using namespace MIDIUtils;

    /// \brief Container for MIDI Message
    ///
    /// The MIDIMessage class is a simple, lightweight container which can hold a single
    /// MIDI Message that can fit within 7 bytes plus status byte.  It can also hold some
    /// non-MIDI messages, known as Meta messages like No-op, Key signature, Time Signature, etc,
    /// which are useful for internal processing.
    ///
    /// All setters will auto match message types.
    /// For example, setVelcoity will set byte2 in note event and poly pressure(aftertouch).
    /// and set byte1 in channel pressure.
    /// All getters and is-ers will assert message types.

    class MIDIMessage {
    public:
        /// \brief The default constructor
        constexpr explicit MIDIMessage() noexcept
        {
            clear();
        }

        constexpr ~MIDIMessage() noexcept = default;

        constexpr MIDIMessage(const MIDIMessage& rhs) noexcept            = default;
        constexpr MIDIMessage(MIDIMessage&& rhs) noexcept                 = default;
        constexpr MIDIMessage& operator=(const MIDIMessage& rhs) noexcept = default;
        constexpr MIDIMessage& operator=(MIDIMessage&& rhs) noexcept      = default;

        /// \brief Clear this message
        constexpr void clear() noexcept
        {
            _data.clear();
            marker = Utils::MFMessageMark::None;
        }

        /// \name Getters
        /// \{

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return !isMFMarker() && _data.empty();
        }

        /// \brief Convert message to human readable message
        [[nodiscard]] std::string msgText() const
        {
            using std::dec;
            using std::endl;
            using std::hex;

            if (empty()) {
                return "Empty message";
            }

            std::stringstream str;
            if (isMFMarker()) {
                str << "MFMarker:" << static_cast<int>(marker) << endl;
            }
            if (isMetaEvent()) {
                str << "Meta Event:" << endl
                    << "Meta Length:" << _data[2] << endl;
            }
            if (isSysEx()) {
                str << "SysEx";
            }

            if (isChannelMsg()) {
                str << "Channel:" << channel();
            }
            if (isTextEvent()) {
                str << "Text:" << textEventText() << endl;
            }

            str << "Full message:";
            for (const auto& a : _data) {
                str << ' ';
                str << hex << a;
            }
            str << dec << endl;

            switch (type()) {
            case MIDIMsgStatus::NOTE_ON:
                str << "Note On" << note() << velocity();
                break;
            case MIDIMsgStatus::NOTE_OFF:
                str << "Note Off" << note() << velocity();
                break;
            case MIDIMsgStatus::POLY_PRESSURE:
                str << "Poly Pressure" << _data[1] << _data[2];
                break;
            case MIDIMsgStatus::CONTROL_CHANGE:
                str << "Control Change" << controller() << controllerValue();
                break;
            case MIDIMsgStatus::PROGRAM_CHANGE:
                str << "Program Change" << programChangeValue();
                break;
            case MIDIMsgStatus::CHANNEL_PRESSURE:
                str << "Channel Pressure" << _data[1];
                break;
            case MIDIMsgStatus::PITCH_BEND:
                str << "Pitch Bend" << pitchBendValue();
                break;
            }

            return str.str();
        }

        [[nodiscard]] constexpr size_t length() const
        {
            return _data.size();
        }

        [[nodiscard]] constexpr std::vector<uint8_t> data() const
        {
            return _data;
        }

        /// \warning May return negative
        [[nodiscard]] constexpr int expectedLength() const
        {
            if (isMetaEvent()) {
                return _data[2];
            }
            if (isSystemMessage()) {
                return getExpectedSCMMessageLength(_data[0]);
            }
            return getExpectedMessageLength(_data[0]);
        }

        [[nodiscard]] constexpr uint8_t statusByte() const
        {
            return _data[0];
        }

        [[nodiscard]] constexpr uint8_t channel() const
        {
            assert(isChannelMsg());
            return (_data[0] & 0x0FU) + 1;
        }

        [[nodiscard]] constexpr uint8_t type() const
        {
            return _data[0] & 0xF0U;
        }

        [[nodiscard]] constexpr uint8_t note() const
        {
            assert(isNote());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t velocity() const
        {
            assert(isNoteOn());
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t controller() const
        {
            assert(isControlChange());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t controllerValue() const
        {
            assert(isControlChange());
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t metaType() const
        {
            assert(isMetaEvent());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t channelPressure() const
        {
            assert(isChannelPressure());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t programChangeValue() const
        {
            assert(isProgramChange());
            return _data[1];
        }

        /// \brief Get signed pitch bend value
        [[nodiscard]] constexpr int16_t pitchBendValue() const
        {
            assert(isPitchBend());
            return (static_cast<int16_t>(_data[2] << 7U) | _data[1]) - 0x2000;
        }

        [[nodiscard]] constexpr uint8_t timeSigNumerator() const
        {
            assert(isTimeSignature());
            return _data[3];
        }

        /// \brief Get \b Powered Denominator
        [[nodiscard]] constexpr uint8_t timeSigDenominator() const
        {
            assert(isTimeSignature());
            return static_cast<uint8_t>(std::pow(2, _data[4]));
        }

        [[nodiscard]] constexpr uint8_t timeSigDenominatorRaw() const
        {
            assert(isTimeSignature());
            return _data[4];
        }

        [[nodiscard]] constexpr int8_t keySigSharpFlats() const
        {
            assert(isKeySignature());
            return static_cast<int8_t>(_data[3]); // In C++20
        }

        [[nodiscard]] constexpr bool keySigMajorMinor() const
        {
            assert(isKeySignature());
            return _data[4] != 0U;
        }

        [[nodiscard]] constexpr double balancePan() const
        {
            assert(isCCBalance() || isCCPan());
            int val = controllerValue();
            if (val == 127) {
                val = 128;
            }
            return (val - 64) / 64.0;
        }

        [[nodiscard]] constexpr uint32_t rawTempo() const
        {
            assert(isTempo());
            return rawCat(_data[3], _data[4], _data[5]);
        }

        [[nodiscard]] constexpr unsigned long bpm() const
        {
            return 60000000 / rawTempo();
        }

        [[nodiscard]] constexpr std::string textEventText() const
        {
            assert(isTextEvent());
            return {_data.cbegin() + 3, _data.cend()};
        }

        // For later use
        /* #define rens(n)                           \
            [[nodiscard]] uint8_t byte##n() const \
            {                                     \
                return data[n];                   \
            } */

        [[nodiscard]] constexpr uint8_t byte0() const
        {
            return _data[0];
        }

        [[nodiscard]] constexpr uint8_t byte1() const
        {
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t byte2() const
        {
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t byte3() const
        {
            return _data[3];
        }

        [[nodiscard]] constexpr uint8_t byte4() const
        {
            return _data[4];
        }

        [[nodiscard]] constexpr uint8_t byte5() const
        {
            return _data[5];
        }

        [[nodiscard]] constexpr uint8_t byte6() const
        {
            return _data[6];
        }

        [[nodiscard]] constexpr uint8_t byte7() const
        {
            return _data[7];
        }

        [[nodiscard]] constexpr uint8_t byte8() const
        {
            return _data[8];
        }

        [[nodiscard]] constexpr uint8_t byte9() const
        {
            return _data[9];
        }

        [[nodiscard]] constexpr uint8_t byte10() const
        {
            return _data[10];
        }

        [[nodiscard]] constexpr uint8_t byte11() const
        {
            return _data[11];
        }

        [[nodiscard]] constexpr uint8_t byte12() const
        {
            return _data[12];
        }

        [[nodiscard]] constexpr uint8_t byte13() const
        {
            return _data[13];
        }

        [[nodiscard]] constexpr uint8_t byte14() const
        {
            return _data[14];
        }

        [[nodiscard]] constexpr uint8_t byte15() const
        {
            return _data[15];
        }

        [[nodiscard]] constexpr uint8_t byte16() const
        {
            return _data[16];
        }

        [[nodiscard]] constexpr MFMessageMark MFMarker() const
        {
            return marker;
        }

        /// \}

        /// \name Setter
        /// \{

        /// \brief \b Clear and set length
        constexpr void setLength(size_t len)
        {
            _data.clear();
            _data.resize(len, 0);
        }

        constexpr void expandLength(size_t size)
        {
            if (_data.size() < size) {
                _data.resize(size, 0);
            }
        }

        constexpr void setStatus(uint8_t a)
        {
            expandLength(1);
            _data[0] = a;
        }

        constexpr void setChannel(uint8_t a)
        {
            expandLength(1);
            assert(a <= 16);
            _data[0] = (_data[0] & 0xF0) | (a - 1);
        }

        /// \warning \a a must be 0x40, 0xF0, 0x70, etc...
        /// The least significant 4 bit will be ignored.
        constexpr void setTyoe(uint8_t a)
        {
            expandLength(1);
            assert((a >> 1) <= 0xF);
            _data[0] = (_data[0] & 0x0F) | a;
        }

        /// \brief Equals to \a setStatus() .
        constexpr void setByte0(uint8_t a)
        {
            setStatus(a);
        }

        // for later use
        /* #define ens(x)                 \
            void setByte##x(uint8_t a) \
            {                          \
                ensureSize(x + 1);     \
                data[x] = a;           \
            } */

        constexpr void setByte1(uint8_t a)
        {
            expandLength(2);
            _data[1] = a;
        }

        constexpr void setByte2(uint8_t a)
        {
            expandLength(2 + 1);
            _data[2] = a;
        }

        constexpr void setByte3(uint8_t a)
        {
            expandLength(3 + 1);
            _data[3] = a;
        }

        constexpr void setByte4(uint8_t a)
        {
            expandLength(4 + 1);
            _data[4] = a;
        }

        constexpr void setByte5(uint8_t a)
        {
            expandLength(5 + 1);
            _data[5] = a;
        }

        constexpr void setByte6(uint8_t a)
        {
            expandLength(6 + 1);
            _data[6] = a;
        }

        constexpr void setByte7(uint8_t a)
        {
            expandLength(7 + 1);
            _data[7] = a;
        }

        constexpr void setByte8(uint8_t a)
        {
            expandLength(8 + 1);
            _data[8] = a;
        }

        constexpr void setByte9(uint8_t a)
        {
            expandLength(9 + 1);
            _data[9] = a;
        }

        constexpr void setByte10(uint8_t a)
        {
            expandLength(10 + 1);
            _data[10] = a;
        }

        constexpr void setByte11(uint8_t a)
        {
            expandLength(11 + 1);
            _data[11] = a;
        }

        constexpr void setByte12(uint8_t a)
        {
            expandLength(12 + 1);
            _data[12] = a;
        }

        constexpr void setByte13(uint8_t a)
        {
            expandLength(13 + 1);
            _data[13] = a;
        }

        constexpr void setByte14(uint8_t a)
        {
            expandLength(14 + 1);
            _data[14] = a;
        }

        constexpr void setByte15(uint8_t a)
        {
            expandLength(15 + 1);
            _data[15] = a;
        }

        constexpr void setByte16(uint8_t a)
        {
            expandLength(16 + 1);
            _data[16] = a;
        }

        constexpr void setNote(uint8_t a)
        {
            assert(isNote() || isPolyPressure());
            _data[1] = a;
        }

        constexpr void setVelocity(uint8_t a)
        {
            assert(isNote() || isPolyPressure() || isChannelPressure());
            if (isNote() || isPolyPressure()) {
                _data[2] = a;
            } else if (isChannelPressure()) {
                _data[1] = a;
            }
        }

        constexpr void setProgramChangeValue(uint8_t a)
        {
            assert(isProgramChange());
            _data[1] = a;
        }

        constexpr void setController(uint8_t a)
        {
            assert(isControlChange());
            expandLength(2);
            _data[1] = a;
        }

        constexpr void setControllerValue(uint8_t a)
        {
            assert(isControlChange());
            expandLength(3);
            _data[2] = a;
        }

        constexpr void setPitchBendValue(int16_t a)
        {
            assert(isPitchBend());
            int16_t b = a + 0x2000;
            _data[1]  = b & 0x7F;
            _data[2]  = (b >> 7) & 0x7F;
        }

        constexpr void setMetaNumber(uint8_t a)
        {
            assert(isMetaEvent());
            _data[1] = a;
        }

        constexpr void setupNoteOn(uint8_t channel, uint8_t note, uint8_t vel)
        {
            _data[0] = MIDIMsgStatus::NOTE_ON | (channel - 1);
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setupNoteOff(uint8_t channel, uint8_t note, uint8_t vel)
        {
            setLength(3);
            _data[0] = MIDIMsgStatus::NOTE_OFF | (channel - 1);
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setupPolyPressure(uint8_t channel, uint8_t note, uint8_t press)
        {
            setLength(3);
            _data[0] = MIDIMsgStatus::POLY_PRESSURE | (channel - 1);
            _data[1] = note;
            _data[2] = press;
        }

        constexpr void setupControlChange(uint8_t channel, uint8_t ctrl, uint8_t val)
        {
            setLength(3);
            _data[0] = MIDIMsgStatus::CONTROL_CHANGE | (channel - 1);
            _data[1] = ctrl;
            _data[2] = val;
        }

        constexpr void setupPan(uint8_t channel, double pan)
        {
            uint8_t r = std::floor(64 * (pan + 1));
            if (r > 127) {
                r = 127;
            }
            setupControlChange(channel, MIDICCNumber::PAN, r);
        }

        constexpr

            void
            setupProgramChange(uint8_t chan, uint8_t val)
        {
            setLength(2);
            _data[0] = MIDIMsgStatus::PROGRAM_CHANGE | (chan - 1);
            _data[1] = val;
        }

        constexpr void setupPitchBend(uint8_t chan, uint8_t lsb, uint8_t msb)
        {
            setLength(3);
            _data[0] = MIDIMsgStatus::PITCH_BEND | (chan - 1);
            _data[1] = lsb;
            _data[2] = msb;
        }

        constexpr void setupPitchBend(uint8_t chan, int16_t val)
        {
            setLength(3);
            _data[0] = MIDIMsgStatus::PITCH_BEND | (chan - 1);
            setPitchBendValue(val);
        }

        constexpr void setupAllNotesOff(uint8_t chan)
        {
            setupControlChange(chan, MIDICCNumber::ALL_NOTE_OFF, 127);
        }

        constexpr void setupAllSoundsOff(uint8_t chan)
        {
            setupControlChange(chan, MIDICCNumber::ALL_SOUND_OFF, 127);
        }

        /// \code
        /// FF type size args
        /// \endcode
        constexpr void setupMetaEvent(uint8_t type, std::initializer_list<uint8_t> args = {})
        {
            setLength(3 + args.size());
            _data[0] = MIDIMsgStatus::META_EVENT;
            _data[1] = type;
            _data[2] = args.size();
            if (args.size() > 0) {
                std::copy(args.begin(), args.end(), _data.begin() + 3);
            }
        }

        constexpr void setupRawTempo(uint16_t tempo)
        {
            uint8_t f;
            uint8_t s;
            uint8_t t;
            f = (tempo >> 16) & 0xFF;
            s = (tempo >> 8) & 0xFF;
            t = tempo & 0xFF;
            setupMetaEvent(MIDIMetaNumber::TEMPO, {f, s, t});
        }

        constexpr void setupBPM(uint16_t bpm)
        {
            setupRawTempo(60000000 / bpm);
        }

        constexpr void setupEndOfTrack()
        {
            setupMetaEvent(MIDIMetaNumber::END_OF_TRACK);
        }

        constexpr void setTimeSignature(uint8_t numerator, uint8_t denominator, uint8_t midi_tick_per_beat, uint8_t _32nd_per_midi_4th_note)
        {
            setupMetaEvent(MIDIMetaNumber::TIMESIG, {numerator, static_cast<uint8_t>(std::log2(denominator)), midi_tick_per_beat, _32nd_per_midi_4th_note});
        }

        constexpr void setKeySignature(int8_t sharp_flats, uint8_t major_minor)
        {
            setupMetaEvent(MIDIMetaNumber::KEYSIG, {static_cast<unsigned char>(sharp_flats), major_minor});
        }

        constexpr void setupMFBeatMarker()
        {
            clear();
            marker = MFMessageMark::BeatMarker;
        }

        /// \}

        /// \name

        [[nodiscard]] constexpr bool isMFMarker() const
        {
            return marker != MFMessageMark::None;
        }

        [[nodiscard]] constexpr bool isChannelMsg() const
        {
            return !isMFMarker() && (_data[0] >= MIDIMsgStatus::NOTE_OFF) && (_data[0] < MIDIMsgStatus::SYSEX_START);
        }

        [[nodiscard]] constexpr bool isNoteOn() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::NOTE_ON);
        }

        [[nodiscard]] constexpr bool isNoteOff() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isNoteOnV0() const
        {
            return isNoteOn() && (velocity() == 0);
        }

        [[nodiscard]] constexpr bool isNote() const
        {
            return isNoteOn() || isNoteOff();
        }

        [[nodiscard]] constexpr bool isImplicitNoteOn() const
        {
            return isNoteOn() && (velocity() != 0);
        }

        [[nodiscard]] constexpr bool isImplicitNoteOff() const
        {
            return isNoteOff() || isNoteOnV0();
        }

        [[nodiscard]] constexpr bool isPolyPressure() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::POLY_PRESSURE);
        }

        [[nodiscard]] constexpr bool isControlChange() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::CONTROL_CHANGE);
        }

        [[nodiscard]] constexpr bool isCCVolume() const
        {
            return isControlChange() && (controller() == MIDICCNumber::VOLUME);
        }

        [[nodiscard]] constexpr bool isCCSustainOn() const
        {
            return isControlChange() && (controller() == MIDICCNumber::SUSTAIN) && (controllerValue() >= 64);
        }

        [[nodiscard]] constexpr bool isCCSustainOff() const
        {
            return isControlChange() && (controller() == MIDICCNumber::SUSTAIN) && (controllerValue() < 64);
        }

        [[nodiscard]] constexpr bool isCCBalance() const
        {
            return isControlChange() && (controller() == MIDICCNumber::BALANCE);
        }

        [[nodiscard]] constexpr bool isCCPan() const
        {
            return isControlChange() && (controller() == MIDICCNumber::PAN);
        }

        [[nodiscard]] constexpr bool isProgramChange() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::PROGRAM_CHANGE);
        }

        [[nodiscard]] constexpr bool isChannelPressure() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::CHANNEL_PRESSURE);
        }

        [[nodiscard]] constexpr bool isPitchBend() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::PITCH_BEND);
        }

        [[nodiscard]] constexpr bool isSysEx() const
        {
            return !isMFMarker() && (type() == MIDIMsgStatus::SYSEX_START);
        }

        /// \brief Universal Real Time System Exclusive message
        /// \c F0 \c 7F ...
        [[nodiscard]] constexpr bool isSysExURT() const
        {
            return isSysEx() && (_data[1] == 0x7F);
        }

        [[nodiscard]] constexpr bool isMTC() const
        {
            return !isMFMarker() && (_data[0] == MIDIMsgStatus::MTC);
        }

        [[nodiscard]] constexpr bool isSongPosition() const
        {
            return !isMFMarker() && (_data[0] == MIDIMsgStatus::SONG_POSITION);
        }

        [[nodiscard]] constexpr bool isSongSelect() const
        {
            return !isMFMarker() && (_data[0] == MIDIMsgStatus::SONG_SELECT);
        }

        [[nodiscard]] constexpr bool isTuneRequest() const
        {
            return !isMFMarker() && (_data[0] == MIDIMsgStatus::TUNE_REQUEST);
        }

        [[nodiscard]] constexpr bool isMetaEvent() const
        {
            return !isMFMarker() && (_data[0] == MIDIMsgStatus::META_EVENT);
        }

        [[nodiscard]] constexpr bool isTextEvent() const
        {
            return isMetaEvent() && (_data[1] >= MIDIMetaNumber::GENERIC_TEXT) && (_data[1] <= MIDIMetaNumber::MARKER_TEXT);
        }

        [[nodiscard]] constexpr bool isAllNotesOff() const
        {
            return isControlChange() && (_data[1] == MIDICCNumber::ALL_NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isAllSoundsOff() const
        {
            return isControlChange() && (_data[1] == MIDICCNumber::ALL_SOUND_OFF);
        }

        [[nodiscard]] constexpr bool isMFNoOp() const
        {
            return (marker == MFMessageMark::NoOp);
        }

        [[nodiscard]] constexpr bool isChannelPrefix() const
        {
            return isMetaEvent() && (_data[1] == MIDIMetaNumber::CHANNEL_PREFIX);
        }

        [[nodiscard]] constexpr bool isTempo() const
        {
            return isMetaEvent() && (_data[1] == MIDIMetaNumber::TEMPO);
        }

        [[nodiscard]] constexpr bool isEndOfTrack() const
        {
            return isMetaEvent() && (_data[1] == MIDIMetaNumber::END_OF_TRACK);
        }

        [[nodiscard]] constexpr bool isTimeSignature() const
        {
            return isMetaEvent() && (_data[1] == MIDIMetaNumber::TIMESIG);
        }

        [[nodiscard]] constexpr bool isKeySignature() const
        {
            return isMetaEvent() && (_data[1] == MIDIMetaNumber::KEYSIG);
        }

        /// \warning Meta events are not system messages.
        [[nodiscard]] constexpr bool isSystemMessage() const
        {
            return !isMFMarker() && !isMetaEvent() && (_data[0] & 0xF0) == 0xF0;
        }

    protected:
        std::vector<uint8_t> _data;
        Utils::MFMessageMark marker = MFMessageMark::None;
    };

    namespace details {
        using MIDIUtils::MIDIClockTime;

        /// \brief MIDIMessage SysEx Extension
        template <class... Base>
        class MIDIMessageSysExExt : public Base... {
            // TODO: I do not know what should be here.
        };

        template <class Base>
        class MIDIMessageTimedExt : public Base {
        public:
            constexpr MIDIMessageTimedExt() noexcept  = default;
            constexpr ~MIDIMessageTimedExt() noexcept = default;

            constexpr MIDIMessageTimedExt(const MIDIMessageTimedExt& rhs) noexcept            = default;
            constexpr MIDIMessageTimedExt(MIDIMessageTimedExt&& rhs) noexcept                 = default;
            constexpr MIDIMessageTimedExt& operator=(const MIDIMessageTimedExt& rhs) noexcept = default;
            constexpr MIDIMessageTimedExt& operator=(MIDIMessageTimedExt&& rhs) noexcept      = default;

            constexpr explicit MIDIMessageTimedExt(const Base& rhs) noexcept
                : Base::_data(rhs._data)
                , Base::marker(rhs.marker)
            {
            }

            constexpr explicit MIDIMessageTimedExt(Base&& rhs) noexcept
                : Base::_data(std::exchange(rhs._data, {}))
                , Base::marker(rhs.marker) // trivially-copyable
            {
            }

            constexpr MIDIMessageTimedExt& operator=(const MIDIMessage& rhs) noexcept
            {
                *this = MIDIMessageTimedExt<Base>(rhs); // call copy ctor and move assign
            }

            constexpr MIDIMessageTimedExt& operator=(MIDIMessage&& rhs) noexcept
            {
                ~this();
                Base::_data  = std::exchange(rhs._data, {});
                Base::marker = rhs._data;
                dtime        = 0;
            }

            constexpr void clear()
            {
                Base::clear();
                dtime = 0;
            }

            [[nodiscard]] constexpr MIDIClockTime deltaTime() const noexcept
            {
                return dtime;
            }

            constexpr void setDeltaTime(MIDIClockTime t) noexcept
            {
                dtime = t;
            }

            constexpr auto operator<=>(const MIDIMessageTimedExt& rhs) const noexcept
            {
                return deltaTime() <=> rhs.deltaTime();
            }

        private:
            MIDIClockTime dtime{};
        };
    }

    using MIDITimedMessage = details::MIDIMessageTimedExt<MIDIMessage>;
}
