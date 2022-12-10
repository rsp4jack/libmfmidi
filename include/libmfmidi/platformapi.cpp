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
    int usleep(unsigned int usec)
    {
        return ::usleep(usec);
    }
#elif defined(_WIN32)
    int usleep(unsigned int usec)
    {
        using namespace std;
        using namespace std::chrono;
        using hclock                         = high_resolution_clock;
        constexpr unsigned long long MIN_RES = 6; // in ms
        constexpr unsigned long long MAX_RES = 2;  // in ms

        static bool     inited = false;
        static uint64_t freq;
        if (!inited) {
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
            inited = true;
        }

        timeBeginPeriod(1);
        // const hclock::time_point target_time{hclock::duration{hclock::now().time_since_epoch().count() + usec * (freq / 1'000'000)}};
        uint64_t current_time, target_time;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&target_time));
        target_time += freq / 1'000'000 * usec;

        while (true) {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&current_time));
            if (current_time < target_time) {
                if ((target_time - current_time) > freq / 1000 * MIN_RES) {
                    Sleep(max((target_time - current_time) / freq * 1000, MIN_RES) - MIN_RES); // avoid underflow
                } else if ((target_time - current_time) > freq / 1000 * MAX_RES) {
                    Sleep(1);
                } else {
                    SwitchToThread();
                }
            } else {
                break;
            }
        }

        timeEndPeriod(1);
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