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

/// \file abstracttimer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Timer

#pragma once

#include <functional>
#include <utility>
#include <cstdint>

namespace libmfmidi::details {
    class Win32MMTimer;
    class POSIXTimer;
}

namespace libmfmidi {
    /// \brief Abstract timer
    class AbstractTimer {
    public:
        virtual ~AbstractTimer() noexcept = default;
        using callback_type               = std::function<void()>;
        /// in ms
        virtual bool start(unsigned long delay) = 0;

        virtual bool stop() = 0;

        virtual bool isOn() = 0;

        virtual void setCallback(callback_type func) noexcept
        {
            m_cb = std::move(func);
        }

        /// in ms
        [[nodiscard]] virtual int delay() const noexcept
        {
            return mdelay;
        }

    protected:
        callback_type m_cb;
        unsigned int mdelay = 0;
    };

#if defined(_WIN32)
    using PlatformTimer = details::Win32MMTimer;
#elif defined(_POSIX_)
    using PlatformTimer = details::POSIXTimer;
#endif

}
