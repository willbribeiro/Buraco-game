#include "config.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace config {
namespace fs = std::filesystem;
namespace {

constexpr int kMinWidth = 640;
constexpr int kMaxWidth = 7680;
constexpr int kMinHeight = 400;
constexpr int kMaxHeight = 4320;
constexpr int kMinScale = 0;
constexpr int kMaxScale = 8;
constexpr int kMinPosition = -32768;
constexpr int kMaxPosition = 32767;

std::string trim(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseInt(const std::string& text, int& value) {
    if (text.empty()) return false;
    int parsed = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last) return false;
    value = parsed;
    return true;
}

bool parseBool(const std::string& text, bool& value) {
    const std::string v = lower(text);
    if (v == "1" || v == "true" || v == "on" || v == "yes" || v == "sim") {
        value = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "off" || v == "no" || v == "nao") {
        value = false;
        return true;
    }
    return false;
}

void warn(std::string* warning, int line, const std::string& message) {
    if (!warning) return;
    std::ostringstream out;
    out << "line " << line << ": " << message << '\n';
    *warning += out.str();
}

}  // namespace

Settings defaults() { return Settings{}; }

bool normalize(Settings& settings) {
    const Settings before = settings;
    settings.version = 1;
    settings.display.scale =
        std::clamp(settings.display.scale, kMinScale, kMaxScale);
    settings.display.windowWidth =
        std::clamp(settings.display.windowWidth, kMinWidth, kMaxWidth);
    settings.display.windowHeight =
        std::clamp(settings.display.windowHeight, kMinHeight, kMaxHeight);
    settings.display.windowX =
        std::clamp(settings.display.windowX, kMinPosition, kMaxPosition);
    settings.display.windowY =
        std::clamp(settings.display.windowY, kMinPosition, kMaxPosition);
    settings.sound.volume = std::clamp(settings.sound.volume, 0, 100);
    return !(settings == before);
}

bool parse(const std::string& text, Settings& out, std::string* warning) {
    out = defaults();
    bool ok = true;
    bool centeredSeen = false;

    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;

        // Allow trailing comments: "vsync=0  # comment".
        const size_t comment = line.find('#');
        if (comment != std::string::npos) line = trim(line.substr(0, comment));
        if (line.empty()) continue;

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            warn(warning, lineNumber, "expected key=value");
            ok = false;
            continue;
        }

        const std::string key = lower(trim(line.substr(0, equals)));
        const std::string value = trim(line.substr(equals + 1));

        int number = 0;
        bool flag = false;
        if (key == "version") {
            if (!parseInt(value, number)) {
                warn(warning, lineNumber, "invalid version '" + value + "'");
                ok = false;
            } else if (number != 1) {
                warn(warning, lineNumber,
                     "unsupported version " + std::to_string(number) +
                         " (reading known keys)");
                ok = false;
            }
        } else if (key == "fullscreen") {
            if (parseBool(value, flag)) out.display.fullscreen = flag;
            else { warn(warning, lineNumber, "invalid fullscreen '" + value + "'"); ok = false; }
        } else if (key == "scale") {
            if (parseInt(value, number)) out.display.scale = number;
            else { warn(warning, lineNumber, "invalid scale '" + value + "'"); ok = false; }
        } else if (key == "window_width") {
            if (parseInt(value, number)) out.display.windowWidth = number;
            else { warn(warning, lineNumber, "invalid window_width '" + value + "'"); ok = false; }
        } else if (key == "window_height") {
            if (parseInt(value, number)) out.display.windowHeight = number;
            else { warn(warning, lineNumber, "invalid window_height '" + value + "'"); ok = false; }
        } else if (key == "window_centered") {
            if (parseBool(value, flag)) {
                out.display.centered = flag;
                centeredSeen = true;
            } else { warn(warning, lineNumber, "invalid window_centered '" + value + "'"); ok = false; }
        } else if (key == "window_x") {
            if (parseInt(value, number)) {
                out.display.windowX = number;
                if (!centeredSeen) out.display.centered = false;
            } else { warn(warning, lineNumber, "invalid window_x '" + value + "'"); ok = false; }
        } else if (key == "window_y") {
            if (parseInt(value, number)) {
                out.display.windowY = number;
                if (!centeredSeen) out.display.centered = false;
            } else { warn(warning, lineNumber, "invalid window_y '" + value + "'"); ok = false; }
        } else if (key == "maximized") {
            if (parseBool(value, flag)) out.display.maximized = flag;
            else { warn(warning, lineNumber, "invalid maximized '" + value + "'"); ok = false; }
        } else if (key == "integer_scale") {
            if (parseBool(value, flag)) out.display.integerScale = flag;
            else { warn(warning, lineNumber, "invalid integer_scale '" + value + "'"); ok = false; }
        } else if (key == "vsync") {
            if (parseBool(value, flag)) out.display.vsync = flag;
            else { warn(warning, lineNumber, "invalid vsync '" + value + "'"); ok = false; }
        } else if (key == "sound") {
            if (parseBool(value, flag)) out.sound.enabled = flag;
            else { warn(warning, lineNumber, "invalid sound '" + value + "'"); ok = false; }
        } else if (key == "sound_volume") {
            if (parseInt(value, number)) out.sound.volume = number;
            else { warn(warning, lineNumber, "invalid sound_volume '" + value + "'"); ok = false; }
        }
        // Unknown keys are intentionally ignored for forward compatibility.
    }

    normalize(out);
    return ok;
}

