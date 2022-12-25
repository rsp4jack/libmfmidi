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

/// \file midisequencer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief MIDI Sequencer

#pragma once

#include "libmfmidi/midiutils.hpp"
#include "libmfmidi/midimultitrack.hpp"

namespace libmfmidi {
    /// \brief A powerful sequencer
    ///
    class MIDISequencer {
    public:
        /// \brief A class holding status and play status
        /// 
        class MIDISequencerCursor {
        public:
            using Time = uintmax_t;
            explicit MIDISequencerCursor(MIDISequencer& seq)
                : mseq(seq)
            {
            }

            void tick(Time sleept) // the time that slept
            {
                static 
            }

        private:
            Time mtimetick{}; // in ns, support 5850 centuries long
            Time mmiditick{};
            MIDISequencer& mseq;
        };
    };
}

