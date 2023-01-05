#include "libmfmidi/platformapi.hpp"
#include <iostream>

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
    int nanosleep(std::chrono::nanoseconds nsec)
    {
        using namespace std;
        using namespace std::chrono;
        constexpr DWORD MIN_RES = 15; // in ms
        constexpr DWORD MAX_RES = 5; // in ms

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
        target_time += freq / 1'000'000'000.0 * nsec.count();

        while (true) {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&current_time));
            if (current_time < target_time) {
                if ((target_time - current_time) > freq / 1000 * MIN_RES) {
                    Sleep(
                        max(
                            static_cast<DWORD>((target_time - current_time) / static_cast<double>(freq) * 1000), MIN_RES
                        ) // avoid underflow
                        - MIN_RES
                    );
                } else if ((target_time - current_time) > freq / 1000 * MAX_RES) {
                    Sleep(1);
                } else {
                    SwitchToThread();
                }
            } else {
                break;
            }
        }
        /*
        uint64_t st = 0, ct = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&st);
        do {
            if (nsec.count() / 1000 > 10000 + (ct - st) * 1000000 / freq)
                Sleep((nsec.count() / 1000 - (ct - st) * 1'000'000 / freq) / 2000);
            else if (nsec.count() / 1000 > 5000 + (ct - st) * 1000000 / freq)
                Sleep(1);
            else
                std::this_thread::yield();
            QueryPerformanceCounter((LARGE_INTEGER*)&ct);
        } while ((ct - st) * 1000000 < nsec.count() / 1000 * freq);
        timeEndPeriod(caps.wPeriodMin);
        return 0;
        */
    }

    std::chrono::nanoseconds hiresticktime()
    {
        static uint64_t freq = 0;
        if (freq == 0) {
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
        }
        uint64_t tick;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&tick));
        return std::chrono::nanoseconds{static_cast<unsigned long long>(tick / (freq / 1000.0 / 1000))};
    }

#else
    std::chrono::duration<unsigned long long, std::nano> hiresticktime()
    {
        return std::chrono::duration_cast<std::chrono::duration<unsigned long long, std::nano>>(std::chrono::steady_clock::now().time_since_epoch());
    }
#endif
}