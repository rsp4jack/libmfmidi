#include <libmfmidi/platformapi.hpp>
#include <fmt/core.h>
#include <chrono>

int main()
{
    using libmfmidi::nanosleep;
    using std::literals;

    std::chrono::microseconds msec{1ms};
    for (int i = 0; i < 12; ++i) {
        fmt::println("Sleeping for {}", msec);
        nanosleep(std::chrono::duration_cast<std::chrono::nanoseconds>(msec));

    }
}