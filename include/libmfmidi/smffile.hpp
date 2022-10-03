/// \file smffile.cpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMF MIDI File

#pragma once

#include "midimultitrack.hpp"

namespace libmfmidi {
    /// \brief SMF File Info
    struct SMFFileInfo {
        uint16_t type{};
        MIDIUtils::MIDIDivision  division{};
        uint16_t ntrk;
    };

    class SMFFile : public MIDIMultiTrack {
    public:
        /// \brief Get SMF MIDI Type (0,1,2)
        [[nodiscard]] auto getType() const
        {
            return info.type;
        }

        [[nodiscard]] auto getDivision() const
        {
            return info.division;
        }

        void setType(uint8_t type)
        {
            info.type = type;
        }

        void setDivision(MIDIDivision div)
        {
            info.division = div;
        }

    private:
        SMFFileInfo info;
    };
}
