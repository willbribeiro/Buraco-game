# Dependency-free unit tests.  They link buraco_core from the parent directory,
# so a test always exercises the same code the game ships.
add_executable(buraco_tests
    test_framework.cpp
    cards_test.cpp
    game_test.cpp
    save_test.cpp
    report_test.cpp
    ui_test.cpp)

target_link_libraries(buraco_tests PRIVATE buraco_core)

if (MSVC)
    target_compile_options(buraco_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(buraco_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(buraco_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Console application: the harness prints its report to stdout.
set_target_properties(buraco_tests PROPERTIES WIN32_EXECUTABLE FALSE)

# The tests link SDL2 through buraco_core, so the DLL must sit next to the
# executable when it is run from the build tree.
add_custom_command(TARGET buraco_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL2::SDL2>"
        "$<TARGET_FILE_DIR:buraco_tests>")

add_test(NAME buraco_tests COMMAND buraco_tests)
