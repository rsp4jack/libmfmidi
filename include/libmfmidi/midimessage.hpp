/// \file midimessage.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDIMessage class

// NOLINTBEGIN(readability-identifier-length, bugprone-easily-swappable-parameters)

#pragma once

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4267 4146)
#endif

#include "libmfmidi/mfutils.hpp"
#include "libmfmidi/midiutils.hpp"
#include <cmath>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>
#include <vector>
#include <iomanip>
#include <sstream>

namespace libmfmidi {
    /// \brief Container for MIDI Message
    ///
    /// A container to store MIDI Messages. Contains a Container.
    ///
    /// All setters and getters will auto match message types.
    /// For example, setVelcoity will set byte2 in note event and poly pressure(aftertouch).
    /// and set byte1 in channel pressure.
    /// All getters and is-ers may assert message types.

    template <std::ranges::range Storage>
    class MIDIBasicMessage {
    private:
        using enum MIDIMsgStatus;

    public:
        /// \brief The default constructor
        constexpr explicit MIDIBasicMessage() noexcept = default;

        constexpr MIDIBasicMessage(std::initializer_list<uint8_t> elems) noexcept
            : _data(elems)
        {
        }

        constexpr explicit MIDIBasicMessage(Storage elems) noexcept
            : _data(std::move(elems))
        {
        }

        constexpr ~MIDIBasicMessage() noexcept = default;

        MF_DEFAULT_MOVE(MIDIBasicMessage);
        MF_DEFAULT_COPY(MIDIBasicMessage);

        using value_type             = typename Storage::value_type;
        using iterator               = typename Storage::iterator;
        using const_iterator         = typename Storage::const_iterator;
        using reverse_iterator       = typename Storage::reverse_iterator;
        using const_reverse_iterator = typename Storage::const_reverse_iterator;
        using size_type              = typename Storage::size_type;

        /// \brief Clear this message
        constexpr void clear() noexcept
        {
            _data.clear();
            marker = MFMessageMark::None;
        }

        /// \name Getters
        /// \{

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return M() && _data.empty();
        }

        [[nodiscard]] constexpr size_t size() const noexcept
        {
            return _data.size();
        }

        constexpr void push_back(uint8_t a) noexcept
        {
            _data.push_back(a);
        }

        constexpr void erase(const_iterator it) noexcept
        {
            _data.erase(std::move(it));
        }

        constexpr void erase(const_iterator beg, const_iterator edn) noexcept
        {
            _data.erase(std::move(beg), std::move(edn));
        }

        constexpr iterator begin() noexcept
        {
            return _data.begin();
        }

