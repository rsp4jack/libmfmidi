cmake_minimum_required(VERSION 3.15.0)
project(libmfmidi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fdiagnostics-color=always")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

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
add_library(KDMAPI STATIC include/omnimidi/omnimidiapi.cpp)
target_link_libraries(KDMAPI PUBLIC winmm)
target_precompile_headers(KDMAPI PUBLIC <Windows.h>)

find_package(fmt REQUIRED)
find_package(tl-expected REQUIRED)
find_package(rtmidi REQUIRED)

# --- libmfmidi

add_library(libmfmidi STATIC
    include/libmfmidi/abstractmididevice.hpp
    include/libmfmidi/abstractsamhandler.hpp
    include/libmfmidi/abstracttimer.hpp
    include/libmfmidi/kdmapidevice.hpp
    include/libmfmidi/libmfmidi.hpp
    include/libmfmidi/mfconcepts.hpp
    include/libmfmidi/mfexceptions.hpp
    include/libmfmidi/mfutils.hpp
    include/libmfmidi/midiadvancedtrackplayer.hpp
    include/libmfmidi/midimatrix.hpp
    include/libmfmidi/midimessage.hpp
    include/libmfmidi/midimessagefdc.hpp
    include/libmfmidi/midimultitrack.hpp
    include/libmfmidi/midimultitrackcursor.hpp
    include/libmfmidi/midinotifier.hpp
    include/libmfmidi/midiprocessor.hpp
    include/libmfmidi/midireadonplay.hpp
    include/libmfmidi/midistatus.hpp
    include/libmfmidi/miditrack.hpp
    include/libmfmidi/miditrackplayer.hpp
    include/libmfmidi/miditrackplayerlooply.hpp
    include/libmfmidi/midiutils.hpp
    include/libmfmidi/dummy.cpp
    include/libmfmidi/platformapi.hpp
    include/libmfmidi/posixtimer.hpp
    include/libmfmidi/rtmididevice.hpp
    include/libmfmidi/samhandlers.hpp
    include/libmfmidi/smffile.hpp
    include/libmfmidi/smfreader.hpp
    include/libmfmidi/smfreaderpolicy.hpp
    include/libmfmidi/win32mmtimer.hpp
    src/libmfmidi/platformapi.cpp
)

if(WIN32)
    target_precompile_headers(libmfmidi PUBLIC <Windows.h>)
endif()
target_include_directories(libmfmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(libmfmidi PRIVATE RtMidi::rtmidi KDMAPI PUBLIC fmt::fmt tl::expected)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(example)

# execute_process(COMMAND "python.exe" "-m" "compdb" "-p" "build" "list" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}  ECHO_ERROR_VARIABLE ECHO_OUTPUT_VARIABLE)
