#include "window_state.h"

namespace window_state {
namespace {

window_scale::Size configuredSize(const config::DisplaySettings& settings) {
    if (settings.scale >= 1) return window_scale::sizeForScale(settings.scale);
    return window_scale::clampSize(settings.windowWidth, settings.windowHeight);
}

}  // namespace

bool positionVisible(const config::DisplaySettings& settings,
                     const std::vector<window_scale::Rect>& displays) {
    if (displays.empty()) return true;
    const window_scale::Size size = configuredSize(settings);
    return window_scale::isVisible(
        window_scale::Rect{settings.windowX, settings.windowY, size.width, size.height},
        displays);
}

bool ensureVisible(config::DisplaySettings& settings,
                   const std::vector<window_scale::Rect>& displays) {
    if (settings.centered || displays.empty()) return false;
    if (positionVisible(settings, displays)) return false;
    settings.centered = true;
    return true;
}

bool onResize(config::DisplaySettings& settings, int width, int height,
              int appliedWidth, int appliedHeight, bool managed) {
    if (managed || width <= 0 || height <= 0) return false;
    if (width == appliedWidth && height == appliedHeight) return false;

    const window_scale::Size size = window_scale::clampSize(width, height);
    if (size.width == settings.windowWidth && size.height == settings.windowHeight &&
        settings.scale == 0)
        return false;

    settings.scale = 0;
    settings.windowWidth = size.width;
    settings.windowHeight = size.height;
    return true;
}

bool onMove(config::DisplaySettings& settings, int x, int y, int appliedX,
            int appliedY, bool managed) {
    if (managed) return false;
    if (x == appliedX && y == appliedY) return false;
    if (!settings.centered && x == settings.windowX && y == settings.windowY) return false;

    settings.centered = false;
    settings.windowX = x;
    settings.windowY = y;
    return true;
}

bool onMaximized(config::DisplaySettings& settings, bool maximized) {
    if (settings.maximized == maximized) return false;
    settings.maximized = maximized;
    return true;
}

}  // namespace window_state
