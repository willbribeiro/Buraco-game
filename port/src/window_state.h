// Window state helpers.
//
// The event loop and the renderer both need to turn SDL window events into
// configuration changes.  Keeping that arithmetic free of SDL makes the rules
// (ignore events caused by our own resize, never overwrite geometry while the
// window manager owns it, recenter a window saved on a monitor that is gone)
// testable without opening a window.
#pragma once

#include "config.h"
#include "window_scale.h"

#include <vector>

namespace window_state {

// True when the saved position intersects at least one display.  An empty
// display list means "unknown" and keeps the saved position.
bool positionVisible(const config::DisplaySettings& settings,
                     const std::vector<window_scale::Rect>& displays);

// Sets `centered` when the saved position is no longer on any display.
// Returns true when the settings changed.
bool ensureVisible(config::DisplaySettings& settings,
                   const std::vector<window_scale::Rect>& displays);

// Records a user resize as an explicit window size (clearing `scale`).
// `managed` is true while fullscreen or maximized, when the window manager
// owns the geometry; sizes equal to the size we applied ourselves are ignored
// so simply showing the window does not rewrite the configuration.
bool onResize(config::DisplaySettings& settings, int width, int height,
              int appliedWidth, int appliedHeight, bool managed);

// Records a user move as an explicit position (clearing `centered`).
// Positions equal to the one we applied ourselves are ignored.
bool onMove(config::DisplaySettings& settings, int x, int y, int appliedX,
            int appliedY, bool managed);

// Records maximize/restore transitions.
bool onMaximized(config::DisplaySettings& settings, bool maximized);

}  // namespace window_state
