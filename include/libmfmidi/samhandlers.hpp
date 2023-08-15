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
            *stm << fmt::format("{}\t{}\t{}", ticktime, statusToText(msg.status(), 1), msg.msgHex()) << std::endl;
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

        void on_starttrack(uint16_t trk) override
        {
            ticktime = 0;
            *stm << "Track " << trk << ":" << '\n'
                 << "Tick Time\tMessage Type\tMessage" << std::endl;
        }

        void on_endtrack(uint16_t  /*trk*/) override
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

        void on_starttrack(uint16_t trk) override
        {
            curtrk = trk;
        }

        void on_endtrack(uint16_t /*trk*/) override
        {
            curtrk = 0xFFFFFFFF;
        }

    private:
        MIDIMultiTrack* _file;
        SMFFileInfo*    _info;
        uint32_t        curtrk = 0xFFFFFFFF;
    };
}
