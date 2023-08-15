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

#pragma once

#include <iostream>
#include <istream>
#include <functional>
#include "libmfmidi/abstractsamhandler.hpp"
#include "libmfmidi/midiutils.hpp"
#include "libmfmidi/mfexceptions.hpp"
#include "libmfmidi/smffile.hpp"
#include "libmfmidi/smfreaderpolicy.hpp"
#include <tl/expected.hpp>
#include <libmfmidi/mfconcepts.hpp>

namespace libmfmidi {
    constexpr uint32_t MThd = 1297377380U; // big endian
    constexpr uint32_t MTrk = 1297379947U; // big endian

    struct smf_error : public std::exception {
        smf_error(SMFReaderPolicy pol, const char* msg) noexcept
            : m_policy(pol)
            //, m_message(msg)
            , m_what_s(std::format("smf_error: {} (P{})", msg, static_cast<std::underlying_type_t<SMFReaderPolicy>>(pol)))
        {
        }

        [[nodiscard]] const char* what() const noexcept override 
        {
            return m_what_s.c_str();
        }

        [[nodiscard]] SMFReaderPolicy policy() const
        {
            return m_policy;
        }

    private:
        SMFReaderPolicy m_policy;
        std::string     m_what_s;
    };

    template <class T>
    concept SMFReaderIO = requires(T clz, char* data, std::streamsize size, uint8_t u8v) {
        {
            clz.readU8()
        } -> std::same_as<uint8_t>;
        {
            clz.readU16()
        } -> std::same_as<uint16_t>;
        {
            clz.readU32()
        } -> std::same_as<uint32_t>;
        clz.readRaw(data, size);
        {
            clz.readVarNum()
        } -> std::same_as<std::pair<uint32_t, size_t>>;
        clz.putback(u8v);
    };

    class SMFReaderStdIstreamIO {
    public:
        explicit SMFReaderStdIstreamIO(std::istream* ist) noexcept
            : stream(ist)
        {
        }

        std::istream* stream;

        [[nodiscard]] uint8_t readU8() const
        {
            uint8_t val = stream->get();
            return val;
        }

        [[nodiscard]] uint16_t readU16() const
        {
            uint16_t dat;
            stream->read(reinterpret_cast<char*>(&dat), 2);
            dat = byteswapbe(dat);
            return dat;
        }

        [[nodiscard]] uint32_t readU32() const
        {
            uint32_t dat;
            stream->read(reinterpret_cast<char*>(&dat), 4);
            dat = byteswapbe(dat);
            return dat;
        }

        void readRaw(char* data, size_t size) const
        {
            stream->read(data, static_cast<std::streamsize>(size));
        }

        [[nodiscard]] std::pair<uint32_t, size_t> readVarNum() const
        {
            return libmfmidi::readVarNum(stream);
        }

        void putback(uint8_t chr) const
        {
            stream->putback(static_cast<char>(chr));
        }
    };

    static_assert(SMFReaderIO<SMFReaderStdIstreamIO>);

    /// \brief SMF Reader
    using Reader = SMFReaderStdIstreamIO; // TODO: remove me

    class SMFReader : protected Reader {
        ptrdiff_t                    m_etc = 0; // signed because we need negative number to detect if underflow
        AbstractSAMHandler*          m_hand;
        SMFReaderPolicyProcessorType m_pol;
        bool                         m_warnheader        = false;
        bool                         m_warntrk           = false;
        bool                         m_warn              = false; // internal
        bool                         m_usedrunningstatus = false;

    public:
        template <class... ReaderArg>
        explicit SMFReader(AbstractSAMHandler* handler, ReaderArg&&... readerargs)
            : Reader(std::forward<ReaderArg>(readerargs)...)
            , m_hand(handler)
        {
            resetReader();
        }

        /// \warning reset before parse if needed
        void parse()
        {
            auto info = readHeader();
            for (uint16_t i = 0; i < info.ntrk; ++i) {
                readTrack(i);
            }
        }

        /// \warning This won't reset IO
        void resetReader() noexcept
        {
            m_etc        = 0;
            m_warnheader = false;
            m_warntrk    = false;
            m_warn       = false;
        }

        void setPolicyProcessor(SMFReaderPolicyProcessorType func)
        {
            m_pol = std::move(func);
        }

        Reader* reader()
        {
            return this;
        }

    protected:
        using enum SMFReaderPolicy;
        using enum MIDIMsgStatus;

        bool policy(SMFReaderPolicy pol)
        {
            if (m_pol) {
                return m_pol(pol);
            }
            return true;
        }

        // TODO: error and warning handling
        void warnpol(SMFReaderPolicy pol, const std::string_view& why) noexcept
        {
            std::cerr << "SMFReader: P" << static_cast<unsigned int>(pol) << " ignored an error (" << why << ")" << '\n';
            m_warn = true;
        }

        void warn(const std::string_view& why)
        {
            std::cerr << "SMFReader: Warning: " << why << '\n';
            m_warn = true;
        }

#pragma region io

        uint8_t readU8E()
        {
            --m_etc;
            return readU8();
        }

        uint16_t readU16E()
        {
            m_etc -= 2;
            return readU16();
        }

        uint32_t readU32E()
        {
            m_etc -= 4;
            return readU32();
        }

        void readRawE(char* dat, std::streamsize size)
        {
            m_etc -= size;
            readRaw(dat, size);
        }

        /// Max variable number is 0X0FFFFFFF (32bit)
        uint32_t readVarNumE()
        {
            auto result = readVarNum();
            m_etc -= static_cast<ptrdiff_t>(result.second);
            return result.first;
        }

#pragma endregion

