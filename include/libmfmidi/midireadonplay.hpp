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

/// \file midireadonplay.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Read on Play

#pragma once

#include "libmfmidi/midimessage.hpp"
#include "libmfmidi/smfreaderpolicy.hpp"
#include <span>
#include <ranges>
#include <tuple>

namespace libmfmidi {
    // TODO: Read on Play
    using MIDIReadOnPlayTimedMessage = MIDIBasicTimedMessage<std::span<const uint8_t>>;

    class MIDIReadOnPlayTrack {
    public:
        using base_type = std::span<const uint8_t>;

        explicit MIDIReadOnPlayTrack(base_type base) noexcept
            : m_data(base)
        {
        }

        class sentry {};

        class iterator {
            friend MIDIReadOnPlayTrack;

        public:
            using difference_type = std::ptrdiff_t;
            using value_type      = MIDIReadOnPlayTimedMessage;

            iterator() noexcept = default;

            MF_DEFAULT_COPY(iterator);
            MF_DEFAULT_MOVE(iterator);
            ~iterator() noexcept = default;

            MIDIReadOnPlayTimedMessage operator*() const
            {
                if (m_start == nullptr) {
                    throw std::logic_error{"Unable to dereference a empty iterator"};
                }
                return MIDIReadOnPlayTimedMessage{
                    m_delta,
                    base_type{m_start, m_len}
                };
            }

            auto& operator++() &
            {
                if (m_current == nullptr || m_base == nullptr) {
                    throw std::logic_error{"Unable to increase a empty iterator"};
                }
                uint8_t data;
                // Read a event
                std::tie(m_delta, std::ignore) = readVarNum();

                m_start = m_current;

                // status
                data = readU8();
                if (data < 0x80) {
                    if (status == 0) {
                        throw std::runtime_error("Running Status without status");
                    }
                    --m_current;
                } else {
                    status = data;
                }

                if ((status >= NOTE_OFF) && (status < SYSEX_START)) { // also test if status is vaild
                    m_len = getExpectedMessageLength(status);
                    m_current += m_len-1;
                } else {
                    switch (status) {
                    case META_EVENT: {
                        // this is a Meta Event, because Reset is never in SMF file
                        auto metatype = readU8(); // Meta type

                        const auto [msglength, varsize] = readVarNum();

                        if (m_current + msglength > &m_base->base().back()) {
                            throw std::runtime_error("invalid Meta Event length: bigger than track length");
                        }
                        m_len = 2 + varsize + msglength;
                        m_current += msglength;
                        break;
                    }
                    case 0xF0: { // first format of sysex: F0 <len> <data> F7
                        const auto [msglength, varsize] = readVarNum();
                        if (m_current + msglength > &m_base->base().back()) {
                            throw std::runtime_error("invalid SysEx Event length: bigger than track length");
                        }
                        MIDIVarNum count = 0;
                        do {
                            data = readU8();
                            ++count;
                        } while (data != SYSEX_END);
                        if (count - 1 != msglength) { // also read sysex end but not in len
                            warnpol(InvaildSysExLength, "Invaild SysEx length: given length != actually length");
                        }
                        m_len = 1 + varsize + count;
                        break;
                    }
                    case 0xF7: { // second format of sysex: F7 <len> <data>
                        const auto [msglength, varsize] = readVarNum();
                        if (m_current + msglength > &m_base->base().back()) {
                            throw std::runtime_error("invalid SysEx Event length: bigger than track length");
                        }
                        m_len = 1 + varsize + msglength;
                        m_current += msglength;
                        break;
                    }

                    default:
                        if ((status & 0xF0) == 0xF0) {
                            warnpol(IncompatibleEvent, "Incompatible SMF event: System Message [0xF1,0xFE] in SMF file");
                            m_len = getExpectedSMessageLength(status);
                            m_current += m_len-1;
                        } else {
                            throw std::runtime_error("Unknown or Unexpected status: not a Channel Message, Meta Event or SysEx");
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
                return m_current == rhs.m_current && m_base == rhs.m_base;
            }

            bool operator==(sentry /*unused*/) const noexcept
            {
                return m_current == nullptr || m_base == nullptr || m_current > &m_base->base().back();
            }

        protected:
            iterator(const MIDIReadOnPlayTrack* base, const uint8_t* current, uint8_t runningstatus = 0)
                : status{runningstatus}
                , m_current{current}
                , m_base{base}
            {
            }
            using enum SMFReaderPolicy;
            using enum MIDIMsgStatus;

            void warnpol(SMFReaderPolicy pol, const std::string_view& why) noexcept
            {
                std::cerr << "[SMFReader/ROP WARN] P" << static_cast<unsigned int>(pol) << ": " << why << "\n";
            }

            uint8_t readU8()
            {
                if (m_current > &m_base->base().back()) {
                    throw std::runtime_error("ROP: Unexpected EOF");
                }
                uint8_t d = *m_current;
                ++m_current;
                return d;
            }

            std::pair<uint32_t, size_t> readVarNum()
            {
                auto result = libmfmidi::readVarNumIt(static_cast<const uint8_t*>(m_current), (&m_base->base().back()) + 1);
                m_current += result.second;
                return result;
            }

        private:
            // Current message
            const uint8_t* m_start = nullptr;
            size_t         m_len   = 0;
            MIDIClockTime  m_delta = 0;

            // SMF reader
            uint8_t status = 0; // midi status, for running status

            // Stream
            const uint8_t* m_current = nullptr;

            const MIDIReadOnPlayTrack* m_base = nullptr;
        };

        MIDIReadOnPlayTrack(std::span<uint8_t> data)
            : m_data(data)
        {
        }

        base_type& base() noexcept
        {
            return m_data;
        }

        [[nodiscard]] const base_type& base() const noexcept
        {
            return m_data;
        }

        [[nodiscard]] iterator begin() const noexcept
        {
            iterator it{this, &m_data.front()};
            ++it;
            return it;
        }

        [[nodiscard]] sentry end() const noexcept
        {
            return {};
        }

    private:
        base_type m_data;
    };

    static_assert(std::forward_iterator<MIDIReadOnPlayTrack::iterator>);
    static_assert(std::ranges::forward_range<MIDIReadOnPlayTrack>);
    static_assert(std::same_as<MIDIReadOnPlayTrack::iterator, std::ranges::iterator_t<MIDIReadOnPlayTrack>>);
}
