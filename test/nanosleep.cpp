#include <chrono>
#include <fmt/core.h>
#include <mfmidi/timingapi.hpp>
#include <thread>

int main()
{
    using mfmidi::nanosleep;
    using namespace std::literals;

    auto prevClock = std::chrono::high_resolution_clock::now();
    auto prevalter = mfmidi::hiresticktime();
    while (true) {
        auto   thisClock  = std::chrono::high_resolution_clock::now();
        auto   alterthis  = mfmidi::hiresticktime();
        double deltaTime  = (thisClock - prevClock).count() / 1e9;
        double alterDelta = (alterthis - prevalter).count() / 1e9;
        printf(" frame time: %.2lf (%.2lf) ms\n", deltaTime * 1e3, alterDelta * 1e3);

        // updateGame();

        // make sure each frame takes *at least* 1/60th of a second
        auto   frameClock = std::chrono::high_resolution_clock::now();
        double sleepSecs  = 1.0 / 60 - (frameClock - thisClock).count() / 1e9;
        if (sleepSecs > 0)
            // std::this_thread::sleep_for();
            nanosleep(std::chrono::nanoseconds((int64_t)(sleepSecs * 1e9)));

        prevClock = thisClock;
        prevalter = alterthis;
    }
}