        // clang-format off
#define REPORTP(pol, msg)            \
    if (policy(pol)) {               \
        throw                        \
            smf_error{(pol), (msg)}  \
        ;                            \
    }                                \
    warnpol(pol, msg);

#define REPORT(msg) throw smf_error{None,(msg)};

        // clang-format on

        SMFFileInfo readHeader()
        {
            auto hd = readU32();
            if (hd != MThd) {
                REPORTP(InvaildHeaderType, "Invalid header, expected MThd");
            }

            m_etc = readU32(); // for header chunk size != 6
            if (m_etc != 6) {
                REPORTP(InvaildHeaderSize, "invalid header chunk size, expected 6");
            }

            uint16_t ftype = readU16E();
            if (ftype > 2) {
                REPORTP(InvaildSMFType, "invalid SMF type, expected <= 2");
            }

            uint16_t ftrks = readU16E();

            // fix invaild smf type
            if (ftype == 0 && ftrks > 1) {
                REPORTP(InvaildSMFType, "Multiple tracks in SMF Type 0, fixing to SMF Type 1");
                ftype = 1;
            }
            if (ftype > 2) {
                REPORTP(InvaildSMFType, "SMF Type > 2, auto fixing to type 0 or 1");
                if (ftrks > 1) {
                    ftype = 1;
                } else {
                    ftype = 0;
                }
            }

            if (ftrks == 0) {
                warn("No track");
            }

            auto fdiv = static_cast<MIDIDivision>(readU16());
            if (!fdiv) {
                REPORT("MIDI Division is 0");
            }
            if (fdiv.isSMPTE()) {
                warn("Experimental: Negative MIDI Division (SMPTE)");
            }
            m_hand->on_header(ftype, ftrks, fdiv);
            m_warnheader = m_warn;

            return {.type = ftype, .division = fdiv, .ntrk = ftrks};
        }

        void readEvent(uint8_t& status, MIDIMessage& buffer)
        {
            // status
            uint8_t data = readU8E();
            if (data < 0x80) {
                if (status == 0) {
                    REPORT("Running Status without status");
                }
                m_usedrunningstatus = true;
                putback(data); // data is part of midi message
                ++m_etc;
            } else {
                status = data;
            }

            buffer.clear();
            buffer.reserve(4); // default to reserve 4 bytes

            buffer.push_back(status);

            if (buffer.isChannelMsg()) { // also test if status is vaild
                auto msg_len = buffer.expectedLength();
                buffer.resize(msg_len);

                readRawE(reinterpret_cast<char*>(buffer.base().data()+1), msg_len-1);
                return;
            }
            switch (status) {
            case META_EVENT: {
                // this is a Meta Event, because Reset is never in SMF file
                buffer.push_back(readU8E()); // Meta type

                const uint32_t len = readVarNumE();
                writeVarNumIt(len, std::back_inserter(buffer)); // todo: improve this behavior

                if (len > m_etc) {
                    REPORT("invalid Meta Event length: bigger than etc");
                }
                auto orig_size = buffer.size();
                buffer.resize(buffer.size() + len);
                readRawE(reinterpret_cast<char*>(buffer.base().data()+orig_size), len);
                break;
            }
            case 0xF0: { // first format of sysex: F0 <len> <data> F7
                const MIDIVarNum len = readVarNumE();
                if (len > m_etc) {
                    REPORT("invalid SysEx Event length: bigger than etc");
                }
                MIDIVarNum count = 0;
                do {
                    data = readU8E();
                    buffer.push_back(data);
                    ++count;
                } while (data != SYSEX_END);
                if (count - 1 != len) {
                    REPORTP(InvaildSysExLength, "Invaild SysEx length: given length != actually length");
                }
                break;
            }
            case 0xF7: { // second format of sysex: F7 <len> <data>
                const MIDIVarNum len = readVarNumE();
                if (len > m_etc) {
                    REPORT("invalid SysEx Event length: bigger than etc");
                }
                auto orig_size = buffer.size();
                buffer.resize(buffer.size() + len);
                readRawE(reinterpret_cast<char*>(buffer.base().data()+orig_size), len);
                break;
            }

            default:
                if (buffer.isSystemMessage()) {
                    REPORTP(IncompatibleEvent, "Incompatible SMF event: System Message [0xF1,0xFE] in SMF file");
                    for (int i = 0; i < buffer.expectedLength() - 1; ++i) {
                        buffer.push_back(readU8E());
                    }
                } else {
                    REPORT("Unknown or Unexpected status: not a Channel Message, Meta Event or SysEx");
                }
            }
        }

        /// \param trkid Track ID (begun from 0)
        void readTrack(uint16_t trkid)
        {
            m_warntrk = false;
            m_warn    = false;

            if (readU32() != MTrk) {
                warnpol(InvaildHeaderType, "invalid header, expected MTrk");
            }
            const uint32_t length = readU32();

            m_etc                   = length;
            uint8_t          status = 0; // midi status, for running status
            MIDITimedMessage buffer;

            m_hand->on_starttrack(trkid);
            while (m_etc > 0) {
                // Read a event
                const MIDIClockTime deltatime = readVarNumE();
                readEvent(status, buffer);
                buffer.setDeltaTime(deltatime);

                m_hand->on_midievent(std::move(buffer));
                /* if (m_etc < 0) {
                    REPORT("m_etc is negative, that means underflow");
                } */
            }

            m_hand->on_endtrack(trkid);
            m_warntrk = m_warn;
        }
    };
}