std::string serialize(const Settings& settings) {
    std::ostringstream out;
    out << "# Buraco settings\n";
    out << "version=" << settings.version << '\n';
    out << "fullscreen=" << (settings.display.fullscreen ? 1 : 0) << '\n';
    out << "scale=" << settings.display.scale << '\n';
    out << "window_width=" << settings.display.windowWidth << '\n';
    out << "window_height=" << settings.display.windowHeight << '\n';
    out << "window_centered=" << (settings.display.centered ? 1 : 0) << '\n';
    out << "window_x=" << settings.display.windowX << '\n';
    out << "window_y=" << settings.display.windowY << '\n';
    out << "maximized=" << (settings.display.maximized ? 1 : 0) << '\n';
    out << "integer_scale=" << (settings.display.integerScale ? 1 : 0) << '\n';
    out << "vsync=" << (settings.display.vsync ? 1 : 0) << '\n';
    out << "sound=" << (settings.sound.enabled ? 1 : 0) << '\n';
    out << "sound_volume=" << settings.sound.volume << '\n';
    return out.str();
}

bool load(const std::string& path, Settings& out, std::string* warning) {
    out = defaults();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (warning) *warning += "cannot open " + path + '\n';
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    parse(text, out, warning);
    return true;
}

bool save(const std::string& path, const Settings& settings, std::string* error) {
    if (path.empty()) {
        if (error) *error = "empty config path";
        return false;
    }

    Settings normalized = settings;
    normalize(normalized);

    const fs::path target(path);
    const fs::path parent = target.parent_path();
    std::error_code ec;
    if (!parent.empty()) fs::create_directories(parent, ec);
    if (ec) {
        if (error) *error = "cannot create " + parent.string() + ": " + ec.message();
        return false;
    }

    const fs::path temporary = fs::path(target.string() + ".tmp");
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            if (error) *error = "cannot write " + temporary.string();
            return false;
        }
        file << serialize(normalized);
        file.flush();
        if (!file) {
            if (error) *error = "cannot write " + temporary.string();
            return false;
        }
    }

    fs::rename(temporary, target, ec);
    if (ec) {
        // On Windows rename does not always replace an existing file.
        std::error_code ignored;
        fs::remove(target, ignored);
        ec.clear();
        fs::rename(temporary, target, ec);
    }
    if (ec) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        if (error) *error = "cannot replace " + target.string() + ": " + ec.message();
        return false;
    }
    return true;
}

std::string defaultPath() {
    char* base = SDL_GetBasePath();
    fs::path folder;
    if (base) {
        folder = fs::path(base);
        SDL_free(base);
    } else {
        std::error_code ec;
        folder = fs::current_path(ec);
        if (ec) folder = ".";
    }
    return (folder / "buraco.cfg").string();
}

std::string fallbackPath() {
    char* pref = SDL_GetPrefPath("buraco", "buraco");
    if (!pref) return defaultPath();
    const fs::path folder(pref);
    SDL_free(pref);
    return (folder / "buraco.cfg").string();
}

bool loadDefault(Settings& out, std::string& usedPath, std::string* warning) {
    const std::string primary = defaultPath();
    std::error_code ec;

    if (fs::is_regular_file(primary, ec)) {
        usedPath = primary;
        return load(primary, out, warning);
    }

    const std::string fallback = fallbackPath();
    if (!fallback.empty() && fallback != primary && fs::is_regular_file(fallback, ec)) {
        usedPath = fallback;
        return load(fallback, out, warning);
    }

    out = defaults();
    usedPath = primary;
    return false;
}

bool saveWithFallback(const std::string& preferred, const Settings& settings,
                      std::string& usedPath, std::string* error) {
    std::string firstError;
    if (save(preferred, settings, &firstError)) {
        usedPath = preferred;
        return true;
    }

    const std::string fallback = fallbackPath();
    if (fallback.empty() || fallback == preferred) {
        usedPath = preferred;
        if (error) *error = firstError;
        return false;
    }

    std::string secondError;
    if (save(fallback, settings, &secondError)) {
        usedPath = fallback;
        return true;
    }

    usedPath = preferred;
    if (error) *error = firstError + "; fallback: " + secondError;
    return false;
}

}  // namespace config
