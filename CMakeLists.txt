cmake_minimum_required(VERSION 3.25)
project(OrbitCourier VERSION 0.1.0 DESCRIPTION "Gravity-assist delivery game" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OC_BUILD_GAME "Build the game executable (downloads raylib)" ON)
option(OC_BUILD_TESTS "Build the unit tests (downloads doctest)" ON)

include(FetchContent)

# Strict warnings for our own code only (not for third-party libraries).
add_library(oc_warnings INTERFACE)
if(MSVC)
    target_compile_options(oc_warnings INTERFACE /W4 /WX /permissive- /utf-8)
else()
    # -ffp-contract=off: no fused multiply-add, so the simulation gives the same numbers on every compiler.
    # -Wno-missing-field-initializers: designated initializers may skip members that have defaults (C++20 style).
    target_compile_options(oc_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror -ffp-contract=off
        -Wno-missing-field-initializers)
endif()

add_subdirectory(src/core)

if(OC_BUILD_GAME)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    if(EMSCRIPTEN)
        set(PLATFORM Web CACHE STRING "" FORCE)
    endif()
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz
        SYSTEM
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(raylib)
    add_subdirectory(src/game)
endif()

if(OC_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
        SYSTEM
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(doctest)
    add_subdirectory(tests)
endif()
