/// \file platformapi.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Platform "Abstract"

#pragma once

namespace libmfmidi {
    /// \brief libmfmidi's nanosleep
    ///
    /// \param nsec time in nanoseconds
    /// \return int 0 if succeeded
    int nanosleep(unsigned long long nsec);

    /// \brief Get high resolution time stamp in nanoseconds
    /// 
    /// \return unsigned long long time stamp in nanoseconds
    unsigned long long hiresticktime(); // TODO: sched_clock in linux
}
