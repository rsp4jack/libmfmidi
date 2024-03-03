#include "libmfmidi/midistatus.hpp"

std::unordered_multimap<libmfmidi::MIDIStatus*, libmfmidi::MIDIStatusProcessor*> libmfmidi::MIDIStatusProcessor::mstmap{};