        constexpr iterator end() noexcept
        {
            return _data.end();
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept
        {
            return _data.cbegin();
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept
        {
            return _data.cend();
        }

        constexpr value_type& at(size_type idx) noexcept
        {
            return _data.at(idx);
        }

        constexpr value_type& operator[](size_type idx) noexcept
        {
            return _data[idx];
        }

        constexpr const value_type& operator[](size_type idx) const noexcept
        {
            return _data[idx];
        }

        [[nodiscard]] constexpr const value_type& at(size_type idx) const noexcept
        {
            return _data.at(idx);
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
                str << "MFMarker: " << static_cast<int>(marker) << endl;
            }
            if (isSysEx()) {
                str << "SysEx" << endl;
            }

            if (isChannelMsg()) {
                str << "Channel: " << static_cast<int>(channel()) << endl;
            }
            if (isTextEvent()) {
                str << "Text: " << textEventText() << endl;
            }

            str << "Full message:";
            for (const auto& a : _data) {
                str << ' ';
                str << hex << std::setfill('0') << std::setw(2) << static_cast<int>(a);
            }
            str << dec << endl;

            switch (type()) {
            case NOTE_ON: // We cant print a uint8_t like int, A cast needed.
                str << "Note On " << static_cast<int>(note()) << ' ' << static_cast<int>(velocity());
                break;
            case NOTE_OFF:
                str << "Note Off " << static_cast<int>(note()) << ' ' << static_cast<int>(velocity());
                break;
            case POLY_PRESSURE:
                str << "Poly Pressure " << static_cast<int>(note()) << ' ' << static_cast<int>(pressure());
                break;
            case CONTROL_CHANGE:
                str << "Control Change " << static_cast<int>(controller()) << ' ' << static_cast<int>(controllerValue());
                break;
            case PROGRAM_CHANGE:
                str << "Program Change " << static_cast<int>(programChangeValue());
                break;
            case CHANNEL_PRESSURE:
                str << "Channel Pressure " << static_cast<int>(pressure());
                break;
            case PITCH_BEND:
                str << "Pitch Bend " << pitchBendValue();
                break;
            }

            return str.str();
        }

        [[nodiscard]] std::string msgHex() const
        {
            return memoryDump(reinterpret_cast<const char*>(data().data()), size());
        }

        [[nodiscard]] constexpr size_t length() const
        {
            return size();
        }

        [[nodiscard]] constexpr const std::vector<uint8_t>& data() const
        {
            return _data;
        }

        [[nodiscard]] constexpr std::vector<uint8_t>& data()
        {
            return _data;
        }

        void resize(size_t size)
        {
            _data.resize(size);
        }

        void reserve(size_t size)
        {
            _data.reserve(size);
        }

        /// \warning May return negative
        /// \warning If {0xFF} is provided, \a MIDIMessage treats it as Reset. so it returns 1.
        [[nodiscard]] constexpr int expectedLength() const
        {
            if (isMetaEvent()) {
                return -1;
            }
            if (isSystemMessage()) {
                return getExpectedSMessageLength(_data[0]);
            }
            return getExpectedMessageLength(_data[0]);
        }

        [[nodiscard]] constexpr uint8_t status() const
        {
            return _data[0];
        }

        [[nodiscard]] constexpr uint8_t channel() const
        {
            C(isChannelMsg());
            return (_data[0] & 0x0FU) + 1;
        }

        /// \warning If not channel msg, return status(not &0xF0)
        [[nodiscard]] constexpr uint8_t type() const
        {
            if (isChannelMsg()) {
                return _data[0] & 0xF0U;
            }
            return _data[0];
        }

        [[nodiscard]] constexpr uint8_t note() const
        {
            C(isNote() || isPolyPressure());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t velocity() const
        {
            C(isNote());
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t pressure() const
        {
            C(isPolyPressure() || isChannelPressure());
            if (isPolyPressure()) {
                return _data[2];
            }
            if (isChannelPressure()) {
                return _data[1];
            }
            std::unreachable();
        }

        [[nodiscard]] constexpr uint8_t controller() const
        {
            C(isControlChange());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t controllerValue() const
        {
            C(isControlChange());
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t metaType() const
        {
            C(isMetaEvent());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t channelPressure() const
        {
            C(isChannelPressure());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t programChangeValue() const
        {
            C(isProgramChange());
            return _data[1];
        }

        /// \brief Get signed pitch bend value
        [[nodiscard]] constexpr int16_t pitchBendValue() const
        {
            C(isPitchBend());
            return (static_cast<int16_t>(_data[2] << 7U) | _data[1]) - 0x2000;
        }

        [[nodiscard]] constexpr uint8_t timeSigNumerator() const
        {
            C(isTimeSignature());
            return _data[3];
        }

        /// \brief Get \b Powered Denominator
        [[nodiscard]] uint8_t timeSigDenominator() const
        {
            C(isTimeSignature());
            return static_cast<uint8_t>(std::pow(2, _data[4]));
        }

        [[nodiscard]] constexpr uint8_t timeSigDenominatorRaw() const
        {
            C(isTimeSignature());
            return _data[4];
        }

        [[nodiscard]] constexpr int8_t keySigSharpFlats() const
        {
            C(isKeySignature());
            return static_cast<int8_t>(_data[3]); // In C++20
        }

        [[nodiscard]] constexpr bool keySigMajorMinor() const
        {
            C(isKeySignature());
            return _data[4] != 0U;
        }

        [[nodiscard]] constexpr double balancePan() const
        {
            C(isCCBalance() || isCCPan());
            int val = controllerValue();
            if (val == 127) {
                val = 128;
            }
            return (val - 64) / 64.0;
        }

        [[nodiscard]] constexpr MIDITempo tempo() const
        {
            C(isTempo());
            return MIDITempo::fromMSPQ(rawCat(_data[3], _data[4], _data[5]));
        }

        [[nodiscard]] constexpr std::string textEventText() const
        {
            C(isTextEvent());
            if (_data.size() > 2) {
                auto result = readVarNumIt(_data.cbegin() + 2, _data.cend());
                if (result.first == -1 && result.second == -1) {
                    return {};
                }
                return {_data.cbegin() + 2 + result.second, _data.cend()};
            }
            return "";
        }

        [[nodiscard]] constexpr uint8_t atIf(std::size_t pos, uint8_t normal = 0) const
        {
            if (pos >= size()) {
                return normal;
            }
            return at(pos);
        }

        // For later use
        /* #define rens(n)                           \
            [[nodiscard]] uint8_t byte##n() const \
            {                                     \
                return atIf(n);                   \
            } */

        [[nodiscard]] constexpr uint8_t byte0() const
        {
            return atIf(0);
        }

        [[nodiscard]] constexpr uint8_t byte1() const
        {
            return atIf(1);
        }

        [[nodiscard]] constexpr uint8_t byte2() const
        {
            return atIf(2);
        }

        [[nodiscard]] constexpr uint8_t byte3() const
        {
            return atIf(3);
        }

        [[nodiscard]] constexpr uint8_t byte4() const
        {
            return atIf(4);
        }

        [[nodiscard]] constexpr uint8_t byte5() const
        {
            return atIf(5);
        }

        [[nodiscard]] constexpr uint8_t byte6() const
        {
            return atIf(6);
        }

        [[nodiscard]] constexpr uint8_t byte7() const
        {
            return atIf(7);
        }

        [[nodiscard]] constexpr uint8_t byte8() const
        {
            return atIf(8);
        }

        [[nodiscard]] constexpr uint8_t byte9() const
        {
            return atIf(9);
        }

        [[nodiscard]] constexpr uint8_t byte10() const
        {
            return atIf(10);
        }

        [[nodiscard]] constexpr uint8_t byte11() const
        {
            return atIf(11);
        }

        [[nodiscard]] constexpr uint8_t byte12() const
        {
            return atIf(12);
        }

        [[nodiscard]] constexpr uint8_t byte13() const
        {
            return atIf(13);
        }

        [[nodiscard]] constexpr uint8_t byte14() const
        {
            return atIf(14);
        }

        [[nodiscard]] constexpr uint8_t byte15() const
        {
            return atIf(15);
        }

        [[nodiscard]] constexpr uint8_t byte16() const
        {
            return atIf(16);
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
            C(a <= 16);
            _data[0] = (_data[0] & 0xF0) | (a - 1);
        }

        /// \warning \a a must be 0x40, 0xF0, 0x70, etc...
        /// The least significant 4 bit will be ignored.
        constexpr void setTyoe(uint8_t a)
        {
            expandLength(1);
            C((a >> 1) <= 0xF);
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
            C(isNote() || isPolyPressure());
            _data[1] = a;
        }

        constexpr void setVelocity(uint8_t a)
        {
            C(isNote() || isPolyPressure() || isChannelPressure());
            if (isNote() || isPolyPressure()) {
                _data[2] = a;
            } else if (isChannelPressure()) {
                _data[1] = a;
            }
        }

        constexpr void setProgramChangeValue(uint8_t a)
        {
            C(isProgramChange());
            _data[1] = a;
        }

        constexpr void setController(uint8_t a)
        {
            C(isControlChange());
            expandLength(2);
            _data[1] = a;
        }

        constexpr void setControllerValue(uint8_t a)
        {
            C(isControlChange());
            expandLength(3);
            _data[2] = a;
        }

        constexpr void setPitchBendValue(int16_t a)
        {
            C(isPitchBend());
            const int16_t b = a + 0x2000;
            _data[1]        = b & 0x7F;
            _data[2]        = (b >> 7) & 0x7F;
        }

        constexpr void setMetaNumber(uint8_t a)
        {
            C(isMetaEvent());
            _data[1] = a;
        }

        constexpr void setupNoteOn(uint8_t channel, uint8_t note, uint8_t vel)
        {
            _data[0] = NOTE_ON | (channel - 1);
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setupNoteOff(uint8_t channel, uint8_t note, uint8_t vel)
        {
            setLength(3);
            _data[0] = NOTE_OFF | (channel - 1);
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setupPolyPressure(uint8_t channel, uint8_t note, uint8_t press)
        {
            setLength(3);
            _data[0] = POLY_PRESSURE | (channel - 1);
            _data[1] = note;
            _data[2] = press;
        }

        constexpr void setupControlChange(uint8_t channel, uint8_t ctrl, uint8_t val)
        {
            setLength(3);
            _data[0] = CONTROL_CHANGE | (channel - 1);
            _data[1] = ctrl;
            _data[2] = val;
        }

        /// \a pan : -1.0 ~ 1.0
        void setupPan(uint8_t channel, double pan)
        {
            auto r = static_cast<uint8_t>(std::floor(64 * (pan + 1)));
            if (r > 127) {
                r = 127;
            }
            setupControlChange(channel, MIDICCNumber::PAN, r);
        }

        constexpr void setupProgramChange(uint8_t chan, uint8_t val)
        {
            setLength(2);
            _data[0] = PROGRAM_CHANGE | (chan - 1);
            _data[1] = val;
        }

        constexpr void setupPitchBend(uint8_t chan, uint8_t lsb, uint8_t msb)
        {
            setLength(3);
            _data[0] = PITCH_BEND | (chan - 1);
            _data[1] = lsb;
            _data[2] = msb;
        }

        constexpr void setupPitchBend(uint8_t chan, int16_t val)
        {
            setLength(3);
            _data[0] = PITCH_BEND | (chan - 1);
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
            // sh*tty code
            clear();
            _data = {MIDINumSpace::META_EVENT, type};
            writeVarNumIt(static_cast<MIDIVarNum>(args.size()), std::back_inserter(_data));
            std::copy(args.begin(), args.end(), std::back_inserter(_data));
        }

        constexpr void setupTempo(MIDITempo tempo)
        {
            uint8_t f;
            uint8_t s;
            uint8_t t;
            f = (tempo.mspq() >> 16) & 0xFF;
            s = (tempo.mspq() >> 8) & 0xFF;
            t = tempo.mspq() & 0xFF;
            setupMetaEvent(MIDIMetaNumber::TEMPO, {f, s, t});
        }


        constexpr void setupEndOfTrack()
        {
            setupMetaEvent(MIDIMetaNumber::END_OF_TRACK);
        }

        constexpr void setupTimeSignature(uint8_t numerator, uint8_t denominator, uint8_t midi_tick_per_beat, uint8_t _32nd_per_midi_4th_note)
        {
            setupMetaEvent(MIDIMetaNumber::TIMESIG, {numerator, static_cast<uint8_t>(std::log2(denominator)), midi_tick_per_beat, _32nd_per_midi_4th_note});
        }

        constexpr void setupKeySignature(int8_t sharp_flats, uint8_t major_minor)
        {
            setupMetaEvent(MIDIMetaNumber::KEYSIG, {static_cast<unsigned char>(sharp_flats), major_minor});
        }

        template <std::convertible_to<uint8_t>... T>
        constexpr void setupMFMarker(MFMessageMark mark, T... init)
        {
            _data  = {init...};
            marker = mark;
        }

        /// \}

        /// \name is-er
        /// \{

        [[nodiscard]] constexpr bool strictVaild() const
        {
            // s**t code but fun
            // clang-format off
            return isMFMarker() || (!_data.empty() && (
                (isVoiceMessage() && getExpectedMessageLength(status()) == _data.size()) // voice message: expected length
                || (isSystemMessage() && ( // system message
                    (isSysEx() && *(_data.end()-1) == SYSEX_END) // sysex: have sysex_start and sysex_end
                    || ((status() >= 0xF1 && status() <= 0xF1) && getExpectedSMessageLength(status()) == _data.size()) // some system message: expected length, sometimes LUT return 0, so wont match and get false
                ))
                || (isMetaEvent() && _data.size() >= 3 && isMetaVaild()) // meta msg
            ));
            // clang-format on
        }

        [[nodiscard]] constexpr bool isMFMarker() const
        {
            return marker != MFMessageMark::None;
        }

        [[nodiscard]] constexpr bool isVoiceMessage() const
        {
            return isChannelMsg();
        }

        [[nodiscard]] constexpr bool isChannelPrefix() const
        {
            return isMetaEvent() && L(2) && (_data[1] == MIDIMetaNumber::CHANNEL_PREFIX);
        }

        /// \warning Meta events are not system messages.
        [[nodiscard]] constexpr bool isSystemMessage() const
        {
            return !isMetaEvent() && (status() & 0xF0) == 0xF0;
        }

        [[nodiscard]] constexpr bool isChannelMsg() const
        {
            return M() && L(1) && (status() >= NOTE_OFF) && (status() < SYSEX_START);
        }

        [[nodiscard]] constexpr bool isNoteOn() const
        {
            return M() && L(1) && (type() == NOTE_ON);
        }

        [[nodiscard]] constexpr bool isNoteOff() const
        {
            return M() && L(1) && (type() == NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isPolyPressure() const
        {
            return M() && L(1) && (type() == POLY_PRESSURE);
        }

        [[nodiscard]] constexpr bool isControlChange() const
        {
            return M() && L(1) && (type() == CONTROL_CHANGE);
        }

        [[nodiscard]] constexpr bool isProgramChange() const
        {
            return M() && L(1) && (type() == PROGRAM_CHANGE);
        }

        [[nodiscard]] constexpr bool isChannelPressure() const
        {
            return M() && L(1) && (type() == CHANNEL_PRESSURE);
        }

        [[nodiscard]] constexpr bool isPitchBend() const
        {
            return M() && L(1) && (type() == PITCH_BEND);
        }

        [[nodiscard]] constexpr bool isSysEx() const
        {
            return M() && L(1) && (status() == SYSEX_START);
        }

        [[nodiscard]] constexpr bool isMTC() const
        {
            return M() && L(1) && (status() == MTC);
        }

        [[nodiscard]] constexpr bool isSongPosition() const
        {
            return M() && L(1) && (status() == SONG_POSITION);
        }

        [[nodiscard]] constexpr bool isSongSelect() const
        {
            return M() && L(1) && (status() == SONG_SELECT);
        }

        [[nodiscard]] constexpr bool isTuneRequest() const
        {
            return M() && L(1) && (status() == TUNE_REQUEST);
        }

        // TODO: Reset or Meta?
        /// \warning Currently only {0xFF} is reset.
        [[nodiscard]] constexpr bool isMetaEvent() const
        {
            return M() && L(2) && (status() == META_EVENT);
        }

        [[nodiscard]] constexpr bool isReset() const
        {
            return M() && (size() == 1) && (status() == RESET);
        }

        /// \brief Universal Real Time System Exclusive message
        /// \c F0 \c 7F ...
        [[nodiscard]] constexpr bool isSysExURT() const
        {
            return isSysEx() && L(2) && (_data[1] == 0x7F);
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

        [[nodiscard]] constexpr bool isTextEvent() const
        {
            return isMetaEvent() && L(2) && (_data[1] >= MIDIMetaNumber::GENERIC_TEXT) && (_data[1] <= MIDIMetaNumber::MARKER_TEXT);
        }

        [[nodiscard]] constexpr bool isAllNotesOff() const
        {
            return isControlChange() && L(2) && (_data[1] == MIDICCNumber::ALL_NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isAllSoundsOff() const
        {
            return isControlChange() && L(2) && (_data[1] == MIDICCNumber::ALL_SOUND_OFF);
        }

        [[nodiscard]] constexpr bool isMFNoOp() const
        {
            return (marker == MFMessageMark::NoOp);
        }

        [[nodiscard]] constexpr bool isTempo() const
        {
            return isMetaEvent() && L(2) && (_data[1] == MIDIMetaNumber::TEMPO);
        }

        [[nodiscard]] constexpr bool isEndOfTrack() const
        {
            return isMetaEvent() && L(2) && (_data[1] == MIDIMetaNumber::END_OF_TRACK);
        }

        [[nodiscard]] constexpr bool isTimeSignature() const
        {
            return isMetaEvent() && L(2) && (_data[1] == MIDIMetaNumber::TIMESIG);
        }

        [[nodiscard]] constexpr bool isKeySignature() const
        {
            return isMetaEvent() && L(2) && (_data[1] == MIDIMetaNumber::KEYSIG);
        }

    protected:
        std::vector<uint8_t> _data; // i lov qt mplicit iharing
        MFMessageMark        marker = MFMessageMark::None;

        // not mf mark
        [[nodiscard]] constexpr inline bool M() const
        {
            return !isMFMarker();
        }

        // size>=
        [[nodiscard]] constexpr inline bool L(size_t len) const
        {
            return _data.size() >= len;
        }

        // internal C
        static constexpr inline void C(bool cond)
        {
            if (!cond) {
                throw std::logic_error("MIDIMessage: assert failed");
            }
        }

        [[nodiscard]] constexpr bool isMetaVaild() const
        {
            // ff type len ddddddddddddddd
            if (!isMetaEvent() || _data.size() < 3) {
                return false;
            }
            const int explen = getExpectedMetaLength(metaType());
            auto      result = readVarNumIt(_data.cbegin() + 2, _data.cend());
            if (result.first == -1U && result.second == -1U) {
                // "META: Reading length: Invalid variable number";
                return false; // too early eof
            }
            if (_data.size() != 2U + result.first + result.second) { // uncorrect msg len
                // "META: Length check: Invalid message length";
                return false;
            }
            if (explen < 0) { // var length and unspec meta type
                return true;
            }
            if (static_cast<unsigned int>(explen) != result.first + 3U) { // uncorrect meta length
                // "META: Length check: invalid meta length (not match spec)";
                return false;
            }
            switch (metaType()) {
            case MIDIMetaNumber::CHANNEL_PREFIX:
                if (_data[3] > 15) {
                    // "META: Channel Prefix: > 15";
                    return false;
                }
                break;
            case MIDIMetaNumSpace::KEYSIG:
                if (_data[4] > 1) {
                    // "META: Key Signature: Not major or minor (mi > 1)";
                    return false;
                }
                break;
            }
            return true;
        }

        /// \}
    };

    using MIDIMessage = MIDIBasicMessage<std::vector<uint8_t>>;

    namespace details {
        template <class Base>
        class MIDIMessageTimedExt : public Base {
        public:
            constexpr MIDIMessageTimedExt() noexcept  = default;
            constexpr ~MIDIMessageTimedExt() noexcept = default;

            constexpr MIDIMessageTimedExt(std::initializer_list<uint8_t> args) noexcept
                : Base(args)
            {
            }

            MF_DEFAULT_COPY(MIDIMessageTimedExt);
            MF_DEFAULT_MOVE(MIDIMessageTimedExt);

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

            constexpr MIDIMessageTimedExt& operator=(const Base& rhs) noexcept
            {
                *this = MIDIMessageTimedExt<Base>(rhs); // call copy ctor and move assign
            }

            constexpr MIDIMessageTimedExt& operator=(Base&& rhs) noexcept
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

    template <class T>
    using MIDIBasicTimedMessage = details::MIDIMessageTimedExt<MIDIBasicMessage<T>>;

    using MIDITimedMessage = MIDIBasicTimedMessage<std::vector<uint8_t>>;
}

// NOLINTEND(readability-identifier-length, bugprone-easily-swappable-parameters)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
