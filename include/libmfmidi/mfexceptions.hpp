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

/// \file mfexceptions.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief libmfmidi exceptions

#include <ios>
#include <stdexcept>
#include <format>
#include "libmfmidi/mfutils.hpp"
#include "libmfmidi/smfreaderpolicy.hpp"

namespace libmfmidi {
    class smf_reader_error : public std::exception {
    public:
        explicit smf_reader_error(std::streampos pos, const char* msg, SMFReaderPolicy policy = SMFReaderPolicy::None)
            : std::exception(std::format("Error on byte {}: {}, Cause by policy: P{}", static_cast<std::streamoff>(pos), msg, static_cast<unsigned int>(policy)).c_str())
        {
        }
    };

}
