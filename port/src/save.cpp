#include "save.h"

#include <SDL.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <system_error>
#include <utility>

namespace save {
namespace fs = std::filesystem;
namespace {

constexpr std::size_t kMaxSaveNameLength = 60;
constexpr int kMaxSaveCopies = 100000;

// SDL_GetBasePath() works before SDL_Init(), which matters because the save
// folder is resolved during the intro screen.
fs::path basePath() {
    char* base = SDL_GetBasePath();
    if (!base) return {};
    fs::path path(base);
    SDL_free(base);
    return path;
}

// Per-user data folder: %APPDATA%\buraco\buraco on Windows,
// ~/.local/share/buraco/buraco on Linux.  SDL creates it when missing.
fs::path prefPath() {
    char* pref = SDL_GetPrefPath("buraco", "buraco");
    if (!pref) return {};
    fs::path path(pref);
    SDL_free(pref);
    return path;
}

fs::path currentDir() {
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    return ec ? fs::path(".") : cwd;
}

// Windows reserves CON, NUL, COM1..COM9 and LPT1..LPT9 even when they carry an
// extension ("CON.txt" still opens the console device).  Trailing spaces and
// dots are ignored by the filesystem, so strip them before comparing.
bool reservedDeviceName(const std::string& upper) {
    std::string stem = upper;
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.'))
        stem.pop_back();

    static const char* kReserved[] = {"CON", "PRN", "AUX", "NUL"};
    for (const char* r : kReserved)
        if (stem == r) return true;
    return stem.size() == 4 &&
           (stem.rfind("COM", 0) == 0 || stem.rfind("LPT", 0) == 0) &&
           stem[3] >= '1' && stem[3] <= '9';
}

}  // namespace

std::string& path() {
    static std::string p = "buraco.sav";
    return p;
}

std::string& dirOverride() {
    static std::string d;
    return d;
}

bool writableDirectory(const fs::path& folder) {
    std::error_code ec;
    if (!fs::is_directory(folder, ec) || ec) return false;

    // A unique probe name keeps two instances starting together from clashing.
    static unsigned long counter = 0;
    const fs::path probe =
        folder / (".buraco-write-" + std::to_string(++counter) + ".tmp");
    {
        std::ofstream file(probe, std::ios::binary | std::ios::trunc);
        if (!file) return false;
    }
    fs::remove(probe, ec);
    return true;
}

fs::path dir() {
    if (!dirOverride().empty()) return fs::path(dirOverride());

    const fs::path base = basePath();
    if (writableDirectory(base)) return base;

    // Installed builds (/usr/bin, /opt, AppImage mount) cannot write next to
    // the executable, so saves move to the per-user data folder.
    const fs::path pref = prefPath();
    if (!pref.empty()) return pref;

    return base.empty() ? currentDir() : base;
}

std::vector<fs::path> searchDirs() {
    std::vector<fs::path> dirs;
    if (!dirOverride().empty()) {
        dirs.push_back(fs::path(dirOverride()));
        return dirs;
    }

    const auto add = [&dirs](const fs::path& candidate) {
        if (candidate.empty()) return;
        for (const fs::path& existing : dirs)
            if (existing == candidate) return;
        dirs.push_back(candidate);
    };

    add(basePath());
    add(prefPath());
    if (dirs.empty()) dirs.push_back(currentDir());
    return dirs;
}

bool looksLikeSave(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    std::string line;
    if (!std::getline(f, line)) return false;
    return line.rfind("BURACO-SAVE", 0) == 0;
}

std::optional<fs::path> findLatestSaveIn(const std::vector<fs::path>& folders) {
    std::optional<fs::path> best;
    fs::file_time_type bestTime{};

    for (const fs::path& folder : folders) {
        std::error_code ec;
        fs::directory_iterator entries(folder, ec);
        if (ec) continue;
        for (const fs::directory_entry& entry : entries) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) continue;
            const fs::path file = entry.path();
            if (file.extension() != ".sav") continue;
            if (!looksLikeSave(file)) continue;
            const fs::file_time_type stamp = fs::last_write_time(file, entryError);
            if (entryError) continue;
            // The folders are ordered, so an exact timestamp tie keeps the
            // higher-priority one.
            if (!best || stamp > bestTime) {
                best = file;
                bestTime = stamp;
            }
        }
    }
    return best;
}

std::optional<fs::path> findLatestSave() { return findLatestSaveIn(searchDirs()); }

std::string savedPlayerName(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.rfind("name 0 ", 0) == 0) return line.substr(7);
    }
    return {};
}

std::string sanitizeName(const std::string& raw) {
    std::string out;
    for (unsigned char c : raw) {
        if (c < 32 || c == 127) continue;
        if (std::strchr("<>:\"/\\|?*", c))
            out += '_';
        else
            out += char(c);
    }
    while (!out.empty() && (out.front() == ' ' || out.front() == '.'))
        out.erase(out.begin());
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    if (out.empty()) out = "JOGADOR";
    if (out.size() > kMaxSaveNameLength) out.resize(kMaxSaveNameLength);

    // Check the part before the first dot so "CON.txt" is renamed too.
    const std::size_t dot = out.find('.');
    const std::size_t stemEnd = dot == std::string::npos ? out.size() : dot;
    std::string upper = out.substr(0, stemEnd);
    for (char& c : upper) c = char(std::toupper(uint8_t(c)));
    if (reservedDeviceName(upper)) {
        out.insert(stemEnd, 1, '_');
        if (out.size() > kMaxSaveNameLength) out.resize(kMaxSaveNameLength);
    }
    return out;
}

std::string uniquePathForName(const std::string& rawName) {
    const std::string base = sanitizeName(rawName);
    const fs::path folder = dir();
    std::error_code ec;
    fs::path candidate = folder / (base + ".sav");
    if (!fs::exists(candidate, ec)) return candidate.string();
    for (int n = 2; n < kMaxSaveCopies; ++n) {
        candidate = folder / (base + std::to_string(n) + ".sav");
        if (!fs::exists(candidate, ec)) return candidate.string();
    }
    return (folder / (base + "_overflow.sav")).string();
}

}  // namespace save
