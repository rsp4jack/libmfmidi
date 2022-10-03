/// \file smfreader.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMF Reader

#pragma once

#include <istream>
#include <functional>
#include "abstractsamhandler.hpp"
#include "midiutils.hpp"

namespace libmfmidi {
    /// \brief SMF Reader
    /// This class will read the
    class SMFReader {
    public:
        SMFReader(std::istream* istr, std::istream::off_type offest, AbstractSAMHandler* handler)
            : ise(istr)
            , m_off(offest)
            , m_hand(handler)
        {
            reset();
        }

        bool parse()
        {
            reset();
            readHeader();
            if (!m_sucheader) {
                return false;
            }
            for (uint16_t i = 0; i < m_trks; ++i) {
                m_curtrk = i;
                readTrack();
                if (!m_suclasttrk) {
                    return false;
                }
            }
            return true;
        }

        void reset()
        {
            m_suc       = false;
            m_warn      = false;
            m_sucheader = false;
            m_etc       = 0;
            ise->seekg(m_off, std::ios::beg);
            ise->exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
        }

    protected:
        uint8_t readU8()
        {
            uint8_t d;
            ise->read(reinterpret_cast<char*>(&d), 1);
            return d;
        }

        uint8_t readU8E()
        {
            // TODO: Maybe need underflow check.
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
            --m_etc;
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
            --m_etc;
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
            --m_etc;
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
            return MIDIUtils::readVarNum(ise);
        }

        uint32_t readVarNumE()
        {
            auto result = readVarNum();
            m_etc -= result.second;
            return result.first;
        }

        void readHeader()
        {
            m_sucheader = false;
            char mthd[5]{};
            readVar(mthd, 4);
            if (strcmp(mthd, "MThd") != 0) {
                report("invalid header, expected MThd");
                return;
            }
            if (readU32() != 6) {
                report("invalid header chunk size, expected 6");
                return;
            }
            m_type = readU16();
            if (m_type > 2) {
                report("invalid SMF type, expected <= 2");
                return;
            }
            m_trks = readU16();
            if (m_type == 0 && m_trks > 1) {
                reportw("Multiple tracks in SMF Type 0, fixing to SMF Type 1");
                m_type = 1;
            }
            if (m_trks == 0) {
                reportw("No track");
            }
            m_division = static_cast<MIDIDivision>(readU16());
            if (m_division == 0) {
                report("MIDI Division is 0");
                return;
            }
            if (m_division < 0) {
                reportw("Experimental: Negative MIDI Division (SMPTE)");
            }
            m_sucheader = true;
            m_hand->on_header(m_type, m_trks, m_division);
        }

        void readTrack()
        {
            m_suclasttrk = false;
            char mtrk[5]{};
            readVar(mtrk, 4);
            if (strcmp(mtrk, "MTrk") != 0) {
                report("invalid header, expected MTrk");
                return;
            }
            uint8_t c = 0; // temp variable
            uint8_t status = 0; // midi status
            uint32_t      length  = readU32();
            m_etc                 = length;
            m_hand->on_starttrack(m_curtrk);
            while (m_etc > 0) {
                // Read a event
                MIDIClockTime deltatime = readVarNumE();
                //m_hand->on_deltatime(deltatime);

                c = readU8E();
                if ((c&0x80)==0) {
                    if (status == 0) {
                        report("Running Status without status");
                        return;
                    }
                    m_userunningstatus = true;
                } else {
                    status = c;
                }
                MIDITimedMessage buffer{status};
                buffer.setDeltaTime(deltatime);
                if (buffer.isChannelMsg()) { // also if vaild (in range)
                    for (int i = 0; i < buffer.expectedLength()-1; ++i) {
                        buffer.push_back(readU8E());
                    }
                } else {
                    switch (status) {
                    case 0xFF:{
                        // Meta Event
                        // Reset wont in SMF file
                        buffer.push_back(readU8E()); // type
                        uint32_t len = readVarNumE();
                        writeVarNumIt(len, std::back_inserter(buffer));
                        if(len > m_etc) {
                            report("invalid Meta Event length: bigger than track length");
                            break;
                        }
                        for (uint32_t i = 0; i < len; ++i) {
                            buffer.push_back(readU8E());
                        }
                        break;
                    }
                    case MIDIUtils::MIDIMsgStatus::SYSEX_START:
                        do {
                            c = readU8E();
                            buffer.push_back(c);
                        } while (c != MIDIUtils::MIDIMsgStatus::SYSEX_END);
                        break;
                    default:
                        report("Unknown or Unexpected status: not a Channel Message, Meta Event or SysEx");
                        return;
                    }
                }
                m_hand->on_midievent(buffer);
            }
            m_hand->on_endtrack(m_curtrk);
            m_suclasttrk = true;
        }

        /// \brief Return if parse process got warnings
        [[nodiscard]] bool warned() const noexcept
        {
            return m_warn;
        }

    protected:
        void report(const std::string& what, bool warn = false)
        {
            m_warn |= warn;
            m_hand->on_error(ise->tellg(), what, warn);
        }

        void reportw(const std::string& what)
        {
            report(what, true);
        }

    private:
        size_t                 m_etc = 0;
        std::istream*          ise;
        uint16_t               m_type{};
        MIDIDivision               m_division{};
        uint16_t               m_trks{};
        std::istream::off_type m_off;
        AbstractSAMHandler*    m_hand;
        bool                   m_sucheader = false;
        bool                   m_suclasttrk = false;
        bool                   m_suc       = false; ///< Fully parsed
        bool                   m_warn       = false; ///< Parsed with warning, false if fully not parsed
        bool                   m_userunningstatus = false;
        uint16_t               m_curtrk{};
    };
}
