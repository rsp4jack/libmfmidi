cmake_minimum_required(VERSION 3.0.0)
project(libmfmidi VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

include(CTest)
enable_testing()

option(HEADER_ONLY "Enable header-only" OFF)
option(ENABLE_UNSPEC_FEATURES "Enable unspecification features." OFF)
if(HEADER_ONLY)
    set(TYPE INTERFACE)
    set(PUB INTERFACE)
    set(PRI INTERFACE)
else()
    set(TYPE STATIC)
    set(PUB PUBLIC)
    set(PRI PRIVATE)
endif()

add_library(libmfmidi ${TYPE}
    include/libmfmidi/libmfmidi.hpp
    include/libmfmidi/mfutils.hpp
    include/libmfmidi/midimessage.hpp
    include/libmfmidi/midiutils.hpp
    include/libmfmidi/readerutils.hpp
    include/libmfmidi/midimessage.cpp
    include/libmfmidi/details/midimessageext.hpp
)

configure_file(include/libmfmidi/mfconfig.hpp.in mfconfig.hpp)

target_include_directories(libmfmidi ${PRI} ${CMAKE_CURRENT_BINARY_DIR} ${PUB} ${CMAKE_CURRENT_SOURCE_DIR}/include/libmfmidi)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

# execute_process(COMMAND "python.exe" "-m" "compdb" "-p" "build" "list" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}  ECHO_ERROR_VARIABLE ECHO_OUTPUT_VARIABLE)
