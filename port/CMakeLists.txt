cmake_minimum_required(VERSION 3.20)
project(buraco_port VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
# GNUInstallDirs derives DOCDIR from the project name (buraco_port); the
# installed package is just "buraco".
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATADIR}/doc/buraco")

option(BURACO_BUILD_TESTS "Build the unit tests" ON)

# vcpkg: vcpkg install sdl2
# 2.0.18 is the first release with SDL_RenderSetVSync and SDL_GetTicks64.
find_package(SDL2 2.0.18 CONFIG REQUIRED)

# Game rules plus the text UI and the headless report helpers.  Everything
# except the SDL renderer and the entry point is here, so the tests can link
# the same code the executable runs.
add_library(buraco_core STATIC
    src/assets.cpp
    src/cards.cpp
    src/config.cpp
    src/game.cpp
    src/game_save.cpp
    src/i18n.cpp
    src/net.cpp
    src/net_protocol.cpp
    src/net_session.cpp
    src/options.cpp
    src/sound.cpp
    src/ui.cpp
    src/video.cpp
    src/window_scale.cpp
    src/window_state.cpp
    src/save.cpp
    src/report.cpp)

target_include_directories(buraco_core PUBLIC src)
target_link_libraries(buraco_core PUBLIC SDL2::SDL2)

# Where `cmake --install` puts the assets.  It is the last-resort lookup for an
# installed binary that is started from a directory outside the prefix.
target_compile_definitions(buraco_core PRIVATE
    BURACO_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/buraco/assets")

if(WIN32)
    # Winsock is part of the operating system, not an extra dependency.
    target_link_libraries(buraco_core PUBLIC ws2_32)
endif()

add_executable(buraco
    src/audio.cpp
    src/main.cpp
    src/renderer.cpp)

# SDL2::SDL2main is a Windows-only helper library and may not exist on other
# platforms.  When present it has to come before SDL2::SDL2 in the link line.
target_link_libraries(buraco PRIVATE buraco_core)
if(TARGET SDL2::SDL2main)
    target_link_libraries(buraco PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(buraco PRIVATE SDL2::SDL2)

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

# `cmake --install` produces the FHS layout used by Linux packages.  The build
# tree keeps the portable layout (assets next to the executable).
install(TARGETS buraco RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/buraco/assets)

# Desktop entry and icons, shared by `cmake --install` and the tarball built by
# packaging/linux/pack_linux.sh.  A source snapshot without the packaging
# directory still configures, so the rules are guarded by EXISTS.
set(BURACO_PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/linux")
if (EXISTS "${BURACO_PACKAGING_DIR}/buraco.desktop")
    install(FILES "${BURACO_PACKAGING_DIR}/buraco.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
if (EXISTS "${BURACO_PACKAGING_DIR}/buraco.svg")
    install(FILES "${BURACO_PACKAGING_DIR}/buraco.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
if (EXISTS "${BURACO_PACKAGING_DIR}/buraco-256.png")
    install(FILES "${BURACO_PACKAGING_DIR}/buraco-256.png"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME buraco.png)
endif()

# Documentation that belongs in /usr/share/doc (also used by the .deb).
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME README.md)
endif()
if (EXISTS "${BURACO_PACKAGING_DIR}/INSTALL.txt")
    install(FILES "${BURACO_PACKAGING_DIR}/INSTALL.txt"
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
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

# Bundle the MinGW runtime DLLs next to the executable so the release zip runs
# on machines that do not have the toolchain installed.  Only the DLLs the
# linker actually asks for are copied, and missing ones are skipped, so other
# toolchains (for example a static build) keep working unchanged.
if (MINGW)
    get_filename_component(BURACO_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach (BURACO_RUNTIME_DLL
             libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if (EXISTS "${BURACO_MINGW_BIN}/${BURACO_RUNTIME_DLL}")
            add_custom_command(TARGET buraco POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BURACO_MINGW_BIN}/${BURACO_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:buraco>/${BURACO_RUNTIME_DLL}")
        endif()
    endforeach()
endif()

# `cpack -G DEB` reuses the install rules above and produces
# buraco_<version>_<arch>.deb.  packaging/linux/pack_linux.sh --deb wraps it,
# and the tarball/AppImage are assembled by the scripts (they need files the
# install tree does not have, such as the SDL2 runtime for the AppImage).
if (UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "buraco")
    set(CPACK_PACKAGE_VENDOR "willbribeiro")
    set(CPACK_PACKAGE_CONTACT "Willian Ribeiro <140359443+willbribeiro@users.noreply.github.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Buraco card game with two decks and jokers (native SDL2 port)")
    # One line: CPack writes multi-line values with a stray ';' per newline.
    set(CPACK_PACKAGE_DESCRIPTION
        "Native C++20/SDL2 port of the MS-DOS game BURACO.EXE: two decks, jokers, canastras, hot-seat and LAN play, Portuguese and English interface, VGA 8x16 text mode look.")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/willbribeiro/Buraco-game")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0 (>= 2.0.18)")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    set(CPACK_PACKAGE_EXECUTABLES "buraco;Buraco")
    set(CPACK_STRIP_FILES TRUE)
    include(CPack)
endif()
