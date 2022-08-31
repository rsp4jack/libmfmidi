/// \file libmfmidi.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Main file of libmfmidi

#pragma once
#include <climits>

/*!
 * \mainpage
 *
 * \section history History

   \section design Design

   Channel is begin from 1.
   Tracks is begin from 0.
   Ports is begin from 1.

 * \section install Installation
 *
 * \section start Getting started

 * \section faq Frequently Asked Questions
    No one asked me...
*/

namespace libmfmidi {
    namespace {
        inline void _pseudo_check_arch() noexcept
        {
#if !defined(__LLP64__) && !defined(__ILP32__)
            static_assert(CHAR_BIT == 8);
            static_assert(sizeof(char) == 1); // of course.
            static_assert(sizeof(short int) == 2);
            static_assert(sizeof(int) == 4);
            static_assert(sizeof(long int) == 4); // FIXME: LP64 (*nix)
            static_assert(sizeof(long long int) == 8);
#endif
        }
    }
}
