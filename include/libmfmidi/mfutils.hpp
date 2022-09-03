/// \file mfutils.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief libmfmidi utils

#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>
#define __cpp_lib_format
#include <format>

namespace std {
#if !defined(__cpp_lib_byteswap)
    template <std::integral T>
    constexpr T byteswap(T value) noexcept // c++23 https://en.cppreference.com/w/cpp/numeric/byteswap
    {
        static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
        auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        std::ranges::reverse(value_representation);
        return std::bit_cast<T>(value_representation);
    }
#endif
#if !defined(__cpp_lib_unreachable)
    [[noreturn]] inline void unreachable()
    {
        // Uses compiler specific extensions if possible.
        // Even if no extension is used, undefined behavior is still raised by
        // an empty function body and the noreturn attribute.
#if defined(__GNUC__) // GCC, Clang, ICC
        __builtin_unreachable();
#elif defined(_MSC_VER) // MSVC
        __assume(false);
#endif
    }
#endif
}

/// \brief A empty class
class empty_class {};

namespace libmfmidi {
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::uint8_t;
}

namespace libmfmidi::Utils {
    template <std::integral T>
    constexpr T byteswapbe(T val) noexcept
    {
        if (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }
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
