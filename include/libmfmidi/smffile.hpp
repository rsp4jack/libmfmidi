/// \file smffile.cpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMF MIDI File

#pragma once

#include "libmfmidi/midimultitrack.hpp"

namespace libmfmidi {
    /// \brief SMF File Info
    struct SMFFileInfo {
        uint16_t type{};
        MIDIUtils::MIDIDivision  division{};
        uint16_t ntrk;
    };
}
