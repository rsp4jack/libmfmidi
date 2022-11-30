cmake_minimum_required(VERSION 3.2.0)
project(libmfmidi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
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

# --- RtMidi macros
# Check for Jack (any OS)
find_library(JACK_LIB jack)
find_package(PkgConfig)
pkg_check_modules(jack jack)

if(JACK_LIB OR jack_FOUND)
    set(HAVE_JACK TRUE)
endif()

option(RTMIDI_API_JACK "Compile with JACK support." ${HAVE_JACK})

if(UNIX AND NOT APPLE)
    option(RTMIDI_API_ALSA "Compile with ALSA support." ON)
endif()

option(RTMIDI_API_WINMM "Compile with WINMM support." ${WIN32})
option(RTMIDI_API_CORE "Compile with CoreMIDI support." ${APPLE})

# Jack
if(RTMIDI_API_JACK)
    if(NOT HAVE_JACK)
        message(FATAL_ERROR "Jack API requested but no Jack dev libraries found")
    endif()

    set(NEED_PTHREAD ON)
    list(APPEND PKGCONFIG_REQUIRES "jack")
    list(APPEND API_DEFS "-D__UNIX_JACK__")
    list(APPEND API_LIST "jack")

    if(jack_FOUND)
        list(APPEND LINKLIBS ${jack_LIBRARIES})
        list(APPEND INCDIRS ${jack_INCLUDEDIR})
    else()
        list(APPEND LINKLIBS ${JACK_LIB})
    endif()

    # Check for jack_port_rename
    include(CheckSymbolExists)
    set(tmp_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    list(APPEND CMAKE_REQUIRED_LIBRARIES jack)
    check_symbol_exists(jack_port_rename ${jack_INCLUDEDIR}/jack/jack.h JACK_HAS_PORT_RENAME)
    set(CMAKE_REQUIRED_LIBRARIES ${tmp_CMAKE_REQUIRED_LIBRARIES})

    if(JACK_HAS_PORT_RENAME)
        list(APPEND API_DEFS "-DJACK_HAS_PORT_RENAME")
    endif()
endif()

# ALSA
if(RTMIDI_API_ALSA)
    set(NEED_PTHREAD ON)
    find_package(ALSA)

    if(NOT ALSA_FOUND)
        message(FATAL_ERROR "ALSA API requested but no ALSA dev libraries found")
    endif()

    list(APPEND INCDIRS ${ALSA_INCLUDE_DIR})
    list(APPEND LINKLIBS ALSA::ALSA)
    list(APPEND PKGCONFIG_REQUIRES "alsa")
    list(APPEND API_DEFS "-D__LINUX_ALSA__")
    list(APPEND API_LIST "alsa")
    list(APPEND PACKAGE_DEPENDENCIES "find_dependency(ALSA)")
endif()

# WinMM
if(RTMIDI_API_WINMM)
    list(APPEND API_DEFS "-D__WINDOWS_MM__")
    list(APPEND API_LIST "winmm")
    list(APPEND LINKLIBS winmm)
endif()

# CoreMIDI
if(RTMIDI_API_CORE)
    find_library(CORESERVICES_LIB CoreServices)
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(COREMIDI_LIB CoreMIDI)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    list(APPEND API_DEFS "-D__MACOSX_CORE__")
    list(APPEND API_LIST "coremidi")
    list(APPEND LINKLIBS ${CORESERVICES_LIB} ${COREAUDIO_LIB} ${COREMIDI_LIB} ${COREFOUNDATION_LIB})
    list(APPEND LIBS_REQUIRES "-framework CoreServices -framework CoreAudio -framework CoreMIDI -framework CoreFoundation")
    list(APPEND LINKFLAGS "-Wl,-F/Library/Frameworks")
endif()

# pthread
if(NEED_PTHREAD)
    find_package(Threads REQUIRED
        CMAKE_THREAD_PREFER_PTHREAD
        THREADS_PREFER_PTHREAD_FLAG)
    list(APPEND PUBLICLINKLIBS Threads::Threads)
    list(APPEND PACKAGE_DEPENDENCIES "find_dependency(Threads)")
endif()

add_library(rtmidi STATIC src/RtMidi.cpp)
target_link_libraries(rtmidi PUBLIC ${PUBLICLINKLIBS}
    PRIVATE ${LINKLIBS})
target_include_directories(rtmidi PRIVATE ${INCDIRS}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_include_directories(rtmidi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/rtmidi)
target_compile_definitions(rtmidi PRIVATE ${API_DEFS})
target_include_directories(rtmidi PRIVATE ${INCDIRS})

# --- KDMAPI (OmniMIDI)
add_library(KDMAPI STATIC include/omnimidi/omnimidiapi.cpp)
target_link_libraries(KDMAPI PUBLIC winmm)
target_precompile_headers(KDMAPI PUBLIC <Windows.h>)

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
    include/libmfmidi/midimatrix.hpp
    include/libmfmidi/midimessage.hpp
    include/libmfmidi/midimessagefdc.hpp
    include/libmfmidi/midimultitrack.hpp
    include/libmfmidi/midimultitrackcursor.hpp
    include/libmfmidi/midinotifier.hpp
    include/libmfmidi/midiprocessor.hpp
    include/libmfmidi/midisequencer.hpp
    include/libmfmidi/midistate.hpp
    include/libmfmidi/miditrack.hpp
    include/libmfmidi/miditrackplayer.hpp
    include/libmfmidi/midiutils.hpp
    include/libmfmidi/nanosleep.cpp
    include/libmfmidi/nanosleep.hpp
    include/libmfmidi/posixtimer.hpp
    include/libmfmidi/rtmididevice.hpp
    include/libmfmidi/samhandlers.hpp
    include/libmfmidi/simpletrackplayer.hpp
    include/libmfmidi/smffile.hpp
    include/libmfmidi/smfreader.hpp
    include/libmfmidi/smfreaderpolicy.hpp
    include/libmfmidi/win32mmtimer.hpp
)

target_precompile_headers(libmfmidi PUBLIC <Windows.h>)
target_include_directories(libmfmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(libmfmidi PRIVATE rtmidi KDMAPI)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(example)

# execute_process(COMMAND "python.exe" "-m" "compdb" "-p" "build" "list" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}  ECHO_ERROR_VARIABLE ECHO_OUTPUT_VARIABLE)
