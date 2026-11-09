#include "save.h"

#include <SDL.h>

#include <cctype>
#include <cstring>
#include <fstream>

namespace save {
namespace fs = std::filesystem;

std::string& path() {
    static std::string p = "buraco.sav";
    return p;
}

std::string& dirOverride() {
    static std::string d;
    return d;
}

fs::path dir() {
    if (!dirOverride().empty()) return fs::path(dirOverride());
    // SDL_GetBasePath() works before SDL_Init().
    char* base = SDL_GetBasePath();
    if (base) {
        fs::path p(base);
        SDL_free(base);
        return p;
    }
    return fs::current_path();
}

bool looksLikeSave(const fs::path& file) {
    std::ifstream f(file, std::ios::binary);
    std::string line;
    if (!std::getline(f, line)) return false;
    return line.rfind("BURACO-SAVE", 0) == 0;
}

std::optional<fs::path> findLatestSave() {
    std::error_code ec;
    std::optional<fs::path> best;
    fs::file_time_type bestTime{};
    for (const auto& entry : fs::directory_iterator(dir(), ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec) || ec) continue;
        const fs::path& file = entry.path();
        if (file.extension() != ".sav") continue;
        if (!looksLikeSave(file)) continue;
        const fs::file_time_type t = fs::last_write_time(file, ec);
        if (ec) continue;
        if (!best || t > bestTime) {
            best = file;
            bestTime = t;
        }
    }
    return best;
}

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
    if (out.size() > 60) out.resize(60);

    // CON, NUL, COM1... are reserved device names on Windows.
    std::string upper = out;
    for (char& c : upper) c = char(std::toupper(uint8_t(c)));
    static const char* kReserved[] = {"CON", "PRN", "AUX", "NUL"};
    for (const char* r : kReserved)
        if (upper == r) out += '_';
    if (upper.size() == 4 &&
        (upper.rfind("COM", 0) == 0 || upper.rfind("LPT", 0) == 0) &&
        upper[3] >= '1' && upper[3] <= '9')
        out += '_';
    return out;
}

std::string uniquePathForName(const std::string& rawName) {
    const std::string base = sanitizeName(rawName);
    const fs::path folder = dir();
    std::error_code ec;
    fs::path candidate = folder / (base + ".sav");
    if (!fs::exists(candidate, ec)) return candidate.string();
    for (int n = 2; n < 100000; ++n) {
        candidate = folder / (base + std::to_string(n) + ".sav");
        if (!fs::exists(candidate, ec)) return candidate.string();
    }
    return (folder / (base + "_overflow.sav")).string();
}

}  // namespace save
