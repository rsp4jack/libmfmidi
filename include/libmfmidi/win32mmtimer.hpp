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

/// \file win32mmtimer.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Win32 Multimedia Timer

#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include <mmsystem.h>
#include "libmfmidi/abstracttimer.hpp"
#include <stdexcept>
#include <algorithm>
#include <string>

namespace libmfmidi::platform {
    class Win32MMTimer : public AbstractTimer {
    public:
        static constexpr unsigned int resolution = 1;

        ~Win32MMTimer() noexcept override
        {
            stop();
        }

        bool start(unsigned long delay) override
        {
            if (ison) {
                return true;
            }
            TIMECAPS caps;
            auto     val = timeGetDevCaps(&caps, sizeof(TIMECAPS)); // reserved
            if (val != MMSYSERR_NOERROR) {
                return false;
            }
            mdelay = std::clamp(static_cast<UINT>(delay), caps.wPeriodMin, caps.wPeriodMax);
            if (timeBeginPeriod(mdelay) != TIMERR_NOERROR) {
                return false;
            }
            ison = true;
            timerid = timeSetEvent(mdelay, resolution, win32cb, reinterpret_cast<DWORD_PTR>(this), TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
            if (!timerid) {
                ison = false;
                timeEndPeriod(mdelay);
                return false;
            }
            mdelay = delay;
            return true;
        }

        bool stop() override
        {
            if (!ison) {
                return true;
            }
            if (timeKillEvent(timerid) != TIMERR_NOERROR) {
                return false;
            }
            if (timeEndPeriod(mdelay) != TIMERR_NOERROR) {
                return false;
            }
            ison = false;
            return true;
        }

        bool isOn() override
        {
            return ison;
        }

    private:
        static void CALLBACK win32cb(UINT  /*uTimerID*/, UINT  /*uMsg*/, DWORD_PTR dwUser, DWORD_PTR  /*dw1*/, DWORD_PTR  /*dw2*/)
        {
            reinterpret_cast<Win32MMTimer*>(dwUser)->m_cb();
        }

        unsigned int timerid;
        bool ison = false;
    };
}

#endif
