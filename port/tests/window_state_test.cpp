#include "window_state.h"

#include "test_framework.hpp"

using namespace window_state;

namespace {

config::DisplaySettings at(int x, int y) {
    config::DisplaySettings settings;
    settings.centered = false;
    settings.windowX = x;
    settings.windowY = y;
    return settings;
}

const std::vector<window_scale::Rect> kDisplays{
    {0, 0, 1920, 1080},
    {-1920, 0, 1920, 1080},
};

}  // namespace

TEST_CASE("window state keeps a position that is still on a monitor") {
    config::DisplaySettings settings = at(100, 100);
    CHECK(positionVisible(settings, kDisplays));
    CHECK(!ensureVisible(settings, kDisplays));
    CHECK_EQ(settings.centered, false);
}

TEST_CASE("window state recenters a window saved on a missing monitor") {
    config::DisplaySettings settings = at(-5000, 100);
    CHECK(!positionVisible(settings, kDisplays));
    CHECK(ensureVisible(settings, kDisplays));
    CHECK_EQ(settings.centered, true);
    CHECK(!ensureVisible(settings, kDisplays));  // centered already
}

TEST_CASE("window state leaves centered windows and unknown displays alone") {
    config::DisplaySettings centered;
    CHECK(!ensureVisible(centered, kDisplays));

    config::DisplaySettings settings = at(-5000, 100);
    CHECK(!ensureVisible(settings, {}));
    CHECK(positionVisible(settings, {}));
}

TEST_CASE("window state records a user resize as a custom size") {
    config::DisplaySettings settings;
    CHECK(onResize(settings, 1200, 750, 1280, 800, false));
    CHECK_EQ(settings.scale, 0);
    CHECK_EQ(settings.windowWidth, 1200);
    CHECK_EQ(settings.windowHeight, 750);
    CHECK(!onResize(settings, 1200, 750, 1280, 800, false));  // unchanged
}

TEST_CASE("window state ignores its own resize and managed windows") {
    config::DisplaySettings settings;
    CHECK(!onResize(settings, 1280, 800, 1280, 800, false));
    CHECK_EQ(settings.scale, 2);

    CHECK(!onResize(settings, 1920, 1080, 1280, 800, true));  // fullscreen/maximized
    CHECK_EQ(settings.windowWidth, 1280);
    CHECK(!onResize(settings, 0, 0, 1280, 800, false));  // bogus event
}

TEST_CASE("window state clamps a tiny resize to the minimum window") {
    config::DisplaySettings settings;
    CHECK(onResize(settings, 100, 90, 1280, 800, false));
    CHECK_EQ(settings.windowWidth, window_scale::kMinWidth);
    CHECK_EQ(settings.windowHeight, window_scale::kMinHeight);
    CHECK_EQ(settings.scale, 0);
}

TEST_CASE("window state records a user move as an explicit position") {
    config::DisplaySettings settings;
    CHECK(onMove(settings, 50, 60, 960, 320, false));
    CHECK_EQ(settings.centered, false);
    CHECK_EQ(settings.windowX, 50);
    CHECK_EQ(settings.windowY, 60);
    CHECK(!onMove(settings, 50, 60, 960, 320, false));  // unchanged
    CHECK(!onMove(settings, 70, 80, 960, 320, true));   // fullscreen/maximized
}

TEST_CASE("window state ignores the position the window opened at") {
    config::DisplaySettings settings;
    CHECK(!onMove(settings, 960, 320, 960, 320, false));
    CHECK_EQ(settings.centered, true);
}

TEST_CASE("window state tracks maximize and restore") {
    config::DisplaySettings settings;
    CHECK(!onMaximized(settings, false));
    CHECK(onMaximized(settings, true));
    CHECK_EQ(settings.maximized, true);
    CHECK(onMaximized(settings, false));
    CHECK_EQ(settings.maximized, false);
}
