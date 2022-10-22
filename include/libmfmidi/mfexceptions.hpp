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
#include "mfutils.hpp"

namespace libmfmidi {
    class mf_error : public std::exception {
    public:
        explicit mf_error(const char* msg) noexcept
            : std::exception(msg)
        {
        }

        explicit mf_error(const std::string& msg) noexcept
            : std::exception(msg.c_str())
        {
        }

    private:
    };

    class smf_reader_error : public mf_error {
    public:
        explicit smf_reader_error(std::streampos pos, const char* msg)
            : mf_error(std::format("Error on byte {}: {}", static_cast<std::streamoff>(pos), msg))
        {
        }
    }
}
