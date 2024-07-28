#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "mfmidi/timingapi.hpp"

#if defined(_UNIX)
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <avrt.h>
#include <synchapi.h>
#endif

namespace mfmidi {
#if defined(_POSIX_VERSION)
    int nanosleep(std::chrono::nanoseconds nsec)
    {
        static timespec ts{};
        /* ts.tv_sec = nsec.count() / 1'000'000'000;
        ts.tv_nsec = nsec.count() % 1'000'000'000; */
        ts.tv_nsec = nsec.count();
        return ::nanosleep(&ts, nullptr);
    }

    std::chrono::nanoseconds hiresticktime()
    {
        static timespec ts;
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        return std::chrono::nanoseconds{ts.tv_sec * 1'000'000'000 + ts.tv_nsec};
    }

    int enable_thread_responsiveness()
    {
        // todo: implement it
        return 0;
    }

    int disable_thread_responsiveness()
    {
        // todo: implement it
        return 0;
    }
#elif defined(_WIN32)
    int nanosleep(std::chrono::nanoseconds nsec)
    {
        using namespace std;
        using namespace std::chrono;
        // constexpr DWORD MIN_RES = 15; // in ms
        // constexpr DWORD MAX_RES = 5;  // in ms
        constexpr DWORD TIMER_RES = 10; // in 100ns

        static bool         inited = false;
        static uint64_t     freq;
        thread_local HANDLE timer{}; // todo: close it

        // static TIMECAPS caps;
        if (!inited) [[unlikely]] {
            QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
            // if (timeGetDevCaps(&caps, sizeof(TIMECAPS)) != MMSYSERR_NOERROR) {
            //     return -1;
            // }
            inited = true;
        }
        if (timer == NULL) [[unlikely]] {
            timer = CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, DELETE | SYNCHRONIZE | TIMER_ALL_ACCESS);
            if (timer == NULL) {
                return 1;
            }
        }

        if (nsec == 0ns) {
            return 0;
        }

        // timeBeginPeriod(caps.wPeriodMin);
        // const hclock::time_point target_time{hclock::duration{hclock::now().time_since_epoch().count() + usec * (freq / 1'000'000)}};
        uint64_t current_time;
        uint64_t target_time;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&current_time));
        target_time = current_time + freq * nsec.count() / 1'000'000'000;

        while (true) {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&current_time));
            if (current_time < target_time) {
                // if ((target_time - current_time) > freq * MIN_RES / 1'000) {
                //     Sleep(
                //         max(
                //             static_cast<DWORD>((target_time - current_time) / static_cast<double>(freq) * 1000), MIN_RES
                //         ) // avoid underflow
                //         - MIN_RES
                //     );
                // } else if ((target_time - current_time) > freq * MAX_RES / 1'000) {
                //     Sleep(1);
                uint64_t period = -(target_time - current_time) * 10'000'000 / freq;
                if (-period > TIMER_RES) {
                    period += TIMER_RES;
                    if (SetWaitableTimerEx(timer, reinterpret_cast<LARGE_INTEGER*>(&period), 0, nullptr, nullptr, nullptr, 0) == 0) {
                        return 2;
                    }
                    if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
                        return 3;
                    }
                } else {
                    SwitchToThread();
                }
            } else {
                break;
            }
        }
        return 0;
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
        return std::chrono::nanoseconds{static_cast<unsigned long long>(tick / (freq / 1e9))};
    }

    thread_local DWORD  mmcss_task_index{};
    thread_local HANDLE mmcss_task_handle{};

    int enable_thread_responsiveness()
    {
        mmcss_task_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcss_task_index);
        if (mmcss_task_handle == 0) {
            return 1;
        }
        return 0;
    }

    int disable_thread_responsiveness()
    {
        if (AvRevertMmThreadCharacteristics(mmcss_task_handle) == 0) {
            return 1;
        }
        return 0;
    }
#else
    std::chrono::duration<unsigned long long, std::nano> hiresticktime()
    {
        return std::chrono::duration_cast<std::chrono::duration<unsigned long long, std::nano>>(std::chrono::steady_clock::now().time_since_epoch());
    }
#endif
}