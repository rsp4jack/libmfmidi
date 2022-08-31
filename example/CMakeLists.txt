cmake_minimum_required(VERSION 3.0.0)
project(example)

set(CMAKE_CXX_STANDARD 20)

add_executable(humanreadable HumanReadable.cpp)

target_link_libraries(humanreadable PUBLIC libmfmidi)

