/* Provide a replacement for the POSIX nanosleep function.
   Copyright (C) 1999-2000, 2002, 2004-2022 Free Software Foundation, Inc.
   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.
   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* written by Jim Meyering
   and Bruno Haible for the native Windows part */

#include "nanosleep.hpp"
#include <ctime>
#include <cstdio>
#include <csignal>
#include <cerrno>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#endif

constexpr int BILLION = 1000 * 1000 * 1000;

#if HAVE_BUG_BIG_NANOSLEEP

int nanosleep(const struct timespec* requested_delay, struct timespec* remaining_delay)
#undef nanosleep
{
    /* nanosleep mishandles large sleeps due to internal overflow problems.
       The worst known case of this is Linux 2.6.9 with glibc 2.3.4, which
       can't sleep more than 24.85 days (2^31 milliseconds).  Similarly,
       cygwin 1.5.x, which can't sleep more than 49.7 days (2^32 milliseconds).
       Solve this by breaking the sleep up into smaller chunks.  */

    if (requested_delay->tv_nsec < 0 || BILLION <= requested_delay->tv_nsec) {
        errno = EINVAL;
        return -1;
    }

    {
        /* Verify that time_t is large enough.  */
        verify(TYPE_MAXIMUM(time_t) / 24 / 24 / 60 / 60);
        const time_t    limit   = 24 * 24 * 60 * 60;
        time_t          seconds = requested_delay->tv_sec;
        struct timespec intermediate;
        intermediate.tv_nsec = requested_delay->tv_nsec;

        while (limit < seconds) {
            int result;
            intermediate.tv_sec = limit;
            result              = nanosleep(&intermediate, remaining_delay);
            seconds -= limit;
            if (result) {
                if (remaining_delay)
                    remaining_delay->tv_sec += seconds;
                return result;
            }
            intermediate.tv_nsec = 0;
        }
        intermediate.tv_sec = seconds;
        return nanosleep(&intermediate, remaining_delay);
    }
}

#elif defined _WIN32 && !defined __CYGWIN__
/* Native Windows platforms.  */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* The Windows API function Sleep() has a resolution of about 15 ms and takes
   at least 5 ms to execute.  We use this function for longer time periods.
   Additionally, we use busy-looping over short time periods, to get a
   resolution of about 0.01 ms.  In order to measure such short timespans,
   we use the QueryPerformanceCounter() function.  */

int nanosleep(unsigned long long nsec)
{
    static bool initialized;
    /* Number of performance counter increments per nanosecond,
       or zero if it could not be determined.  */
    static double ticks_per_nanosecond;

    /* For requested delays of one second or more, 15ms resolution is
       sufficient.  */
    if (!initialized) {
        /* Initialize ticks_per_nanosecond.  */
        LARGE_INTEGER ticks_per_second;

        if (QueryPerformanceFrequency(&ticks_per_second)) {
            ticks_per_nanosecond = static_cast<double>(ticks_per_second.QuadPart) / 1'000'000'000;
        }

        initialized = true;
    }
    if (ticks_per_nanosecond) {
        /* QueryPerformanceFrequency worked.  We can use
           QueryPerformanceCounter.  Use a combination of Sleep and
           busy-looping.  */
        /* Number of milliseconds to pass to the Sleep function.
           Since Sleep can take up to 8 ms less or 8 ms more than requested
           (or maybe more if the system is loaded), we subtract 10 ms.  */
        const DWORD sleep_millis = nsec / 1000000 - 10;
        /* Determine how many ticks to delay.  */
        const LONGLONG wait_ticks = nsec * ticks_per_nanosecond;
        /* Start.  */
        LARGE_INTEGER counter_before;
        if (QueryPerformanceCounter(&counter_before)) {
            /* Wait until the performance counter has reached this value.
               We don't need to worry about overflow, because the performance
               counter is reset at reboot, and with a frequency of 3.6E6
               ticks per second 63 bits suffice for over 80000 years.  */
            LONGLONG const wait_until = counter_before.QuadPart + wait_ticks;
            /* Use Sleep for the longest part.  */
            if (sleep_millis > 0) {
                Sleep(sleep_millis);
            }
            /* Busy-loop for the rest.  */
            LARGE_INTEGER counter_after;
            for (;;) {
                if (QueryPerformanceCounter(&counter_after) == 0) {
                    /* QueryPerformanceCounter failed, but succeeded earlier.
                       Should not happen.  */
                    break;
                }
                if (counter_after.QuadPart >= wait_until) {
                    /* The requested time has elapsed.  */
                    break;
                }
            }
        }
    }
    return 0;
}

#else
/* Other platforms lacking nanosleep.
   It's not clear whether these are still practical porting targets.
   For now, just fall back on pselect.  */

/* Suspend execution for at least *REQUESTED_DELAY seconds.  The
 *REMAINING_DELAY part isn't implemented yet.  */

int nanosleep(const struct timespec* requested_delay, struct timespec* remaining_delay)
{
    return pselect(0, NULL, NULL, NULL, requested_delay, NULL);
}
#endif