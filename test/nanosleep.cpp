#include <chrono>
#include <fmt/core.h>
#include <libmfmidi/platformapi.hpp>
#include <thread>

int main()
{
    using libmfmidi::nanosleep;
    using namespace std::literals;

    auto prevClock = std::chrono::high_resolution_clock::now();
    while (true) {
        auto   thisClock = std::chrono::high_resolution_clock::now();
        double deltaTime = (thisClock - prevClock).count() / 1e9;
        printf(" frame time: %.2lf ms\n", deltaTime * 1e3);

        // updateGame();

        // make sure each frame takes *at least* 1/60th of a second
        auto frameClock = std::chrono::high_resolution_clock::now();
        double sleepSecs = 1.0 / 60 - (frameClock - thisClock).count() / 1e9;
        if (sleepSecs > 0)
            //std::this_thread::sleep_for();
            nanosleep(std::chrono::nanoseconds((int64_t)(sleepSecs * 1e9)));

        prevClock = thisClock;
    }
}