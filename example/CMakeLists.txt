cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 23)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fdiagnostics-color=always")
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif ()

add_executable(humanreadable HumanReadable.cpp)
target_link_libraries(humanreadable PUBLIC mfmidi)

#add_executable(trkplayer TrackPlayer.cpp)
#target_link_libraries(trkplayer PUBLIC mfmidi)
#
#add_executable(advplayer AdvancedPlayer.cpp)
#target_link_libraries(advplayer PUBLIC mfmidi)

add_executable(readonplay ReadOnPlay.cpp)
target_link_libraries(readonplay PUBLIC mfmidi)

add_executable(mt2trk MT2trk.cpp)
target_link_libraries(mt2trk PUBLIC mfmidi)

add_executable(events Events.cpp)
target_link_libraries(events PUBLIC mfmidi)
