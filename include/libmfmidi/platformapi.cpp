#include "libmfmidi/platformapi.hpp"

#if defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else

#endif
#include <chrono>
#include <thread>
#include <cstdint>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace libmfmidi {
#if defined(__linux__)
    int nanosleep(unsigned long long nsec)
    {
        return ::nanosleep(nsec);
    }
#elif defined(_WIN32)
    int nanosleep(unsigned long long nsec)
    {
        using namespace std;
        using namespace std::chrono;
        constexpr DWORD MIN_RES = 3; // in ms
        constexpr DWORD MAX_RES = 2; // in ms

        static bool     inited = false;
        static uint64_t freq;
        static TIMECAPS caps;
        if (!inited) {
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
            if (timeGetDevCaps(&caps, sizeof(TIMECAPS)) != MMSYSERR_NOERROR) {
                return -1;
            }
            inited = true;
        }

        timeBeginPeriod(caps.wPeriodMin);
        // const hclock::time_point target_time{hclock::duration{hclock::now().time_since_epoch().count() + usec * (freq / 1'000'000)}};
        uint64_t current_time;
        uint64_t target_time;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&target_time));
        target_time += freq / 1'000'000'000.0 * nsec;

        while (true) {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&current_time));
            if (current_time < target_time) {
                if ((target_time - current_time) > freq / 1000 * MIN_RES) {
                    Sleep(
                        max(
                            static_cast<DWORD>((target_time - current_time) / static_cast<double>(freq) * 1000), MIN_RES
                        )
                        - MIN_RES
                    ); // avoid underflow
                } else if ((target_time - current_time) > freq / 1000 * MAX_RES) {
                    Sleep(1);
                } else {
                    SwitchToThread();
                }
            } else {
                break;
            }
        }

        timeEndPeriod(caps.wPeriodMin);
        return 0;
    }

#else
    int usleep(unsigned int usec)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(usec));
        return 0;
    }
#endif
}