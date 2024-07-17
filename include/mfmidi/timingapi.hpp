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

#include <chrono>

namespace mfmidi {
    /// \brief mfmidi's nanosleep
    ///
    /// \param nsec time in nanoseconds
    /// \return int 0 if succeeded
    int nanosleep(std::chrono::nanoseconds nsec);

    /// \brief Get high resolution time stamp in nanoseconds
    ///
    /// \return unsigned long long time stamp in nanoseconds
    std::chrono::nanoseconds hiresticktime();

    int enable_thread_responsiveness();
    int disable_thread_responsiveness();
}
