cmake_minimum_required(VERSION 3.20)
project(buraco_port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BURACO_BUILD_TESTS "Build the unit tests" ON)

# vcpkg: vcpkg install sdl2
find_package(SDL2 CONFIG REQUIRED)

# Game rules plus the text UI and the headless report helpers.  Everything
# except the SDL renderer and the entry point is here, so the tests can link
# the same code the executable runs.
add_library(buraco_core STATIC
    src/cards.cpp
    src/game.cpp
    src/game_save.cpp
    src/ui.cpp
    src/video.cpp
    src/save.cpp
    src/report.cpp)

target_include_directories(buraco_core PUBLIC src)
target_link_libraries(buraco_core PUBLIC SDL2::SDL2)

add_executable(buraco
    src/main.cpp
    src/renderer.cpp)

target_link_libraries(buraco PRIVATE buraco_core SDL2::SDL2main SDL2::SDL2)

if (MSVC)
    target_compile_options(buraco_core PRIVATE /W4 /permissive-)
    target_compile_options(buraco PRIVATE /W4 /permissive-)
    target_compile_definitions(buraco_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(buraco PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(buraco_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(buraco PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    set_target_properties(buraco PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (BURACO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Copy the extracted VGA font (and later sprites/maps) next to the executable.
add_custom_command(TARGET buraco POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:buraco>/assets")
