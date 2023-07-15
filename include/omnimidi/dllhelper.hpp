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

#include <stdexcept>
#include <type_traits>
#include <Windows.h>
#include <unordered_map>

// https://github.com/bblanchon/dllhelper/blob/master/dllhelper.hpp
// Edited
// MIT License

class ProcPtr {
public:
    explicit ProcPtr(FARPROC ptr)
        : _ptr(ptr)
    {
    }

    template <typename T>
    requires std::is_function_v<T>
    explicit operator T*() const
    {
        return reinterpret_cast<T*>(_ptr);
    }

private:
    FARPROC _ptr;
};

class DllHelper {
public:
    explicit DllHelper(LPCTSTR filename)
    {
        HMODULE result = GetModuleHandle(filename);
        if (result == nullptr) {
            result = LoadLibraryEx(filename, nullptr, 0);
            if (result == nullptr) {
                throw std::runtime_error("Failed to load library");
            }
            tfree = true;
        }
        _module = result;
    }

    ~DllHelper()
    {
        if(tfree){
            FreeLibrary(_module);
            procmap.clear();
        }
    }

    ProcPtr operator[](LPCSTR proc_name)
    {
        if (procmap.contains(proc_name)) {
            return ProcPtr{procmap.at(proc_name)};
        }
        return ProcPtr{procmap[proc_name] = GetProcAddress(_module, proc_name)};
    }

private:
    HMODULE _module;
    bool    tfree = false;
    std::unordered_map<LPCSTR, FARPROC> procmap;
};

