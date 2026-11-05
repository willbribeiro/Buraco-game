cmake_minimum_required(VERSION 3.20)
project(buraco_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2 (required by game code)
find_package(SDL2 CONFIG QUIET)
if (NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 not found. Install via: vcpkg install sdl2:x64-windows")
endif()

# Find Catch2 (test framework) - optional if not installed
find_package(Catch2 QUIET)
if (Catch2_FOUND AND NOT TARGET Catch2::Catch2WithMain)
    # If only Catch2 headers found, add main manually
    find_library(CATCH2_LIB Catch2 PATHS "$ENV{VCPKG_ROOT}/installed/*/lib")
endif()

# Create test executable with Catch2 main
add_executable(buraco_test 
    catch_main.hpp  # Provides CATCH2_TEST_MAIN macro and Catch2::Catch2WithMain target
    tests/buraco.cpp 
    tests/cards.cpp 
    tests/game.cpp
)

if (Catch2_FOUND AND TARGET Catch2::Catch2WithMain)
    target_link_libraries(buraco_test PRIVATE SDL2::SDL2main SDL2::SDL2 Catch2::Catch2WithMain)
else()
    # Fallback for manual Catch2 setup
    target_link_libraries(buraco_test PRIVATE SDL2::SDL2main SDL2::SDL2)
endif()

if (MSVC)
    target_compile_options(buraco_test PRIVATE /W4 /permissive-)
    target_compile_definitions(buraco_test PRIVATE _CRT_SECURE_NO_WARNINGS DETERMINISTIC_TEST)
else()
    target_compile_options(buraco_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    set_target_properties(buraco_test PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Install test executable
install(TARGETS buraco_test DESTINATION bin)
