# Game logic without any graphics dependency: unit-testable and deterministic.
add_library(oc_core
    src/world.cpp
    src/levels.cpp
    src/mission.cpp)
target_include_directories(oc_core PUBLIC include)
target_compile_features(oc_core PUBLIC cxx_std_20)
target_link_libraries(oc_core PRIVATE oc_warnings)
