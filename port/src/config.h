// Persistent user configuration.
//
// The file is a small, line-oriented `key=value` document so it can be edited
// by hand and diffed.  Unknown keys are ignored and every value is clamped to a
// safe range, so older builds keep reading newer files.
#pragma once

#include "i18n.h"

#include <string>

namespace config {

struct DisplaySettings {
    bool fullscreen = false;
    int scale = 2;              // 0 = custom size, 1..8 = 640x400 * scale
    int windowWidth = 1280;
    int windowHeight = 800;
    bool centered = true;       // false => use windowX/windowY
    int windowX = 0;
    int windowY = 0;
    bool maximized = false;
    bool integerScale = false;  // pixel-perfect (SDL_RenderSetIntegerScale)
    bool vsync = true;

    friend bool operator==(const DisplaySettings&, const DisplaySettings&) = default;
};

struct SoundSettings {
    bool enabled = true;
    int volume = 70;  // 0..100

    friend bool operator==(const SoundSettings&, const SoundSettings&) = default;
};

struct Settings {
    int version = 1;
    DisplaySettings display;
    SoundSettings sound;
    i18n::Language language = i18n::Language::Pt;

    friend bool operator==(const Settings&, const Settings&) = default;
};

Settings defaults();

// Clamps every field to the range the renderer accepts.  Returns true when at
// least one value changed.
bool normalize(Settings& settings);

// Parses `text` on top of the defaults.  Unknown keys are ignored so a newer
// config can still be read by an older build.  Invalid known values keep the
// previous value and append a diagnostic to `warning`; the return value is
// false when any such diagnostic was produced.  `out` is always filled with a
// valid configuration.
bool parse(const std::string& text, Settings& out, std::string* warning = nullptr);

std::string serialize(const Settings& settings);

// Reads a config file.  Returns false only when the file cannot be opened;
// `out` then contains the defaults.  Parse diagnostics go to `warning`.
bool load(const std::string& path, Settings& out, std::string* warning = nullptr);

// Writes atomically through a temporary file.  Parent directories are created
// when needed.  Returns false and fills `error` on failure.
bool save(const std::string& path, const Settings& settings, std::string* error = nullptr);

// Portable location: `buraco.cfg` next to buraco.exe.
std::string defaultPath();

// Per-user fallback used when the portable file is not writable:
// SDL_GetPrefPath("buraco", "buraco")/buraco.cfg.
std::string fallbackPath();

// Loads defaultPath(), or fallbackPath() when the portable file does not
// exist.  `usedPath` receives the file that was read, or the portable path
// that would be written when neither exists.
bool loadDefault(Settings& out, std::string& usedPath, std::string* warning = nullptr);

// Tries `preferred`, then fallbackPath().  `usedPath` receives the file that
// was actually written.
bool saveWithFallback(const std::string& preferred, const Settings& settings,
                      std::string& usedPath, std::string* error = nullptr);

}  // namespace config
