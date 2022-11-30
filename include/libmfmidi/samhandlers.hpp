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

        void on_midievent(const MIDITimedMessage& msg) override
        {
            *stm << "Delta time: " << msg.deltaTime() << ' ' << "Message: " << msg.msgText() << std::endl;
        }

        void on_header(uint32_t format, uint16_t ntrk, MIDIDivision division) override
        {
            *stm << "Header: " << std::format("Format {}; {} tracks; Division: {:x}", format, ntrk, division) << std::endl;
        }

        void on_starttrack(uint32_t trk) override
        {
            *stm << std::format("------TRACK {} BEGIN------", trk) << std::endl;
        }

        void on_endtrack(uint32_t trk) override
        {
            *stm << std::format("-----TRACK {} END-----", trk) << std::endl;
        }

    private:
        std::ostream* stm;
    };

    class SMFFileSAMHandler : public AbstractSAMHandler {
    public:
        explicit SMFFileSAMHandler(MIDIMultiTrack* file, SMFFileInfo* inf)
            : _file(file)
            , _info(inf)
        {
        }

        void on_midievent(const MIDITimedMessage& msg) override
        {
            _file->at(curtrk).push_back(msg);
        }

        void on_header(uint32_t format, uint16_t ntrk, MIDIDivision division) override
        {
            _info->type = format;
            _info->division = division;
            _file->resize(ntrk);
        }

        void on_starttrack(uint32_t trk) override
        {
            curtrk = trk;
        }

        void on_endtrack(uint32_t  /*trk*/) override
        {
            curtrk = 0xFFFFFFFF;
        }

    private:
        MIDIMultiTrack* _file;
        SMFFileInfo* _info;
        uint32_t curtrk=0xFFFFFFFF;
    };
}
