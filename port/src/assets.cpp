#include "assets.h"

#include <SDL.h>

#include <cstdlib>
#include <system_error>

namespace assets {
namespace fs = std::filesystem;
namespace {

fs::path baseDir() {
    char* base = SDL_GetBasePath();
    if (!base) return {};
    fs::path path(base);
    SDL_free(base);
    return path;
}

// SDL_GetBasePath() always ends with a separator, and path::parent_path() of
// "bin/" is "bin" (the trailing empty component counts as the file name).
// Trimming it first makes <prefix>/bin/buraco map to <prefix>/share/....
fs::path directoryOf(const fs::path& path) {
    return path.has_filename() ? path : path.parent_path();
}

}  // namespace

std::vector<fs::path> searchDirs() {
    std::vector<fs::path> dirs;
    const auto add = [&dirs](const fs::path& candidate) {
        if (candidate.empty()) return;
        for (const fs::path& existing : dirs)
            if (existing == candidate) return;
        dirs.push_back(candidate);
    };

    if (const char* override = std::getenv("BURACO_ASSET_DIR");
        override && *override)
        add(fs::path(override));

    const fs::path base = baseDir();
    if (!base.empty()) {
        const fs::path exeDir = directoryOf(base);
        add(exeDir / "assets");
        // <prefix>/bin/buraco -> <prefix>/share/buraco/assets
        add(exeDir.parent_path() / "share" / "buraco" / "assets");
    }

#ifdef BURACO_INSTALL_DATADIR
    add(fs::path(BURACO_INSTALL_DATADIR));
#endif

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) add(cwd / "assets");

    return dirs;
}

fs::path findFont() {
    for (const fs::path& dir : searchDirs()) {
        std::error_code ec;
        const fs::path candidate = dir / kFontFile;
        if (fs::is_regular_file(candidate, ec) && !ec) return candidate;
    }
    return {};
}

}  // namespace assets
