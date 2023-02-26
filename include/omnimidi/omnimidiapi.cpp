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

#include <Windows.h>
#include <tchar.h>
#include "OmniMIDI.h"
#include "dllhelper.hpp"

#define KFUNCTION(rtt, func) \
    rtt KDMAPI(func)() \
    { \
        return static_cast<decltype(func)*>(MODULE()[#func])(); \
    }

static DllHelper& MODULE() noexcept // singleton
{
    static DllHelper instance{_T("OmniMIDI")};
    return instance;
}

// Return the KDMAPI version from OmniMIDI as the following output: Major.Minor.Build Rev. Revision (eg. 1.30.0 Rev. 51).
BOOL KDMAPI(ReturnKDMAPIVer)(LPDWORD Major, LPDWORD Minor, LPDWORD Build, LPDWORD Revision)
{
    return static_cast<decltype(ReturnKDMAPIVer)*>(MODULE()["ReturnKDMAPIVer"])(Major, Minor, Build, Revision);
}

// Checks if KDMAPI is available. You can ignore the output if you want, but you should give the user the choice between WinMM and KDMAPI.
KFUNCTION(BOOL, IsKDMAPIAvailable)

// Initializes OmniMIDI through KDMAPI. (Like midiOutOpen)
KFUNCTION(BOOL, InitializeKDMAPIStream)

// Closes OmniMIDI through KDMAPI. (Like midiOutClose)
KFUNCTION(BOOL, TerminateKDMAPIStream)

// Resets OmniMIDI and all its MIDI channels through KDMAPI. (Like midiOutReset)
KFUNCTION(VOID, ResetKDMAPIStream)

// Send short messages through KDMAPI. (Like midiOutShortMsg)
BOOL KDMAPI(SendCustomEvent)(DWORD eventtype, DWORD chan, DWORD param) noexcept
{
    return static_cast<decltype(SendCustomEvent)*>(MODULE()["SendCustomEvent"])(eventtype, chan, param);
}

// Send short messages through KDMAPI. (Like midiOutShortMsg)
VOID KDMAPI(SendDirectData)(DWORD dwMsg)
{
    return static_cast<decltype(SendDirectData)*>(MODULE()["SendDirectData"])(dwMsg);
}

// Send short messages through KDMAPI like SendDirectData, but bypasses the buffer. (Like midiOutShortMsg)
VOID KDMAPI(SendDirectDataNoBuf)(DWORD dwMsg)
{
    return static_cast<decltype(SendDirectDataNoBuf)*>(MODULE()["SendDirectDataNoBuf"])(dwMsg);
}

// Send long messages through KDMAPI. (Like midiOutLongMsg)
UINT KDMAPI(SendDirectLongData)(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize)
{
    return static_cast<decltype(SendDirectLongData)*>(MODULE()["SendDirectLongData"])(IIMidiHdr, IIMidiHdrSize);
}

// Send long messages through KDMAPI like SendDirectLongData, but bypasses the buffer. (Like midiOutLongMsg)
UINT KDMAPI(SendDirectLongDataNoBuf)(LPSTR MidiHdrData, DWORD MidiHdrDataLen)
{
    return static_cast<decltype(SendDirectLongDataNoBuf)*>(MODULE()["SendDirectLongData"])(MidiHdrData, MidiHdrDataLen);
}

// Prepares the long data, and locks its memory to prevent apps from writing to it.
UINT KDMAPI(PrepareLongData)(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize)
{
    return static_cast<decltype(PrepareLongData)*>(MODULE()["PrepareLongData"])(IIMidiHdr, IIMidiHdrSize);
}

// Unlocks the memory, and unprepares the long data.
UINT KDMAPI(UnprepareLongData)(MIDIHDR* IIMidiHdr, UINT IIMidiHdrSize)
{
    return static_cast<decltype(UnprepareLongData)*>(MODULE()["UnprepareLongData"])(IIMidiHdr, IIMidiHdrSize);
}

// Get or set the current settings for the driver.
BOOL KDMAPI(DriverSettings)(DWORD Setting, DWORD Mode, LPVOID Value, UINT cbValue)
{
    return static_cast<decltype(DriverSettings)*>(MODULE()["DriverSettings"])(Setting, Mode, Value, cbValue);
}

// Get a pointer to the debug info of the driver.
KFUNCTION(DebugInfo*, GetDriverDebugInfo)

// Load a custom sflist. (You can also load SF2 and SFZ files)
VOID KDMAPI(LoadCustomSoundFontsList)(LPWSTR Directory)
{
    return static_cast<decltype(LoadCustomSoundFontsList)*>(MODULE()["LoadCustomSoundFontsList"])(Directory);
}

// timeGetTime, but 64-bit
KFUNCTION(DWORD64, timeGetTime64)
