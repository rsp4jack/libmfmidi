/// \file mfutils.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief libmfmidi utils

#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>


namespace libmfmidi::Utils {
    enum class MFMessageMark {
        None,
        BeatMarker,
        NoOp,
        User
    };

    using std::uint8_t;

    // https://stackoverflow.com/a/72161270/
    // Removed. See https://github.com/clangd/clangd/issues/1179 and https://github.com/llvm/llvm-project/issues/55890
    /* template <std::size_t N>
    using typeof = decltype([] {
        if constexpr (N <= 1) {
            return std::uint8_t{};
        } else if constexpr (N <= 2) {
            return std::uint16_t{};
        } else if constexpr (N <= 4) {
            return std::uint32_t{};
        } else {
            static_assert(N <= 8);
            return std::uint64_t{};
        }
    }());
    */

    /// Return a type that can storage T byte(s).
    template <size_t T>
    using typeof = decltype([]<size_t N>() {
        if constexpr (N <= 1) {
            return uint8_t{};
        } else if constexpr (N <= 2) {
            return uint16_t{};
        } else if constexpr (N <= 4) {
            return uint32_t{};
        } else {
            static_assert(N <= 8);
            return uint64_t{};
        }
    }.template operator()<T>());

    /// \brief A function to connect bytes,
    /// \code {.cpp}
    /// rawCat(0xFF, 0xFA) -> 0xFFFA
    /// \endcode
    template <class... T>
    constexpr auto rawCat(uint8_t fbyte, T... bytes)
    {
        typeof<sizeof...(bytes) + 1> result = fbyte << (sizeof...(bytes) * 8);
        if constexpr (sizeof...(bytes) > 0) {
            result += rawCat(bytes...);
        }
        return result;
    } 
}
