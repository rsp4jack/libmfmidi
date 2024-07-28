#include "mfmidi/midi_message.hpp"
#include "mfmidi/midi_ranges.hpp"
#include <functional>
#include <ranges>
#include <span>

static_assert(std::ranges::forward_range<mfmidi::delta_timed_filter_view<std::span<mfmidi::MIDITimedMessage>, std::function<bool(mfmidi::MIDITimedMessage)>>>);
