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

namespace libmfmidi {
    /// \brief SMF Reader
    class SMFReader {
    public:
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

        void reset()
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

        void reportp(SMFReaderPolicy pol, const std::string_view& why)
        {
            if (policy(pol)) {
                throw smf_reader_error(ise->tellg(), why.data(), pol);
            }
            std::cerr << "SMFReader: P" << static_cast<unsigned int>(pol) << " ignored an error (" << why << ")" << std::endl;
            m_warn = true;
        }

        [[noreturn]] void report(const std::string_view& why)
        {
            throw smf_reader_error(ise->tellg(), why.data());
        }

        void reportw(const std::string_view& why)
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

        void readVar(char* d, std::streamsize s)
        {
            ise->read(d, s);
        }

        void readVarE(char* d, std::streamsize s)
        {
            m_etc -= s;
            readVar(d, s);
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

        void readHeader()
        {
            reset();

            char mthd[5]{};
            readVar(mthd, 4);
            if (strcmp(mthd, "MThd") != 0) {
                reportp(InvaildHeaderType, "Invalid header, expected MThd");
            }

            m_etc = readU32(); // for header chunk size != 6
            if (m_etc != 6) {
                reportp(InvaildHeaderSize, "invalid header chunk size, expected 6");
            }

            m_type = readU16E();
            if (m_type > 2) {
                reportp(InvaildSMFType, "invalid SMF type, expected <= 2");
            }

            m_trks = readU16E();

            // fix invaild smf type
            if (m_type == 0 && m_trks > 1) {
                reportp(InvaildSMFType, "Multiple tracks in SMF Type 0, fixing to SMF Type 1");
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
                reportw("No track");
            }

            m_division = static_cast<MIDIDivision>(readU16());
            if (!m_division) {
                report("MIDI Division is 0");
            }
            if (m_division.isSMPTE()) {
                reportw("Experimental: Negative MIDI Division (SMPTE)");
            }
            m_hand->on_header(m_type, m_trks, m_division);

            m_warnheader = m_warn;
        }

        void readTrack()
        {
            // FIXME: A big bug in onestop test, see https://github.com/craigsapp/midifile
            m_warntrk = false;
            m_warn    = false;

            char mtrk[5]{};
            readVar(mtrk, 4);
            if (strcmp(mtrk, "MTrk") != 0) {
                reportp(InvaildHeaderType, "invalid header, expected MTrk");
            }

            uint8_t        data   = 0; // temp variable
            uint8_t        status = 0; // midi status, for running status
            const uint32_t length = readU32();
            m_etc                 = length;
            m_hand->on_starttrack(m_curtrk);
            while (m_etc > 0) {
                // Read a event
                const MIDIClockTime deltatime = readVarNumE();

                // status
                data = readU8E();
                if (data < 0x80) {
                    if (status == 0) {
                        report("Running Status without status");
                    }
                    m_usedrunningstatus = true;
                    ise->seekg(-1, std::ios_base::cur); // seek back
                    ++m_etc;
                } else {
                    status = data;
                }

                MIDITimedMessage buffer;
                buffer.reserve(4); // default to reserve 4 bytes
                buffer.setDeltaTime(deltatime);

                buffer.push_back(status);

                if (buffer.isChannelMsg()) { // also test if status is vaild
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
                            report("invalid Meta Event length: bigger than track length");
                        }
                        for (uint32_t i = 0; i < len; ++i) {
                            buffer.push_back(readU8E());
                        }
                        break;
                    }
                    case 0xF0: { // first format of sysex: F0 <len> <data> F7
                        const MIDIVarNum len = readVarNumE();
                        if (len > m_etc) {
                            report("invalid SysEx Event length: bigger than track length");
                        }
                        MIDIVarNum count = 0;
                        do {
                            data = readU8E();
                            buffer.push_back(data);
                            ++count;
                        } while (data != SYSEX_END);
                        if (count - 1 != len) { // also read sysex end but not in len
                            reportp(InvaildSysExLength, "Invaild SysEx length: given length != actually length");
                        }
                        break;
                    }
                    case 0xF7: { // second format of sysex: F7 <len> <data>
                        const MIDIVarNum len = readVarNumE();
                        if (len > m_etc) {
                            report("invalid SysEx Event length: bigger than track length");
                        }
                        for (MIDIVarNum i = 0; i < len; ++i) {
                            data = readU8E();
                            buffer.push_back(data);
                        }
                        break;
                    }

                    default:
                        if (buffer.isSystemMessage()) {
                            reportp(IncompatibleEvent, "Incompatible SMF event: System Message (0xF1-0xFE) in SMF file");
                            for (int i = 0; i < buffer.expectedLength() - 1; ++i) {
                                buffer.push_back(readU8E());
                            }
                        } else {
                            report("Unknown or Unexpected status: not a Channel Message, Meta Event or SysEx");
                        }
                    }
                }
                m_hand->on_midievent(buffer);
                if (m_etc < 0) {
                    report("m_etc is negative, that means underflow");
                }
            }

            m_hand->on_endtrack(m_curtrk);
            m_warntrk = m_warn;
        }

    private:
        int64_t                      m_etc = 0; // signed because we need negative number to detect if underflow
        std::istream*                ise;
        SMFType                     m_type{};
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
