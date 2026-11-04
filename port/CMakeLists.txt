cmake_minimum_required(VERSION 3.20)
project(buraco_port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg: vcpkg install sdl2
find_package(SDL2 CONFIG REQUIRED)

add_executable(buraco
    src/main.cpp
    src/cards.cpp
    src/game.cpp)

target_link_libraries(buraco PRIVATE SDL2::SDL2main SDL2::SDL2)

if (MSVC)
    target_compile_options(buraco PRIVATE /W4 /permissive-)
    target_compile_definitions(buraco PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(buraco PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    set_target_properties(buraco PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Copy the extracted VGA font (and later sprites/maps) next to the executable.
add_custom_command(TARGET buraco POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:buraco>/assets")
