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

#pragma once

#include "libmfmidi/midimessage.hpp"
#include <algorithm>
#include <range/v3/view/adaptor.hpp>
#include <ranges>
#include <vector>

namespace libmfmidi {

    constexpr void toAbsTimeTrack(MIDITrack& trk)
    {
        MIDIClockTime time = 0;
        for (auto& event : trk) {
            event.setDeltaTime(time += event.deltaTime());
        }
    }

    constexpr void toRelTimeTrack(MIDITrack& trk)
    {
        MIDIClockTime time = 0;
        for (auto& event : trk) {
            MIDIClockTime tmp = event.deltaTime();
            event.setDeltaTime(tmp - time);
            time = tmp;
        }
    }

}
