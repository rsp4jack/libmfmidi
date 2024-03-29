/// \file abstractsamhandler.hpp
/// \brief Abstract SAM (Simple API for SMF MIDI Files) Handler

#pragma once

#include "libmfmidi/midimessage.hpp"
#include <stdexcept>
#include <fmt/core.h>

namespace libmfmidi {
    class AbstractSAMHandler {
    public:
        AbstractSAMHandler() noexcept = default;
        virtual ~AbstractSAMHandler() noexcept = default;
        virtual void on_midievent(MIDITimedMessage&& msg) = 0;

        virtual void on_error(std::streampos where, const std::string& what, bool warn)
        {
            throw std::runtime_error(fmt::format("Where: {} What: {} Warn: []", static_cast<std::streamoff>(where), what, warn));
        }

        // Struct
        virtual void on_starttrack(uint16_t trk) = 0;
        virtual void on_endtrack(uint16_t trk) = 0;
        virtual void on_header(SMFType format, uint16_t ntrk, MIDIDivision division) = 0;
    };
}
