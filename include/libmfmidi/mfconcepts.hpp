/// \file mfconcepts.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief libmfmidi template concepts and forward declarations

#pragma once

#include <concepts>
#include <functional>

namespace libmfmidi {
    class MIDIMessage;
    namespace details {
        template <class>
        class MIDIMessageTimedExt;
    }
    using MIDITimedMessage = details::MIDIMessageTimedExt<MIDIMessage>;

    template <class T>
    concept MIDIProcessor = requires(T obj, MIDITimedMessage msg) {
                                {
                                    obj.process(msg)
                                    } -> std::same_as<bool>;
                            };

    using MIDIProcessorFunction = std::function<bool(MIDITimedMessage&)>;
}
