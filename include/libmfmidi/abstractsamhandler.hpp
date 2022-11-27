/// \file abstractsamhandler.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Abstract SAM (Simple API for SMF MIDI Files) Handler

#pragma once

#include "libmfmidi/midimessage.hpp"
#include <format>
#include <stdexcept>

namespace libmfmidi {
    class AbstractSAMHandler {
    public:
        AbstractSAMHandler() noexcept = default;
        virtual ~AbstractSAMHandler() noexcept = default;
        virtual void on_midievent(const MIDITimedMessage& msg) = 0;

        virtual void on_error(std::streampos where, const std::string& what, bool warn)
        {
            throw std::runtime_error(std::format("Where: {} What: {} Warn: []", static_cast<std::streamoff>(where), what, warn));
        }

        // Struct
        virtual void on_starttrack(uint32_t trk) = 0;
        virtual void on_endtrack(uint32_t trk) = 0;
        virtual void on_header(uint32_t format, uint16_t ntrk, MIDIDivision division) = 0;
    };
}
