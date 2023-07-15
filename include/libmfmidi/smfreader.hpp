/// \file smfreader.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMF Reader

#pragma once

#include <iostream>
#include <istream>
#include <functional>
#include "libmfmidi/abstractsamhandler.hpp"
#include "libmfmidi/midiutils.hpp"
#include "libmfmidi/mfexceptions.hpp"
#include "libmfmidi/smfreaderpolicy.hpp"
#include <tl/expected.hpp>

namespace libmfmidi {
    /// \brief SMF Reader
    class SMFReader {
    public:
        struct smf_error {
            SMFReaderPolicy policy;
            std::streampos  position;
            const char*     message;
        };

        SMFReader(std::istream* istr, std::istream::off_type offest, AbstractSAMHandler* handler)
            : ise(istr)
            , m_off(offest)
            , m_hand(handler)
        {
            reset();
        }

        void parse()
        {
            reset();
            readHeader();
            for (uint16_t i = 0; i < m_trks; ++i) {
                m_curtrk = i;
                readTrack();
            }
        }

        void reset() noexcept
        {
            m_etc        = 0;
            m_warnheader = false;
            m_warntrk    = false;
            m_warn       = false;
            ise->clear();
            ise->seekg(m_off, std::ios::beg);
            ise->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
        }

        void setPolicyProcessor(SMFReaderPolicyProcessorType func)
        {
            m_pol = std::move(func);
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

        void warnpol(SMFReaderPolicy pol, const std::string_view& why) noexcept
        {
            std::cerr << "SMFReader: P" << static_cast<unsigned int>(pol) << " ignored an error (" << why << ")" << std::endl;
            m_warn = true;
        }

        void warn(const std::string_view& why)
        {
            std::cerr << "SMFReader: Warning on " << ise->tellg() << ": " << why << std::endl;
            m_warn = true;
        }

#pragma region io

        uint8_t readU8()
        {
            uint8_t d;
            ise->read(reinterpret_cast<char*>(&d), 1);
            return d;
        }

        uint8_t readU8E()
        {
            --m_etc;
            return readU8();
        }

        uint16_t readU16()
        {
            uint16_t d;
            ise->read(reinterpret_cast<char*>(&d), 2);
            d = byteswapbe(d);
            return d;
        }

        uint16_t readU16E()
        {
            m_etc -= 2;
            return readU16();
        }

        uint32_t readU32()
        {
            uint32_t d;
            ise->read(reinterpret_cast<char*>(&d), 4);
            d = byteswapbe(d);
            return d;
        }

        uint32_t readU32E()
        {
            m_etc -= 4;
            return readU32();
        }

        uint64_t readU64()
        {
            uint64_t d;
            ise->read(reinterpret_cast<char*>(&d), 8);
            d = byteswapbe(d);
            return d;
        }

        uint64_t readU64E()
        {
            m_etc -= 8;
            return readU64();
        }

        void readBin(char* d, std::streamsize s)
        {
            ise->read(d, s);
        }

        void readBinE(char* d, std::streamsize s)
        {
            m_etc -= s;
            readBin(d, s);
        }

        /// Max variable number is 0X0FFFFFFF (32bit)
        std::pair<uint32_t, size_t> readVarNum()
        {
            return libmfmidi::readVarNum(ise);
        }

        uint32_t readVarNumE()
        {
            auto result = readVarNum();
            m_etc -= result.second;
            return result.first;
        }

#pragma endregion

// clang-format off
#define REPORTP(pol, msg)                                                        \
    if (policy(pol)) {                                                           \
        return tl::unexpected{                                                   \
            smf_error{.policy = pol, .position = ise->tellg(), .message = msg}   \
        };                                                                       \
    }                                                                            \
    warnpol(pol, msg);

#define REPORT(msg) return tl::unexpected{ \
    smf_error{.policy = None, .position = ise->tellg(), .message = msg} \
};

        // clang-format on

