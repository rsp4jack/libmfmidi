/// \file platformapi.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Platform "Abstract"

#pragma once

#include <chrono>

namespace libmfmidi {
    /// \brief libmfmidi's nanosleep
    ///
    /// \param nsec time in nanoseconds
    /// \return int 0 if succeeded
    int nanosleep(std::chrono::duration<unsigned long long, std::nano> nsec);

    /// \brief Get high resolution time stamp in nanoseconds
    /// 
    /// \return unsigned long long time stamp in nanoseconds
    std::chrono::duration<unsigned long long, std::nano> hiresticktime();
}
