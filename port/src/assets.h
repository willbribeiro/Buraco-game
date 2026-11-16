// Locates the data files shipped with the game (the extracted VGA font).
//
// The build tree keeps `assets/` next to the executable, while `cmake
// --install` uses the FHS data directory.  Both layouts, a tarball and a debug
// override are supported here so the renderer never guesses paths itself.
#pragma once

#include <filesystem>
#include <vector>

namespace assets {

// File name of the VGA font inside the asset directory.
inline constexpr const char* kFontFile = "font8x16.bin";

// Directories searched for the font, in priority order:
//   1. $BURACO_ASSET_DIR                     (debug / one-off override)
//   2. <executable>/assets                   (build tree, tarball, AppImage)
//   3. <executable>/../share/buraco/assets   (installed FHS layout)
//   4. BURACO_INSTALL_DATADIR                (configure-time install prefix)
//   5. ./assets                              (running from the repository root)
std::vector<std::filesystem::path> searchDirs();

// First existing font file, or an empty path when no candidate has one.
std::filesystem::path findFont();

}  // namespace assets
