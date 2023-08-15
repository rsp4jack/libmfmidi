/*
 * This file is a part of libmfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// \file mfconcepts.hpp
/// \brief libmfmidi template concepts and forward declarations

#pragma once

#include <concepts>
#include <ranges>
#include <functional>
#include <vector>
#include <tl/expected.hpp>

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
    using MIDITimedMessage      = MIDIBasicTimedMessage<std::vector<uint8_t>>;

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
    using to_simple_type_t = std::remove_cvref_t<
        std::remove_all_extents_t<
            std::remove_pointer_t<
                T>>>;
    template <class T>
    concept is_simple_type = std::same_as<to_simple_type_t<T>, T>;

    template <class From, class To>
    concept weak_convertible_to = std::convertible_to<From, To> && std::same_as<std::decay_t<From>, std::decay_t<To>>;
    template <class Wrap, class Data>
    concept unwrapable_to = weak_convertible_to<Wrap, Data>
                         || requires(Wrap wrap) {{wrap.value()} -> weak_convertible_to<Data>; };
};
