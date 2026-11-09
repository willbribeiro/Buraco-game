#include "window_scale.h"

#include "test_framework.hpp"

using namespace window_scale;

namespace {

Size size(int width, int height) { return Size{width, height}; }

}  // namespace

TEST_CASE("window scale presets are multiples of the 640x400 canvas") {
    CHECK(sizeForScale(1) == size(640, 400));
    CHECK(sizeForScale(2) == size(1280, 800));
    CHECK(sizeForScale(3) == size(1920, 1200));
    CHECK(sizeForScale(8) == size(5120, 3200));
    CHECK(sizeForScale(0) == size(640, 400));  // clamped to 1x
}

TEST_CASE("window size keeps 2x when it fits the display") {
    CHECK(windowSize(1280, 800, 2, 1920, 1040) == size(1280, 800));
    CHECK(windowSize(1280, 800, 2, 1366, 768) == size(1106, 691));
}

TEST_CASE("window size shrinks to 90% of a small usable area") {
    const Size fitted = windowSize(1280, 800, 2, 1366, 768);
    CHECK(fitted.width <= 1229);
    CHECK(fitted.height <= 691);
    CHECK(fitted.width >= kMinWidth);
    CHECK(fitted.height >= kMinHeight);
    const double ratio = double(fitted.width) / double(fitted.height);
    CHECK(ratio > 1.59);
    CHECK(ratio < 1.61);
}

TEST_CASE("window size honours a saved custom size") {
    CHECK(windowSize(1280, 800, 0, 1920, 1040) == size(1280, 800));
    CHECK(windowSize(1000, 700, 0, 1920, 1040) == size(1000, 700));
}

TEST_CASE("window size clamps below the minimum window") {
    CHECK(clampSize(100, 100) == size(640, 400));
    CHECK(clampSize(99999, 99999) == size(7680, 4320));
    CHECK(fitToUsable(640, 400, 100, 100) == size(640, 400));
    CHECK(fitToUsable(1280, 800, 0, 0) == size(1280, 800));
}

TEST_CASE("window size reduces an explicit scale that does not fit") {
    const Size fitted = windowSize(1280, 800, 3, 1920, 1080);
    CHECK(fitted.width <= 1728);
    CHECK(fitted.height <= 972);
    CHECK(fitted == size(1555, 972));
}

TEST_CASE("integer scale is limited by the usable display area") {
    CHECK_EQ(maxIntegerScale(1366, 768), 1);
    CHECK_EQ(maxIntegerScale(1280, 800), 2);
    CHECK_EQ(maxIntegerScale(1920, 1080), 2);
    CHECK_EQ(maxIntegerScale(3840, 2160), 5);
    CHECK_EQ(maxIntegerScale(600, 300), 0);
    CHECK_EQ(maxIntegerScale(0, 0), 0);
}

TEST_CASE("window visibility uses any positive overlap with a display") {
    const std::vector<Rect> displays{{0, 0, 1920, 1080}, {-1920, 0, 1920, 1080}};
    CHECK(isVisible(Rect{100, 100, 640, 400}, displays));
    CHECK(isVisible(Rect{-100, 100, 640, 400}, displays));
    CHECK(isVisible(Rect{-2000, 100, 640, 400}, displays));
    CHECK(!isVisible(Rect{-3000, 100, 640, 400}, displays));
    CHECK(!isVisible(Rect{0, 2000, 640, 400}, displays));
}
