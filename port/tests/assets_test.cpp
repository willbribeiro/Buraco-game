// Asset lookup order (font in the build tree, the FHS data dir or an override).
#include "assets.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace assets;

namespace {

// The game reads BURACO_ASSET_DIR with std::getenv(), so the test has to write
// the environment of this module.  SDL_setenv() is fine on POSIX, but on
// Windows SDL lives in a DLL with its own CRT copy and would set a different
// environment block.
void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    SDL_setenv(name, value.c_str(), 1);
#endif
}

// Points BURACO_ASSET_DIR somewhere else and restores it afterwards.  An empty
// value disables the override, which is what the lookup checks for.
struct AssetDirGuard {
    std::string previous;
    bool had = false;
    const char* name = "BURACO_ASSET_DIR";

    explicit AssetDirGuard(const fs::path& dir) {
        if (const char* value = std::getenv(name)) {
            had = true;
            previous = value;
        }
        setEnv(name, dir.string());
    }

    ~AssetDirGuard() { setEnv(name, had ? previous : std::string()); }
};

fs::path baseDir() {
    char* base = SDL_GetBasePath();
    if (!base) return {};
    const fs::path path(base);
    SDL_free(base);
    // SDL_GetBasePath() ends with a separator; drop the empty last component so
    // parent_path() means the install prefix rather than the bin directory.
    return path.has_filename() ? path : path.parent_path();
}

bool contains(const std::vector<fs::path>& dirs, const fs::path& wanted) {
    for (const fs::path& dir : dirs)
        if (dir == wanted) return true;
    return false;
}

fs::path fontDir(const char* name) {
    const fs::path dir = fs::path(testutil::tempSavePath(name)).parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

}  // namespace

TEST_CASE("asset search starts with BURACO_ASSET_DIR") {
    const fs::path dir = fontDir("assets_override/first");
    AssetDirGuard guard(dir);
    const std::vector<fs::path> dirs = searchDirs();
    REQUIRE(!dirs.empty());
    CHECK_EQ(dirs.front(), dir);
}

TEST_CASE("asset search includes the portable and FHS directories") {
    const fs::path dir = fontDir("assets_override/order");
    AssetDirGuard guard(dir);
    const std::vector<fs::path> dirs = searchDirs();
    const fs::path base = baseDir();
    REQUIRE(!base.empty());
    CHECK(contains(dirs, base / "assets"));
    CHECK(contains(dirs, base.parent_path() / "share" / "buraco" / "assets"));
}

TEST_CASE("asset search has no empty or duplicate entries") {
    const fs::path dir = fontDir("assets_override/unique");
    AssetDirGuard guard(dir);
    const std::vector<fs::path> dirs = searchDirs();
    for (std::size_t i = 0; i < dirs.size(); ++i) {
        CHECK(!dirs[i].empty());
        for (std::size_t j = i + 1; j < dirs.size(); ++j)
            CHECK(dirs[i] != dirs[j]);
    }
}

TEST_CASE("findFont returns the font inside the override directory") {
    const fs::path dir = fontDir("assets_override/font");
    const fs::path font = dir / kFontFile;
    REQUIRE(testutil::writeText(font.string(), std::string(4096, '\0')));
    AssetDirGuard guard(dir);
    CHECK_EQ(findFont(), font);
}

TEST_CASE("findFont falls back to the executable asset folder") {
    const fs::path base = baseDir();
    REQUIRE(!base.empty());
    const fs::path dir = base / "assets";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path font = dir / kFontFile;
    REQUIRE(testutil::writeText(font.string(), std::string(4096, '\0')));

    AssetDirGuard guard{fs::path()};  // disables the environment override
    CHECK_EQ(findFont(), font);

    fs::remove(font, ec);
    fs::remove(dir, ec);
}
