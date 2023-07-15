cmake_minimum_required(VERSION 3.15.0)
project(example)

set(CMAKE_CXX_STANDARD 23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fdiagnostics-color=always")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

add_executable(humanreadable HumanReadable.cpp)
target_link_libraries(humanreadable PUBLIC libmfmidi)

add_executable(trkplayer TrackPlayer.cpp)
target_link_libraries(trkplayer PUBLIC libmfmidi)

add_executable(advplayer AdvancedPlayer.cpp)
target_link_libraries(advplayer PUBLIC libmfmidi)

add_executable(readonplay ReadOnPlay.cpp)
target_link_libraries(readonplay PUBLIC libmfmidi)

add_executable(mt2trk MT2trk.cpp)
target_link_libraries(mt2trk PUBLIC libmfmidi)
