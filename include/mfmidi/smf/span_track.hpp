/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// \file span_track.hpp
/// \brief Read on Play

#pragma once

#include "mfmidi/midi_message.hpp"
#include "mfmidi/midi_ranges.hpp"
#include "mfmidi/smf/smf.hpp"
#include "mfmidi/smf/smf_error.hpp"
#include "mfmidi/smf/variable_number.hpp"
#include <ranges>
#include <span>
#include <tuple>

namespace mfmidi {
    using foreign_midi_message = MIDIBasicTimedMessage<foreign_vector<const uint8_t>>;

    class span_track {
    public:
        using base_type  = std::span<const uint8_t>;
        using value_type = const foreign_midi_message;

        class iterator {
            friend span_track;
            using enum MIDIMsgStatus;

            // Current message
            const uint8_t* _begin      = nullptr;
            size_t         _len        = 0;
            uint_midi_time _delta_time = 0;
            bool           _running_status{};

            // SMF reader
            uint8_t _status = 0; // midi status, for running status

            // Stream
            const uint8_t* _current = nullptr;

            base_type _base{};

        public:
            using difference_type = std::ptrdiff_t;
            using value_type      = const foreign_midi_message;

            iterator() noexcept = default;

            foreign_midi_message operator*() const
            {
                assert(_begin != nullptr);
                foreign_midi_message event;
                event.set_delta_time(_delta_time);
                if (_running_status) {
                    foreign_midi_message::base_type::own_type own;
                    own.reserve(_len);
                    own.push_back(_status);
                    own.append(_begin, _len - 1);
                    event.base() = std::move(own);
                } else {
                    event.base() = base_type{_begin, _len};
                }
                return event;
            }

            iterator& operator++() &
            {
                using enum smf_errc;
                assert(_current != nullptr && !_base.empty() && _current <= &_base.back());
                // Read a event
                std::tie(_delta_time, std::ignore) = readVarNum();

                _begin = _current;

                // status
                uint8_t data    = readU8();
                _running_status = data < 0x80;
                if (_running_status) {
                    if (_status == 0) {
                        throw smf_error(error_running_status);
                    }
                    --_current;
                } else {
                    _status = data;
                }

                if ((_status >= NOTE_OFF) && (_status < SYSEX_START)) { // also test if status is vaild
                    _len = expected_channel_message_length(_status);
                    _current += _len - 1;
                } else {
                    switch (_status) {
                    case META_EVENT: {
                        // this is a Meta Event, because Reset is never in SMF file
                        [[maybe_unused]] auto metatype = readU8(); // Meta type

                        const auto [msglength, varsize] = readVarNum();

                        if (_current - varsize + msglength > &_base.back()) {
                            throw smf_error(error_eof);
                        }
                        _len = 2 + varsize + msglength;
                        _current += msglength;
                        break;
                    }
                    case 0xF0: { // first format of sysex: F0 <len> <data> F7
                        const auto [datasize, varsize] = readVarNum();
                        if (_current + datasize > &_base.back()) {
                            throw smf_error(error_eof);
                        }
                        MIDIVarNum count = 0;
                        do {
                            data = readU8();
                            ++count;
                        } while (data != SYSEX_END);
                        if (count - 1 != datasize) { // also read sysex end but not in len
                            // todo: handle it
                        }
                        _len = 1 + varsize + count;
                        break;
                    }
                    case 0xF7: { // second format of sysex: F7 <len> <data>
                        const auto [datasize, varsize] = readVarNum();
                        if (_current + datasize > &_base.back()) {
                            throw smf_error(error_eof);
                        }
                        _len = 1 + varsize + datasize;
                        _current += datasize;
                        break;
                    }

                    default:
                        if ((_status & 0xF0u) == 0xF0u) {
                            _len = expected_system_message_length(_status);
                            _current += _len - 1;
                        } else {
                            throw smf_error(error_event_type);
                        }
                    }
                }
                return *this;
            }

            auto operator++(int) &
            {
                auto old = *this;
                ++(*this);
                return old;
            }

            bool operator==(const iterator& rhs) const noexcept
            {
                return _current == rhs._current;
            }

            bool operator==(std::default_sentinel_t /*unused*/) const noexcept
            {
                return _current == nullptr || _base.empty() || _current > &_base.back();
            }

