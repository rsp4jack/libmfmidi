/// \file samhandlers.cpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Useful SAM Handlers

#pragma once

#include "libmfmidi/abstractsamhandler.hpp"
#include "libmfmidi/smffile.hpp"

namespace libmfmidi {
    class HumanReadableSAMHandler : public AbstractSAMHandler {
    public:
        explicit HumanReadableSAMHandler(std::ostream* istm)
            : stm(istm)
        {
        }

        void on_midievent(MIDITimedMessage&& msg) override
        {
            ticktime += msg.deltaTime();
            *stm << std::format("{}\t{}\t{}", ticktime, statusToText(msg.status(), 1), msg.msgHex()) << std::endl;
        }

        void on_header(SMFType format, uint16_t ntrk, MIDIDivision division) override
        {
            using std::endl;
            // *stm << "Header: " << std::format("Format {}; {} tracks; Division: {:x}", format, ntrk, division) << std::endl;
            *stm << "Header:" << '\n'
                 << "SMF Format: " << format << '\n'
                 << "Tracks: " << ntrk << '\n'
                 << "Division: " << divisionToText(division) << '\n'
                 << endl;
        }

        void on_starttrack(uint32_t trk) override
        {
            ticktime = 0;
            *stm << "Track " << trk << ":" << '\n'
                 << "Tick Time\tMessage Type\tMessage" << std::endl;
        }

        void on_endtrack(uint32_t  /*trk*/) override
        {
            *stm << std::endl;
        }

    private:
        std::ostream* stm;
        MIDIClockTime ticktime = 0;
    };

    class SMFFileSAMHandler : public AbstractSAMHandler {
    public:
        explicit SMFFileSAMHandler(MIDIMultiTrack* file, SMFFileInfo* inf)
            : _file(file)
            , _info(inf)
        {
        }

        void on_midievent(MIDITimedMessage&& msg) override
        {
            _file->at(curtrk).push_back(std::move(msg));
        }

        void on_header(SMFType format, uint16_t ntrk, MIDIDivision division) override
        {
            _info->type     = format;
            _info->division = division;
            _file->resize(ntrk);
        }

        void on_starttrack(uint32_t trk) override
        {
            curtrk = trk;
        }

        void on_endtrack(uint32_t /*trk*/) override
        {
            curtrk = 0xFFFFFFFF;
        }

    private:
        MIDIMultiTrack* _file;
        SMFFileInfo*    _info;
        uint32_t        curtrk = 0xFFFFFFFF;
    };
}
