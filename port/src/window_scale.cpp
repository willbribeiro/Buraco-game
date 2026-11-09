#include "window_scale.h"

#include <algorithm>
#include <cmath>

namespace window_scale {

Size clampSize(int width, int height) {
    return Size{std::clamp(width, kMinWidth, kMaxWidth),
                std::clamp(height, kMinHeight, kMaxHeight)};
}

Size sizeForScale(int scale) {
    const int s = std::clamp(scale, 1, 8);
    return clampSize(kBaseWidth * s, kBaseHeight * s);
}

Size fitToUsable(int width, int height, int usableWidth, int usableHeight) {
    Size size = clampSize(width, height);
    if (usableWidth <= 0 || usableHeight <= 0) return size;

    const double maxWidth = double(usableWidth) * 0.90;
    const double maxHeight = double(usableHeight) * 0.90;
    double factor = 1.0;
    if (double(size.width) > maxWidth) factor = std::min(factor, maxWidth / size.width);
    if (double(size.height) > maxHeight) factor = std::min(factor, maxHeight / size.height);
    if (factor >= 1.0) return size;

    size.width = int(std::lround(double(size.width) * factor));
    size.height = int(std::lround(double(size.height) * factor));
    return clampSize(size.width, size.height);
}

Size windowSize(int configuredWidth, int configuredHeight, int scale,
                int usableWidth, int usableHeight) {
    const Size base = scale >= 1 ? sizeForScale(scale)
                                 : clampSize(configuredWidth, configuredHeight);
    return fitToUsable(base.width, base.height, usableWidth, usableHeight);
}

int maxIntegerScale(int usableWidth, int usableHeight) {
    if (usableWidth <= 0 || usableHeight <= 0) return 0;
    return std::max(0, std::min(usableWidth / kBaseWidth, usableHeight / kBaseHeight));
}

bool isVisible(const Rect& window, const std::vector<Rect>& displays) {
    for (const Rect& display : displays) {
        const bool overlap =
            window.x < display.x + display.width &&
            window.x + window.width > display.x &&
            window.y < display.y + display.height &&
            window.y + window.height > display.y;
        if (overlap) return true;
    }
    return false;
}

}  // namespace window_scale
