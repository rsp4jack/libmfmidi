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

/// \file smfreaderpolicy.hpp
/// \author Creepercdn (creepercdn@outlook.com)
/// \brief SMFReader Policies

#include <functional>

namespace libmfmidi {
    enum class SMFReaderPolicy {
        None,
        UnexceptedEOF,
        InvaildHeader, // When read file header, treat it as MThd; when read tracks, treat it as MTrk
        NoEndOfTrack,
        // TODO: More...
    };

    /// Param: Policy
    /// Return value: true to throw an error, false to ignore
    using SMFReaderPolicyProcessorType = std::function<bool(SMFReaderPolicy)>;
}