        tl::expected<void, smf_error> readHeader() noexcept
        {
            reset();

            char mthd[5]{};
            readBin(mthd, 4);
            if (strcmp(mthd, "MThd") != 0) {
                REPORTP(InvaildHeaderType, "Invalid header, expected MThd");
            }

            m_etc = readU32(); // for header chunk size != 6
            if (m_etc != 6) {
                REPORTP(InvaildHeaderSize, "invalid header chunk size, expected 6");
            }

            m_type = readU16E();
            if (m_type > 2) {
                REPORTP(InvaildSMFType, "invalid SMF type, expected <= 2");
            }

            m_trks = readU16E();

            // fix invaild smf type
            if (m_type == 0 && m_trks > 1) {
                REPORTP(InvaildSMFType, "Multiple tracks in SMF Type 0, fixing to SMF Type 1");
                m_type = 1;
            }
            if (m_type > 2) {
                if (m_trks > 1) {
                    m_type = 1;
                } else {
                    m_type = 0;
                }
            }

            if (m_trks == 0) {
                warn("No track");
            }

            m_division = static_cast<MIDIDivision>(readU16());
            if (!m_division) {
                REPORT("MIDI Division is 0");
            }
            if (m_division.isSMPTE()) {
                warn("Experimental: Negative MIDI Division (SMPTE)");
            }
            m_hand->on_header(m_type, m_trks, m_division);

            m_warnheader = m_warn;

            return {};
        }

        tl::expected<void, smf_error> readTrack()
        {
            m_warntrk = false;
            m_warn    = false;

            char mtrk[5]{};
            readBin(mtrk, 4);
            if (strcmp(mtrk, "MTrk") != 0) {
                warnpol(InvaildHeaderType, "invalid header, expected MTrk");
            }

            uint8_t        data   = 0; // temp variable
            uint8_t        status = 0; // midi status, for running status
            const uint32_t length = readU32();
            m_etc                 = length;
            m_hand->on_starttrack(m_curtrk);
            MIDITimedMessage buffer;
            while (m_etc > 0) {
                // Read a event
                const MIDIClockTime deltatime = readVarNumE();

                // status
                data = readU8E();
                if (data < 0x80) {
                    if (status == 0) {
                        REPORT("Running Status without status");
                    }
                    m_usedrunningstatus = true;
                    ise->seekg(-1, std::ios_base::cur); // seek back
                    ++m_etc;
                } else {
                    status = data;
                }

                buffer.clear();
                buffer.reserve(4); // default to reserve 4 bytes
                buffer.setDeltaTime(deltatime);

                buffer.push_back(status);

                if (buffer.isChannelMsg()) {                                // also test if status is vaild
                    for (int i = 0; i < buffer.expectedLength() - 1; ++i) { // if isrs,
                        buffer.push_back(readU8E());
                    }
                } else {
                    switch (status) {
                    case META_EVENT: {
                        // this is a Meta Event, because Reset is never in SMF file
                        buffer.push_back(readU8E()); // Meta type

                        const uint32_t len = readVarNumE();
                        writeVarNumIt(len, std::back_inserter(buffer));

                        if (len > m_etc) {
                            REPORT("invalid Meta Event length: bigger than track length");
                        }
                        for (uint32_t i = 0; i < len; ++i) {
                            buffer.push_back(readU8E());
                        }
                        break;
                    }
                    case 0xF0: { // first format of sysex: F0 <len> <data> F7
                        const MIDIVarNum len = readVarNumE();
                        if (len > m_etc) {
                            REPORT("invalid SysEx Event length: bigger than track length");
                        }
                        MIDIVarNum count = 0;
                        do {
                            data = readU8E();
                            buffer.push_back(data);
                            ++count;
                        } while (data != SYSEX_END);
                        if (count - 1 != len) { // also read sysex end but not in len
                            REPORTP(InvaildSysExLength, "Invaild SysEx length: given length != actually length");
                        }
                        break;
                    }
                    case 0xF7: { // second format of sysex: F7 <len> <data>
                        const MIDIVarNum len = readVarNumE();
                        if (len > m_etc) {
                            REPORT("invalid SysEx Event length: bigger than track length");
                        }
                        for (MIDIVarNum i = 0; i < len; ++i) {
                            data = readU8E();
                            buffer.push_back(data);
                        }
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
                m_hand->on_midievent(std::move(buffer));
                /* if (m_etc < 0) {
                    REPORT("m_etc is negative, that means underflow");
                } */
            }

            m_hand->on_endtrack(m_curtrk);
            m_warntrk = m_warn;
            return {};
        }

    private:
        int64_t                      m_etc = 0; // signed because we need negative number to detect if underflow
        std::istream*                ise;
        SMFType                      m_type{};
        MIDIDivision                 m_division{};
        uint16_t                     m_trks{};
        std::istream::off_type       m_off;
        AbstractSAMHandler*          m_hand;
        SMFReaderPolicyProcessorType m_pol;
        bool                         m_warnheader        = false;
        bool                         m_warntrk           = false;
        bool                         m_warn              = false; // internal
        bool                         m_usedrunningstatus = false;
        uint16_t                     m_curtrk{};
    };
}
