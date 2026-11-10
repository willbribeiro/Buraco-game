#include "config.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace config;

TEST_CASE("display config defaults match the historical 1280x800 window") {
    const Settings s = defaults();
    CHECK_EQ(s.version, 1);
    CHECK_EQ(s.display.fullscreen, false);
    CHECK_EQ(s.display.scale, 2);
    CHECK_EQ(s.display.windowWidth, 1280);
    CHECK_EQ(s.display.windowHeight, 800);
    CHECK_EQ(s.display.centered, true);
    CHECK_EQ(s.display.maximized, false);
    CHECK_EQ(s.display.integerScale, false);
    CHECK_EQ(s.display.vsync, true);
}

TEST_CASE("sound config defaults to enabled at 70%") {
    const Settings s = defaults();
    CHECK_EQ(s.sound.enabled, true);
    CHECK_EQ(s.sound.volume, 70);
}

TEST_CASE("display config serializes and parses every field") {
    Settings expected;
    expected.display.fullscreen = true;
    expected.display.scale = 0;
    expected.display.windowWidth = 1000;
    expected.display.windowHeight = 700;
    expected.display.centered = false;
    expected.display.windowX = -120;
    expected.display.windowY = 50;
    expected.display.maximized = true;
    expected.display.integerScale = true;
    expected.display.vsync = false;
    expected.sound.enabled = false;
    expected.sound.volume = 35;

    const std::string text = serialize(expected);
    CHECK(text.find("window_centered=0") != std::string::npos);
    CHECK(text.find("sound=0") != std::string::npos);
    CHECK(text.find("sound_volume=35") != std::string::npos);

    Settings parsed;
    std::string warning;
    CHECK(parse(text, parsed, &warning));
    CHECK(warning.empty());
    CHECK(parsed == expected);
}

TEST_CASE("display config keeps window_centered when x/y are also present") {
    Settings parsed;
    std::string warning;
    CHECK(parse("window_centered=1\nwindow_x=10\nwindow_y=20\n", parsed, &warning));
    CHECK(warning.empty());
    CHECK_EQ(parsed.display.centered, true);
    CHECK_EQ(parsed.display.windowX, 10);
    CHECK_EQ(parsed.display.windowY, 20);
}

TEST_CASE("display config treats x/y alone as an explicit position") {
    Settings parsed;
    CHECK(parse("window_x=10\n", parsed));
    CHECK_EQ(parsed.display.centered, false);
    CHECK_EQ(parsed.display.windowX, 10);
}

TEST_CASE("display config clamps every numeric field") {
    Settings parsed;
    CHECK(parse("scale=99\nwindow_width=1\nwindow_height=999999\n"
                "window_x=-999999\nwindow_y=999999\n",
                parsed));
    CHECK_EQ(parsed.display.scale, 8);
    CHECK_EQ(parsed.display.windowWidth, 640);
    CHECK_EQ(parsed.display.windowHeight, 4320);
    CHECK_EQ(parsed.display.windowX, -32768);
    CHECK_EQ(parsed.display.windowY, 32767);
}

TEST_CASE("display config reports invalid values but keeps the valid ones") {
    Settings parsed;
    std::string warning;
    CHECK(!parse("scale=abc\nvsync=off\n", parsed, &warning));
    CHECK(warning.find("scale") != std::string::npos);
    CHECK_EQ(parsed.display.scale, 2);  // default kept
    CHECK_EQ(parsed.display.vsync, false);
}

TEST_CASE("display config reports malformed lines") {
    Settings parsed;
    std::string warning;
    CHECK(!parse("not-a-key-value\n", parsed, &warning));
    CHECK(warning.find("key=value") != std::string::npos);
}

TEST_CASE("display config ignores unknown keys and comments") {
    Settings parsed;
    std::string warning;
    CHECK(parse("# comment\nfuture_option=42\nvsync=0  # trailing\n", parsed, &warning));
    CHECK(warning.empty());
    CHECK_EQ(parsed.display.vsync, false);
}

TEST_CASE("display config accepts boolean spellings") {
    for (const char* value : {"1", "true", "on", "yes", "sim"}) {
        Settings parsed;
        CHECK(parse(std::string("vsync=") + value + "\n", parsed));
        CHECK_EQ(parsed.display.vsync, true);
    }
    for (const char* value : {"0", "false", "off", "no", "nao"}) {
        Settings parsed;
        CHECK(parse(std::string("vsync=") + value + "\n", parsed));
        CHECK_EQ(parsed.display.vsync, false);
    }
}

TEST_CASE("sound config parses on/off and volume") {
    Settings parsed;
    std::string warning;
    CHECK(parse("sound=off\nsound_volume=25\n", parsed, &warning));
    CHECK(warning.empty());
    CHECK_EQ(parsed.sound.enabled, false);
    CHECK_EQ(parsed.sound.volume, 25);

    CHECK(parse("sound=sim\nsound_volume=0\n", parsed));
    CHECK_EQ(parsed.sound.enabled, true);
    CHECK_EQ(parsed.sound.volume, 0);
}

TEST_CASE("sound config clamps the volume to 0..100") {
    Settings parsed;
    CHECK(parse("sound_volume=250\n", parsed));
    CHECK_EQ(parsed.sound.volume, 100);

    CHECK(parse("sound_volume=-20\n", parsed));
    CHECK_EQ(parsed.sound.volume, 0);
}

TEST_CASE("sound config reports invalid values and keeps defaults") {
    Settings parsed;
    std::string warning;
    CHECK(!parse("sound=maybe\nsound_volume=abc\n", parsed, &warning));
    CHECK(warning.find("sound") != std::string::npos);
    CHECK(warning.find("sound_volume") != std::string::npos);
    CHECK_EQ(parsed.sound.enabled, true);
    CHECK_EQ(parsed.sound.volume, 70);
}

TEST_CASE("display config loads and saves a file") {
    const std::string path = testutil::tempSavePath("config_roundtrip.cfg");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path + ".tmp", ignored);

    Settings expected;
    expected.display.scale = 3;
    expected.display.windowWidth = 1920;
    expected.display.windowHeight = 1200;
    expected.display.integerScale = true;
    expected.display.vsync = false;
    expected.sound.enabled = false;
    expected.sound.volume = 45;

    std::string error;
    REQUIRE(save(path, expected, &error));
    CHECK(error.empty());
    CHECK(std::filesystem::exists(path));
    CHECK(!std::filesystem::exists(path + ".tmp"));

    Settings loaded;
    std::string warning;
    REQUIRE(load(path, loaded, &warning));
    CHECK(warning.empty());
    CHECK(loaded == expected);

    std::filesystem::remove(path, ignored);
}

TEST_CASE("display config missing file returns defaults") {
    const std::string path = testutil::tempSavePath("does_not_exist.cfg");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    Settings loaded;
    loaded.display.scale = 7;
    std::string warning;
    CHECK(!load(path, loaded, &warning));
    CHECK(loaded == defaults());
    CHECK(warning.find("cannot open") != std::string::npos);
}

TEST_CASE("display config normalize reports only real changes") {
    Settings settings = defaults();
    CHECK(!normalize(settings));

    settings.display.scale = 42;
    CHECK(normalize(settings));
    CHECK_EQ(settings.display.scale, 8);

    settings.sound.volume = 150;
    CHECK(normalize(settings));
    CHECK_EQ(settings.sound.volume, 100);
}
