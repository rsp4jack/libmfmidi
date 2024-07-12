#pragma once

#include "mfmidi/mfutility.hpp"
#include "mfmidi/smf/division.hpp"
#include <stdexcept>

namespace mfmidi {
    constexpr uint32_t MThd = 1297377380U; // big endian
    constexpr uint32_t MTrk = 1297379947U; // big endian

    struct smf_header {
        uint16_t type{};
        division division{};
        uint16_t ntrk{};
    };
}