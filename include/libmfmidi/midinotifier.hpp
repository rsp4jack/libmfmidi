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
/// \brief High-level notifier

#pragma once

#include "libmfmidi/mfconcepts.hpp"
#include <functional>
#include <algorithm>

namespace libmfmidi {
    enum class NotifyCategory {
        Full,
        Conductor,
        Transport,
        Track
    };

    enum class NotifyType { // update getNotifyCategoryOfNotifyType too
        All,
        C_All,
        C_Tempo,
        C_KeySig,
        C_TimeSig,
        C_Marker,
        T_All,
        T_Mode, // play, stop...
        T_Measure,
        T_Beat,
        //T_EndOfSong, ///< End of playback, when player stop at end, \a T_EndOfSong will notify without T_Mode.
        TR_All,
        TR_Name,
        TR_Note,
        TR_PG,
        TR_CC,
        TR_PitchBend,
        TR_AfterTouch
    };

    constexpr NotifyCategory getNotifyCategoryOfNotifyType(NotifyType type) noexcept
    {
        using enum NotifyType;
        switch (type) {
        case C_All:
        case C_Tempo:
        case C_KeySig:
        case C_TimeSig:
        case C_Marker:
            return NotifyCategory::Conductor;
        case T_All:
        case T_Mode:
        case T_Measure:
        case T_Beat:
        //case T_EndOfSong:
            return NotifyCategory::Transport;
        case TR_All:
        case TR_Name:
        case TR_Note:
        case TR_PG:
        case TR_CC:
        case TR_PitchBend:
        case TR_AfterTouch:
            return NotifyCategory::Track;
        case All:
            return NotifyCategory::Full;
        }
    }

    /// \brief Notifier
    /// Notifier is high-level interface of MIDI real-time message handling.
    /// e.g. Note on and off, program change, playback start and stop...
    /// \warning Notifier function \b MUST \b NOT throw any expection.
    using MIDINotifierFunctionType = std::function<void(NotifyType)>;

    /// \brief A utility class to support notifier in class. Use CRTP.
    /// Usage:
    /// \code {.cpp}
    /// class A : protected NotifyUtils<A> {
    /// public:
    ///     using NotifyUtils::addNotifier;
    ///     friend class NotifyUtils;
    /// private:
    ///     void doAddNotifier(const MIDINotifierFunctionType& func){
    ///         "Things to do after added notifier."
    ///     }
    /// };
    /// \endcode
    template <class T>
        requires is_simple_type<T>
    class NotifyUtils {
    public:
        void notify(NotifyType type) const noexcept
        {
            for (const auto& func : mnotifiers) {
                func(type);
            }
        }

        void addNotifier(MIDINotifierFunctionType func)
        {
            if(func){
                mnotifiers.push_back(func);
                static_cast<T*>(this)->doAddNotifier(func);
            }
        }

    protected:
        std::vector<MIDINotifierFunctionType> mnotifiers;

    private:
        void doAddNotifier(const MIDINotifierFunctionType& /*func*/) {} // default implemention
        void doRemoveNotifier(const MIDINotifierFunctionType& /*func*/){};
    };
}
