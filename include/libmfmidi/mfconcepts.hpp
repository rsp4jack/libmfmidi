/// \file mfconcepts.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief libmfmidi template concepts and forward declarations

#pragma once

#include <concepts>
#include <ranges>
#include <functional>
#include <vector>

namespace libmfmidi {
    template <std::ranges::range T>
    class MIDIBasicMessage;
    using MIDIMessage = MIDIBasicMessage<std::vector<uint8_t>>;

    namespace details {
        template <class>
        class MIDIMessageTimedExt;
    }

    template <class T>
    using MIDIBasicTimedMessage = details::MIDIMessageTimedExt<MIDIBasicMessage<T>>;
    using MIDITimedMessage = MIDIBasicTimedMessage<std::vector<uint8_t>>;

    template <class T>
    concept MIDIProcessorClass = requires(T obj, MIDITimedMessage msg) {
        {
            obj.process(msg)
        } -> std::same_as<bool>;
    };

    using MIDIProcessorFunction = std::function<bool(MIDITimedMessage&)>;

    // int -> true
    // MIDIMessage -> true
    // union -> true
    // enum class -> true
    // MIDIMessage* -> false
    // int[3] -> false
    // std::vector<int>&& -> false
    template <class T>
    concept is_simple_type = std::same_as<
        std::remove_cvref_t<
            std::remove_all_extents_t<
                std::remove_pointer_t<
                    T>>>,
        T>;
}
