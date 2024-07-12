cmake_minimum_required(VERSION 3.15.0)
project(libmfmidi VERSION 0.1.0)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fdiagnostics-color=always")
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif ()

# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
# add_definitions(-D__RTMIDI_DEBUG__)

# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
# endif()

# if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
# endif()
# endif()


# --- KDMAPI (OmniMIDI)
if (WIN32)
    add_library(KDMAPI STATIC include/omnimidi/omnimidiapi.cpp)
    target_link_libraries(KDMAPI PUBLIC winmm)
endif ()

set(RTMIDI_BUILD_STATIC_LIBS ON)

include(FetchContent)
FetchContent_Declare(
        libremidi
        GIT_REPOSITORY https://github.com/celtera/libremidi.git
        GIT_TAG v4.5.0
)

FetchContent_MakeAvailable(libremidi)

# --- mfmidi

set(mfmidi_win32_sources src/mfmidi/kdmapi_device.cpp include/mfmidi/device/kdmapi_device.hpp)

add_library(mfmidi STATIC
        include/mfmidi/midi_device.hpp
        include/mfmidi/mfmidi.hpp
        include/mfmidi/mfutility.hpp
        include/mfmidi/track_player.hpp
        include/mfmidi/midi_message.hpp
        include/mfmidi/smf/span_track.hpp
        include/mfmidi/midi_status.hpp
        include/mfmidi/midi_utility.hpp
        include/mfmidi/dummy.cpp
        include/mfmidi/timingapi.hpp
        include/mfmidi/device/libremidi_device.hpp
        include/mfmidi/event.hpp
        include/mfmidi/smf/smf.hpp
        include/mfmidi/smf/variable_number.hpp
        include/mfmidi/smf/division.hpp
        include/mfmidi/midi_tempo.hpp
        include/mfmidi/smf/smf_error.hpp
        include/mfmidi/midi_events.hpp
        include/mfmidi/smf.hpp
        include/mfmidi/devices.hpp

        src/mfmidi/platformapi.cpp
        src/mfmidi/smf_error.cpp

        ${mfmidi_win32_sources}
)

if (WIN32)
    #    target_precompile_headers(mfmidi PUBLIC <Windows.h>)
    #    target_precompile_headers(KDMAPI PUBLIC <Windows.h>)

    target_link_libraries(mfmidi PRIVATE KDMAPI)
endif ()
target_include_directories(mfmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mfmidi PUBLIC libremidi)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(mfmidi PUBLIC stdc++exp)
endif ()

include(CPack)
if (BUILD_TESTING)
    add_subdirectory(test)
endif ()

add_subdirectory(example)

# execute_process(COMMAND "python.exe" "-m" "compdb" "-p" "build" "list" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}  ECHO_ERROR_VARIABLE ECHO_OUTPUT_VARIABLE)
