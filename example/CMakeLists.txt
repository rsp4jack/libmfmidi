cmake_minimum_required(VERSION 3.0.0)
project(example)

set(CMAKE_CXX_STANDARD 23)

add_executable(humanreadable HumanReadable.cpp)
target_link_libraries(humanreadable PUBLIC libmfmidi)

add_executable(trkplayer TrackPlayer.cpp)
target_link_libraries(trkplayer PUBLIC libmfmidi)

add_executable(advplayer AdvancedPlayer.cpp)
target_link_libraries(advplayer PUBLIC libmfmidi)
