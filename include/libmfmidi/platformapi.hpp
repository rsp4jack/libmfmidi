/// \file platformapi.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief Platform "Abstract"

#pragma once

namespace libmfmidi {
    /// \brief libmfmidi's usleep
    /// 
    /// \param usec time in us
    /// \return int 0 if succeeded
    int usleep(unsigned int usec);
}
