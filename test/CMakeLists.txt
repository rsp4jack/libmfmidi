enable_testing()

add_executable(nanosleep nanosleep.cpp)
target_link_libraries(nanosleep mfmidi)
add_test(NAME nanosleep COMMAND nanosleep)

add_executable(vlnview vlnview.cpp)
target_link_libraries(vlnview mfmidi)
add_test(NAME vlnview COMMAND vlnview)

add_executable(datatypes datatypes.cpp)
target_link_libraries(datatypes mfmidi)
add_test(NAME datatypes COMMAND datatypes)
