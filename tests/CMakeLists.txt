add_executable(oc_tests main.cpp world_tests.cpp mission_tests.cpp level_tests.cpp progress_tests.cpp)
target_link_libraries(oc_tests PRIVATE oc_core oc_warnings doctest::doctest)
add_test(NAME oc_tests COMMAND oc_tests)
