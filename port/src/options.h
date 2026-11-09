// F9 options screen state and key handling.
//
// The screen only edits config::DisplaySettings; rendering lives in ui.cpp and
// applying the result is the caller's job (Renderer::apply).  Keeping the
// navigation here makes it testable without SDL or a game in progress.
#pragma once

#include "config.h"

#include <string>

namespace options {

enum class Row {
    Scale,         // 0 = automatic/saved size, 1..8 = 640x400 * scale
    Adjustment,    // integer (pixel-perfect) or stretched
    Fullscreen,
    Vsync,
    Maximized,
    Center,        // action: center the window
    Reset,         // action: restore display defaults
    Close,         // action: leave the screen
    Count,
};

constexpr int kRowCount = int(Row::Count);

struct State {
    int selected = 0;
};

enum class Key { Up, Down, Left, Right, Enter, Escape };

enum class Action {
    None,
    Apply,   // settings changed, call Renderer::apply
    Reset,   // display defaults restored, call Renderer::apply
    Close,   // leave the options screen
};

// Moves the selection and edits the row under it.  Arrow keys change values;
// ENTER activates the row (toggles, centers, resets or closes).
Action handleKey(State& state, Key key, config::DisplaySettings& settings);

std::string label(Row row);
std::string value(Row row, const config::DisplaySettings& settings);
bool isAction(Row row);

}  // namespace options
