/*
 * This file is a part of mfmidi.
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

// NOLINTBEGIN(readability-identifier-length, bugprone-easily-swappable-parameters)

#pragma once

#include "mfmidi/mfutility.hpp"
#include "mfmidi/midi_tempo.hpp"
#include "mfmidi/midi_utility.hpp"
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4267 4146)
#endif

namespace mfmidi {
    // not even a view
    template <std::ranges::range R>
        requires std::movable<R>
    class midi_message_owning_view : public std::ranges::view_interface<midi_message_owning_view<R>> {
    protected:
        using enum MIDIMsgStatus;
        R _data;

    public:
        constexpr explicit midi_message_owning_view() = default;
        constexpr midi_message_owning_view(const midi_message_owning_view&)
            requires std::copyable<R>
        = default;
        constexpr midi_message_owning_view(midi_message_owning_view&&) = default;

        constexpr explicit midi_message_owning_view(R&& elems)
            : _data(std::move(elems))
        {}

        template <class... T>
        constexpr explicit midi_message_owning_view(std::piecewise_construct_t, T&&... args)
            : _data(std::forward<T>(args)...)
        {}

        constexpr ~midi_message_owning_view() = default;

        constexpr midi_message_owning_view& operator=(const midi_message_owning_view&)
            requires std::copyable<R>
        = default;
        constexpr midi_message_owning_view& operator=(midi_message_owning_view&&) = default;

        [[nodiscard]] constexpr auto&& base(this auto&& self) noexcept
        {
            return std::forward<decltype(self)>(self)._data;
        }

        [[nodiscard]] constexpr auto begin(this auto&& self)
            requires std::ranges::range<decltype(self._data)>
        {
            return std::ranges::begin(self._data);
        }

        [[nodiscard]] constexpr auto end(this auto&& self)
            requires std::ranges::range<decltype(self._data)>
        {
            return std::ranges::end(self._data);
        }

        [[nodiscard]] constexpr bool empty(this auto&& self)
            requires requires { std::ranges::empty(_data); }
        {
            return std::ranges::empty(self._data);
        }

        [[nodiscard]] constexpr auto size(this auto&& self)
            requires std::ranges::sized_range<decltype(self._data)>
        {
            return std::ranges::size(self._data);
        }

        [[nodiscard]] constexpr auto data(this auto&& self)
            requires std::ranges::contiguous_range<decltype(self._data)>
        {
            return std::ranges::data(self._data);
        }

        [[nodiscard]] constexpr int deduced_expected_length() const
        {
            if (is_meta_event_like()) {
                return expected_meta_event_length(_data[1]);
            }
            if (is_system_message()) {
                return expected_system_message_length(_data[0]);
            }
            return expected_channel_message_length(_data[0]);
        }

        [[nodiscard]] constexpr uint8_t status() const
        {
            C(!empty());
            return _data[0];
        }

        [[nodiscard]] constexpr uint8_t channel() const
        {
            C(is_channel_msg());
            return (_data[0] & 0x0FU) + 1;
        }

        [[nodiscard]] constexpr uint8_t type() const
        {
            if (is_channel_msg()) {
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

        [[nodiscard]] constexpr uint8_t controller_value() const
        {
            C(isControlChange());
            return _data[2];
        }

        [[nodiscard]] constexpr uint8_t meta_type() const
        {
            C(is_meta_event_like());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t channel_pressure() const
        {
            C(isChannelPressure());
            return _data[1];
        }

        [[nodiscard]] constexpr uint8_t program() const
        {
            C(isProgramChange());
            return _data[1];
        }

        /// \brief Get signed pitch bend value
        [[nodiscard]] constexpr int16_t pitch_bend() const
        {
            C(isPitchBend());
            return static_cast<int16_t>(_data[2] << 7U | _data[1]) - 0x2000;
        }

        [[nodiscard]] constexpr uint8_t time_signature_numerator() const
        {
            C(isTimeSignature());
            return _data[3];
        }

        /// \brief Get \b Powered Denominator
        [[nodiscard]] unsigned time_signature_denominator() const
        {
            C(isTimeSignature());
            return 1U << _data[4];
        }

        [[nodiscard]] constexpr uint8_t time_signature_denominator_raw() const
        {
            C(isTimeSignature());
            return _data[4];
        }

        [[nodiscard]] constexpr int8_t key_signature_pitch() const
        {
            C(isKeySignature());
            return static_cast<int8_t>(_data[3]); // In C++20
        }

        [[nodiscard]] constexpr bool key_signature_is_minor() const
        {
            C(isKeySignature());
            return _data[4] != 0U;
        }

        [[nodiscard]] constexpr bool key_signature_is_major() const
        {
            C(isKeySignature());
            return _data[4] == 0U;
        }

        [[nodiscard]] constexpr double position() const
        {
            C(isCCBalance() || isCCPan());
            int val = controller_value();
            if (val == 127) {
                val = 128;
            }
            return (val - 64) / 64.0;
        }

        [[nodiscard]] constexpr tempo tempo() const
        {
            C(isTempo());
            return tempo::from_mspq(rawcat(_data[3], _data[4], _data[5]));
        }

        [[nodiscard]] constexpr std::string_view text() const
        {
            C(isTextEvent());
            auto result = read_smf_variable_length_number(_data | std::views::drop(2));
            return {result.it, std::ranges::cend(_data)};
        }

        /// \}

        /// \name Setter
        /// \{

        constexpr void set_channel(uint8_t a)
        {
            C(is_channel_msg());
            _data[0] = (_data[0] & 0xF0) | (a - 1);
        }

        constexpr void set_type(uint8_t a)
        {
            if (is_channel_msg()) {
                _data[0] = (_data[0] & 0x0F) | (a & 0xF0);
            }
            _data[0] = a;
        }

        constexpr void set_note(uint8_t a)
        {
            C(isNote() || isPolyPressure());
            _data[1] = a;
        }

        constexpr void set_velocity(uint8_t a)
        {
            C(isNote() || isPolyPressure() || isChannelPressure());
            if (isNote() || isPolyPressure()) {
                _data[2] = a;
            } else if (isChannelPressure()) {
                _data[1] = a;
            }
        }

        constexpr void set_program(uint8_t a)
        {
            C(isProgramChange());
            _data[1] = a;
        }

        constexpr void set_controller(uint8_t a)
        {
            C(isControlChange());
            _data[1] = a;
        }

        constexpr void set_controller_value(uint8_t a)
        {
            C(isControlChange());
            _data[2] = a;
        }

        constexpr void set_pitch_bend(int16_t a)
        {
            C(isPitchBend());
            const int16_t b = a + 0x2000;
            _data[1]        = b & 0x7F;
            _data[2]        = (b >> 7) & 0x7F;
        }

        constexpr void set_meta_type(uint8_t a)
        {
            C(is_meta_event_like());
            _data[1] = a;
        }

        constexpr void guarantee_length(size_t len)
        {
            if constexpr (requires { _data.resize(len); }) {
                _data.resize(len);
            } else {
                assert(size() == len);
            }
        }

        constexpr void setup_note_on(uint8_t channel, uint8_t note, uint8_t vel)
        {
            guarantee_length(3);
            _data[0] = NOTE_ON | channel;
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setup_note_off(uint8_t channel, uint8_t note, uint8_t vel)
        {
            guarantee_length(3);
            _data[0] = NOTE_OFF | channel;
            _data[1] = note;
            _data[2] = vel;
        }

        constexpr void setup_poly_pressure(uint8_t channel, uint8_t note, uint8_t press)
        {
            guarantee_length(3);
            _data[0] = POLY_PRESSURE | channel;
            _data[1] = note;
            _data[2] = press;
        }

        constexpr void setup_control_change(uint8_t channel, uint8_t ctrl, uint8_t val)
        {
            guarantee_length(3);
            _data[0] = CONTROL_CHANGE | channel;
            _data[1] = ctrl;
            _data[2] = val;
        }

        /// \a pan : -1.0 ~ 1.0
        void setup_pan(uint8_t channel, double pan)
        {
            auto r = static_cast<uint8_t>(std::floor(64 * (pan + 1)));
            r      = std::min<int>(r, 127);
            setup_pan(channel, r);
        }

        void setup_pan(uint8_t channel, uint8_t position)
        {
            setup_control_change(channel, MIDICCNumber::PAN, position);
        }

        constexpr void setup_program_change(uint8_t chan, uint8_t val)
        {
            guarantee_length(2);
            _data[0] = PROGRAM_CHANGE | chan;
            _data[1] = val;
        }

        constexpr void setup_pitch_bend(uint8_t chan, uint8_t lsb, uint8_t msb)
        {
            guarantee_length(3);
            _data[0] = PITCH_BEND | chan;
            _data[1] = lsb;
            _data[2] = msb;
        }

        constexpr void setup_pitch_bend(uint8_t chan, int16_t val)
        {
            guarantee_length(3);
            _data[0] = PITCH_BEND | chan;
            set_pitch_bend(val);
        }

        constexpr void setup_all_notes_off(uint8_t chan)
        {
            setup_control_change(chan, MIDICCNumber::ALL_NOTE_OFF, 0);
        }

        constexpr void setup_all_sounds_off(uint8_t chan)
        {
            setup_control_change(chan, MIDICCNumber::ALL_SOUND_OFF, 0);
        }

        /// \code
        /// FF type size args
        /// \endcode
        // constexpr void setup_meta_event(uint8_t type, std::initializer_list<std::ranges::range_value_t<R>> args = {})
        //     requires requires(std::ranges::range_value_t<R> val) {_data.clear(); _data.push_back(val); }
        // {
        //     _data.clear();
        //     _data = {MIDINumSpace::META_EVENT, type};
        //     std::ranges::copy(smf_variable_length_number_view<size_t, std::ranges::range_value_t<R>>{args.size()}, std::back_inserter(_data));
        //     std::ranges::copy(args, std::back_inserter(_data));
        // }
        //
        // constexpr void setup_tempo(mfmidi::tempo tempo)
        // {
        //     uint8_t f;
        //     uint8_t s;
        //     uint8_t t;
        //     f = (tempo.mspq() >> 16) & 0xFF;
        //     s = (tempo.mspq() >> 8) & 0xFF;
        //     t = tempo.mspq() & 0xFF;
        //     setup_meta_event(MIDIMetaNumber::TEMPO, {f, s, t});
        // }
        //
        // constexpr void setup_end_of_track()
        // {
        //     setup_meta_event(MIDIMetaNumber::END_OF_TRACK);
        // }
        //
        // constexpr void setup_time_signature(uint8_t numerator, uint8_t denominator, uint8_t midi_tick_per_beat, uint8_t _32nd_per_midi_4th_note)
        // {
        //     setup_meta_event(MIDIMetaNumber::TIMESIG, {numerator, static_cast<uint8_t>(std::log2(denominator)), midi_tick_per_beat, _32nd_per_midi_4th_note});
        // }
        //
        // constexpr void setup_key_signature(int8_t sharp_flats, uint8_t major_minor)
        // {
        //     setup_meta_event(MIDIMetaNumber::KEYSIG, {static_cast<uint8_t>(sharp_flats), major_minor});
        // }

        /// \}

        /// \name is-er
        /// \{

        [[nodiscard]] constexpr bool strict_vaild() const
        {
            // clang-format off
            return (!_data.empty() && (
                (is_voice_message() && expected_channel_message_length(status()) == _data.size()) // voice message: expected length
                || (is_system_message() && ( // system message
                    (isSysEx() && *(_data.end()-1) == SYSEX_END) // sysex: have sysex_start and sysex_end
                    || ((status() >= 0xF1 && status() <= 0xF1) && expected_system_message_length(status()) == _data.size()) // some system message: expected length, sometimes LUT return 0, so wont match and get false
                ))
                || (is_meta_event_like() && _data.size() >= 3 && isMetaVaild()) // meta msg
            ));
            // clang-format on
        }

        [[nodiscard]] constexpr bool is_voice_message() const
        {
            return is_channel_msg();
        }

        [[nodiscard]] constexpr bool is_channel_prefix() const
        {
            return is_meta_event_like() && L(2) && (_data[1] == MIDIMetaNumber::CHANNEL_PREFIX);
        }

        /// \warning Meta events are not system messages.
        [[nodiscard]] constexpr bool is_system_message() const
        {
            return !is_meta_event_like() && (status() & 0xF0) == 0xF0;
        }

        [[nodiscard]] constexpr bool is_channel_msg() const
        {
            return L(1) && (status() >= NOTE_OFF) && (status() < SYSEX_START);
        }

        [[nodiscard]] constexpr bool isNoteOn() const
        {
            return L(1) && (type() == NOTE_ON);
        }

        [[nodiscard]] constexpr bool isNoteOff() const
        {
            return L(1) && (type() == NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isPolyPressure() const
        {
            return L(1) && (type() == POLY_PRESSURE);
        }

        [[nodiscard]] constexpr bool isControlChange() const
        {
            return L(1) && (type() == CONTROL_CHANGE);
        }

        [[nodiscard]] constexpr bool isProgramChange() const
        {
            return L(1) && (type() == PROGRAM_CHANGE);
        }

        [[nodiscard]] constexpr bool isChannelPressure() const
        {
            return L(1) && (type() == CHANNEL_PRESSURE);
        }

        [[nodiscard]] constexpr bool isPitchBend() const
        {
            return L(1) && (type() == PITCH_BEND);
        }

        [[nodiscard]] constexpr bool isSysEx() const
        {
            return L(1) && (status() == SYSEX_START);
        }

        [[nodiscard]] constexpr bool isMTC() const
        {
            return L(1) && (status() == MTC);
        }

        [[nodiscard]] constexpr bool isSongPosition() const
        {
            return L(1) && (status() == SONG_POSITION);
        }

        [[nodiscard]] constexpr bool isSongSelect() const
        {
            return L(1) && (status() == SONG_SELECT);
        }

        [[nodiscard]] constexpr bool isTuneRequest() const
        {
            return L(1) && (status() == TUNE_REQUEST);
        }

        // only {0xFF} is reset.
        // meta: 0xFF, 0xnn (type), <len> (variable length)
        [[nodiscard]] constexpr bool is_meta_event_like() const
        {
            return L(3) && (status() == META_EVENT);
        }

        [[nodiscard]] constexpr bool isReset() const
        {
            return (size() == 1) && (status() == RESET);
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
            return isControlChange() && (controller() == MIDICCNumber::SUSTAIN) && (controller_value() >= 64);
        }

        [[nodiscard]] constexpr bool isCCSustainOff() const
        {
            return isControlChange() && (controller() == MIDICCNumber::SUSTAIN) && (controller_value() < 64);
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
            return is_meta_event_like() && L(3) && (_data[1] >= MIDIMetaNumber::GENERIC_TEXT) && (_data[1] <= MIDIMetaNumber::MARKER_TEXT);
        }

        [[nodiscard]] constexpr bool isAllNotesOff() const
        {
            return isControlChange() && L(2) && (_data[1] == MIDICCNumber::ALL_NOTE_OFF);
        }

        [[nodiscard]] constexpr bool isAllSoundsOff() const
        {
            return isControlChange() && L(2) && (_data[1] == MIDICCNumber::ALL_SOUND_OFF);
        }

        [[nodiscard]] constexpr bool isTempo() const
        {
            return is_meta_event_like() && L(2) && (_data[1] == MIDIMetaNumber::TEMPO);
        }

        [[nodiscard]] constexpr bool isEndOfTrack() const
        {
            return is_meta_event_like() && L(2) && (_data[1] == MIDIMetaNumber::END_OF_TRACK);
        }

        [[nodiscard]] constexpr bool isTimeSignature() const
        {
            return is_meta_event_like() && L(2) && (_data[1] == MIDIMetaNumber::TIMESIG);
        }

        [[nodiscard]] constexpr bool isKeySignature() const
        {
            return is_meta_event_like() && L(2) && (_data[1] == MIDIMetaNumber::KEYSIG);
        }

    protected:
        // size>=
        [[nodiscard]] constexpr bool L(size_t len) const
        {
            return _data.size() >= len;
        }

        // assert
        static constexpr void C(bool cond)
        {
            assert(cond);
        }

        [[nodiscard]] constexpr bool isMetaVaild() const
        {
            // ff type len ddddddddddddddd
            if (!is_meta_event_like() || _data.size() < 3) {
                return false;
            }
            const int explen = expected_meta_event_length(meta_type());
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
            switch (meta_type()) {
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

    using MIDIMessage = midi_message_owning_view<std::vector<uint8_t>>;

    namespace details {
        template <class Base>
        class MIDIMessageTimedExt : public Base {
        public:
            constexpr MIDIMessageTimedExt() noexcept  = default;
            constexpr ~MIDIMessageTimedExt() noexcept = default;

            template <class... T>
            constexpr explicit MIDIMessageTimedExt(MIDIClockTime delta, T&&... args) noexcept
                : Base(std::forward<T>(args)...)
                , dtime(delta)
            {
            }

            MF_DEFAULT_COPY(MIDIMessageTimedExt);
            MF_DEFAULT_MOVE(MIDIMessageTimedExt);

            constexpr explicit MIDIMessageTimedExt(const Base& rhs) noexcept
                : Base(rhs)
            {
            }

            constexpr explicit MIDIMessageTimedExt(Base&& rhs) noexcept
                : Base(std::move(rhs))
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
    using MIDIBasicTimedMessage = details::MIDIMessageTimedExt<midi_message_owning_view<T>>;

    using MIDITimedMessage = MIDIBasicTimedMessage<std::vector<uint8_t>>;

    template <class T>
    concept midi_message_alike = std::ranges::range<T> && sizeof(std::ranges::range_value_t<T>) * CHAR_BIT == 8;
}

template <class T>
constexpr bool std::ranges::enable_borrowed_range<mfmidi::midi_message_owning_view<T>> = enable_borrowed_range<T>;

// NOLINTEND(readability-identifier-length, bugprone-easily-swappable-parameters)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
