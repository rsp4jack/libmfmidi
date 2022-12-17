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
}
