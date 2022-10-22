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

/// \file abstractmidinotifier.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief AbstractMIDINotifier

namespace libmfmidi {
    /// \brief Notifier
    /// Notifier is high-level interface of MIDI real-time message handling.
    /// e.g. Note on and off, program change, playback start and stop...
    class AbstractMIDINotifier {
    public:
        enum class NotifyCategory {
            Full,
            Conductor,
            Transport,
            Track
        };

        enum class NotifyType {
            All,
            C_Tempo,
            C_KeySig,
            C_TimeSig,
            C_Marker,
            T_Mode, // play, stop...
            T_Measure,
            T_Beat,
            T_EndOfSong, ///< End of playback, IS NOT \a T_Mode
            TR_Name,
            TR_Note,
            TR_PG,
            TR_CC,
            TR_PitchBend,
            TR_AfterTouch
        }

        virtual void notify() noexcept = 0;
    };
}