        protected:
            iterator(base_type base, const uint8_t* current, uint8_t runningstatus = 0)
                : _status{runningstatus}
                , _current{current}
                , _base{base}
            {
            }

            uint8_t readU8()
            {
                if (_current > &_base.back()) {
                    throw smf_error(smf_errc::error_eof);
                }
                uint8_t d = *_current;
                ++_current;
                return d;
            }

            std::pair<uint32_t, size_t> readVarNum()
            {
                auto result = read_smf_variable_length_number(std::ranges::subrange{_current, (&_base.back()) + 1});
                _current    = result.it;
                return {result.result, result.size};
            }
        };

        explicit span_track(base_type base) noexcept
            : _base(base)
        {
        }

        base_type& base() noexcept
        {
            return _base;
        }

        [[nodiscard]] const base_type& base() const noexcept
        {
            return _base;
        }

        [[nodiscard]] iterator begin() const noexcept
        {
            if (_base.empty()) {
                return iterator{_base, nullptr};
            }
            iterator it{_base, &_base.front() + 8}; // MTrk <len 4 bytes>
            ++it;
            return it;
        }

        [[nodiscard]] std::default_sentinel_t end() const noexcept
        {
            return {};
        }

    private:
        base_type _base;
    };

    static_assert(std::forward_iterator<span_track::iterator>);
    static_assert(std::ranges::forward_range<span_track>);
    static_assert(std::same_as<span_track::iterator, std::ranges::iterator_t<span_track>>);

    struct parse_smf_header_result {
        smf_header                            info;
        std::vector<std::span<const uint8_t>> tracks;
    };

    // not only parse header, but also split tracks
    [[nodiscard]] inline parse_smf_header_result parse_smf_header(const std::span<const uint8_t>& file)
    {
        using enum smf_errc;
        const uint8_t* current = file.data();
        ptrdiff_t      etc{};

        [[maybe_unused]] auto readU8 = [&]() {
            if (current > &file.back()) {
                throw smf_error(error_eof);
            }
            return *current++;
        };
        auto readU16 = [&]() {
            if (current + 1 > &file.back()) {
                throw smf_error(error_eof);
            }
            auto res = rawcat(*current, *(current + 1));
            current += 2;
            return res;
        };
        auto readU16E = [&]() {
            etc -= 2;
            return readU16();
        };
        auto readU32 = [&]() {
            if (current + 3 > &file.back()) {
                throw smf_error(error_eof);
            }
            auto res = rawcat(*current, *(current + 1), *(current + 2), *(current + 3));
            current += 4;
            return res;
        };

        auto hdr = readU32();
        if (hdr != MThd) {
            throw smf_error(error_file_header);
        }

        etc = readU32(); // for header chunk size != 6
        if (etc != 6) {
            // todo: handle it
        }

        uint16_t ftype = readU16E();
        if (ftype > 2) {
            throw smf_error(error_smf_type);
        }

        uint16_t ftrks = readU16E();

        // fix invaild smf type
        if (ftype == 0 && ftrks > 1) {
            throw smf_error(error_smf_type); // todo: handle it
            ftype = 1;
        }
        if (ftype > 2) {
            // todo: handle it
            if (ftrks > 1) {
                ftype = 1;
            } else {
                ftype = 0;
            }
        }

        if (ftrks == 0) {
            // warn("No track");
            // todo: handle it
        }

        auto fdiv = static_cast<division>(readU16());
        if (!fdiv) {
            throw smf_error(error_division);
        }
        if (fdiv.is_smpte()) {
            std::cerr << "Experimental: Negative MIDI Division (SMPTE)" << '\n';
            // todo: handle it
        }

        smf_header                            info{.type = ftype, .division = fdiv, .ntrk = ftrks};
        std::vector<std::span<const uint8_t>> trks;
        trks.reserve(ftrks);

        for (uint16_t i = 0; i < info.ntrk; ++i) {
            auto trkbegin = current;
            if (readU32() != MTrk) {
                throw smf_error(error_track_header);
            }
            const uint32_t length   = readU32();
            size_t         chunklen = length + 4 + 4;
            trks.emplace_back(trkbegin, chunklen);
            current += length;
        }

        return {info, std::move(trks)};
    }
}

template <>
constexpr bool std::ranges::enable_borrowed_range<mfmidi::span_track> = true;
