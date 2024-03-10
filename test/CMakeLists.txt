enable_testing()

add_executable(nanosleep nanosleep.cpp)
target_link_libraries(nanosleep libmfmidi)
add_test(NAME nanosleep COMMAND nanosleep)
