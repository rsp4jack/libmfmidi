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

/// \file midinotifier.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief High-level notifier

#include <functional>

namespace libmfmidi {
    enum class NotifyCategory {
        Full,
        Conductor,
        Transport,
        Track
    };

    enum class NotifyType { // update getNotifyCategoryOfNotifyType too
        All,
        C_Tempo,
        C_KeySig,
        C_TimeSig,
        C_Marker,
        T_Mode, // play, stop...
        T_Measure,
        T_Beat,
        T_EndOfSong, ///< End of playback, when player stop at end, \a T_EndOfSong will notify.
        TR_Name,
        TR_Note,
        TR_PG,
        TR_CC,
        TR_PitchBend,
        TR_AfterTouch
    };

    constexpr NotifyCategory getNotifyCategoryOfNotifyType(NotifyType type) noexcept
    {
        switch (type) {
        case NotifyType::C_Tempo:
        case NotifyType::C_KeySig:
        case NotifyType::C_TimeSig:
        case NotifyType::C_Marker:
            return NotifyCategory::Conductor;
        case NotifyType::T_Mode:
        case NotifyType::T_Measure:
        case NotifyType::T_Beat:
        case NotifyType::T_EndOfSong:
            return NotifyCategory::Transport;
        case NotifyType::TR_Name:
        case NotifyType::TR_Note:
        case NotifyType::TR_PG:
        case NotifyType::TR_CC:
        case NotifyType::TR_PitchBend:
        case NotifyType::TR_AfterTouch:
            return NotifyCategory::Track;
        case NotifyType::All:
            return NotifyCategory::Full;
        }
    }
    /// \brief Notifier
    /// Notifier is high-level interface of MIDI real-time message handling.
    /// e.g. Note on and off, program change, playback start and stop...
    using MIDINotifierFunctionType = std::function<void(NotifyType)>;
}
