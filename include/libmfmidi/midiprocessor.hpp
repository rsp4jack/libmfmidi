/// \file MIDIProcessor.hpp
/// \brief MIDIProcessor utils

#pragma once

#include "libmfmidi/mfconcepts.hpp"
#include "libmfmidi/midimessage.hpp"
#include <functional>
#include <utility>

namespace libmfmidi {

    /// \brief A helper to make a \a MIDIProcessor by std::function
    ///
    class MIDIProcessorHelper {
    public:
        /// You must keep references in \a func available during MIDIProcessorHelper live time
        explicit MIDIProcessorHelper(MIDIProcessorFunction func) noexcept
            : proc(std::move(func))
        {
        }

        bool process(MIDITimedMessage& msg)
        {
            return proc(msg);
        }

    private:
        MIDIProcessorFunction proc;
    };

    class MIDIProcessorTransposer {
    public:
        MIDIProcessorTransposer() noexcept = default;

        explicit MIDIProcessorTransposer(int8_t tran) noexcept
            : m_tran(tran)
        {
        }

        bool process(MIDITimedMessage& msg) const
        {
            if (msg.isNote() || msg.isPolyPressure()) {
                int traned = static_cast<int>(msg.note()) + m_tran;
                if (traned > 127 || traned < 0) {
                    return false;
                }
                msg.set_note(static_cast<uint8_t>(traned));
            }
            return true;
        }

        [[nodiscard]] int8_t transpose() const noexcept
        {
            return m_tran;
        }

        void setTranspose(int8_t tran) noexcept
        {
            m_tran = tran;
        }

    protected:
        int8_t m_tran = 0;
    };

    class MIDIProcessorRechannelizer {
    public:
        /// All channel is begin from 1.
        void setRechan(uint8_t target, uint8_t dest)
        {
            if (target > 16 || dest > 16) {
                throw std::invalid_argument("target or dest > 16");
            }
            m_map.at(target-1) = dest;
        }

        [[nodiscard]] uint8_t rechan(uint8_t target) const
        {
            return m_map.at(target-1);
        }

        void setAllRechan(uint8_t dest)
        {
            if (dest > 16) {
                throw std::invalid_argument("dest > 16");
            }
            std::fill(m_map.begin(), m_map.end(), dest);
        }

        bool process(MIDITimedMessage& msg)
        {
            if (msg.isChannelMsg()) {
                msg.set_channel(m_map.at(msg.channel()-1));
            }
            return true;
        }

    protected:
        std::array<uint8_t, 16> m_map{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    };
}
