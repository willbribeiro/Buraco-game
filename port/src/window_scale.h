// Pure window-size helpers.
//
// The SDL window is a frame around the fixed 640x400 logical canvas, so the
// only decision this module makes is how large that frame should be.  Keeping
// the arithmetic out of the renderer makes the edge cases (small displays,
// saved sizes, pixel-perfect mode) testable without opening a window.
#pragma once

#include <vector>

namespace window_scale {

constexpr int kBaseWidth = 640;
constexpr int kBaseHeight = 400;
constexpr int kMinWidth = 640;
constexpr int kMinHeight = 400;
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;

struct Size {
    int width = 0;
    int height = 0;

    friend bool operator==(const Size&, const Size&) = default;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Keeps a window inside the range the renderer accepts.
Size clampSize(int width, int height);

// Exact size of a 640x400 canvas at an integer scale.
Size sizeForScale(int scale);

// Shrinks a window to at most 90% of the usable display area, preserving its
// aspect ratio.  When the display bounds are unknown (<= 0), only clamps.
Size fitToUsable(int width, int height, int usableWidth, int usableHeight);

// The startup size: an explicit scale (1..8), or the saved width/height when
// scale is 0, fitted to the usable display area.
Size windowSize(int configuredWidth, int configuredHeight, int scale,
                int usableWidth, int usableHeight);

// Largest integer scale that fits the usable area, or 0 when even 1x does not.
int maxIntegerScale(int usableWidth, int usableHeight);

// True when the window has any positive overlap with one of the displays.
bool isVisible(const Rect& window, const std::vector<Rect>& displays);

}  // namespace window_scale
