#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <mfmidi/mfevents.hpp>

namespace mf = mfmidi;

struct EventA {
    auto operator<=>(const EventA&) const noexcept = default;
};

struct EventB {
    int value;
    int data() const
    {
        return value;
    }

    auto operator<=>(const EventB&) const noexcept = default;
};

struct EventC {
    int* val;
    int& data() const
    {
        return *val;
    }

    auto operator<=>(const EventC&) const noexcept = default;
};

template <class Ev>
void handleEv(Ev event) = delete;

template <>
void handleEv<EventA>([[maybe_unused]] EventA event)
{
    fmt::println("Handle EventA");
}

template <>
void handleEv<EventB>(EventB event)
{
    fmt::println("Handle EventB: {}", event.data());
}

template <>
void handleEv<EventC>(EventC event)
{
    fmt::println("Handle EventC: {} at 0x{:016x}", event.data(), reinterpret_cast<std::uintptr_t>(&event.data()));
}

int main()
{
    using fmt::println;

    println("Events: example of mfmidi");

    mf::event_emitter_util<EventA, EventB, EventC> emitter;
    emitter.add_event_handler([](auto ev) {
        handleEv(ev);
    });
    emitter.add_event_handler([](mf::event_with_data auto ev) {
        println("Handle event with data: {}", ev.data());
    });
    emitter.add_event_handler([](EventA ev) {
        println("Handle EventA #1");
    });

    int data = 42;
    emitter.emit(EventA{});
    emitter.emit(EventB{data});
    emitter.emit(EventC{&data});
}