/// \file mfutils.hpp
/// \brief libmfmidi utils

#pragma once

#include <bit>
#include <algorithm>
#include <charconv>
#include <concepts>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <cctype>
#include <chrono>
#include <version>
#include <utility>

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
    [[noreturn]] inline void unreachable() // c++23 https://en.cppreference.com/w/cpp/utility/unreachable
    {
        // Uses compiler specific extensions if possible.
        // Even if no extension is used, undefined behavior is still raised by
        // an empty function body and the noreturn attribute.
#if defined(__GNUC__)   // GCC, Clang, ICC
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
    using namespace std::literals;
    using std::int16_t;
    using std::int32_t;
    using std::int64_t;
    using std::int8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::uint8_t;
}

template <class T>
inline constexpr void MF_UNUSED(const T& arg) noexcept
{
    static_cast<void>(arg);
}

#define MF_DISABLE_COPY_CTOR(T) T(const T&) noexcept = delete
#define MF_DISABLE_MOVE_CTOR(T) T(T&&) noexcept = delete
#define MF_DISABLE_COPY_ASGN(T) T& operator=(const T&) noexcept = delete
#define MF_DISABLE_MOVE_ASGN(T) T& operator=(T&&) noexcept = delete

#define MF_DEFAULT_COPY_CTOR(T) T(const T&) noexcept = default
#define MF_DEFAULT_MOVE_CTOR(T) T(T&&) noexcept = default
#define MF_DEFAULT_COPY_ASGN(T) T& operator=(const T&) noexcept = default
#define MF_DEFAULT_MOVE_ASGN(T) T& operator=(T&&) noexcept = default

#define MF_DISABLE_COPY(T)   \
    MF_DISABLE_COPY_CTOR(T); \
    MF_DISABLE_COPY_ASGN(T)
#define MF_DISABLE_MOVE(T)   \
    MF_DISABLE_MOVE_CTOR(T); \
    MF_DISABLE_MOVE_ASGN(T)
#define MF_DEFAULT_COPY(T)   \
    MF_DEFAULT_COPY_CTOR(T); \
    MF_DEFAULT_COPY_ASGN(T)
#define MF_DEFAULT_MOVE(T)   \
    MF_DEFAULT_MOVE_CTOR(T); \
    MF_DEFAULT_MOVE_ASGN(T)

namespace libmfmidi {
    template <std::integral T>
    constexpr T byteswapbe(T val) noexcept
    {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(val);
        }
        return val;
    }

    /// \brief Special markers for \a MIDIMessage .
    ///
    enum class MFMessageMark {
        None,  ///< None. default, no data
        NoOp,  ///< No operate, no data
        Tempo, ///< Set tempo in BPM, data: Big-endian 32 bit unsigned integer, tempo in BPM
        User
    };

    /// Return a type that can storage T byte(s).
    template <size_t T>
    using typein = decltype([]<size_t N>() {
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
        // clang-format off
        typein<sizeof...(bytes) + 1> result =
            static_cast<decltype(result)>( // mute warning
                static_cast<decltype(result)>(fbyte) << (sizeof...(bytes) * 8) // integral promotion
            );
        // clang-format on
        if constexpr (sizeof...(bytes) > 0) {
            result |= rawCat(bytes...);
        }
        return result;
    }

    constexpr std::string memoryDump(const void* memory, std::size_t size)
    {
        std::string result;
        result.reserve(size * 2 - 1);
        char buffer[2]{};
        bool ins = false;
        for (const auto& dat : std::span<const uint8_t>(static_cast<const uint8_t*>(memory), size)) {
            buffer[0] = 0;
            buffer[1] = 0;

            auto fmt  = std::to_chars(buffer, buffer + 2, static_cast<uint8_t>(dat), 16);
            buffer[0] = std::toupper(buffer[0]);
            buffer[1] = std::toupper(buffer[1]);

            if (fmt.ptr == &buffer[1]) {
                buffer[1] = buffer[0];
                buffer[0] = '0';
            }

            if (ins) {
                result.push_back(' ');
            } else {
                ins = true;
            }
            result.insert(result.end(), buffer, buffer + 2);
        }
        return result;
    }
}