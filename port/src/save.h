// Save-file discovery and per-player file names.
//
// Saves live next to the executable when that folder is writable (the portable
// Windows layout and the Linux build tree).  An installed Linux binary usually
// sits in a read-only `/usr/bin`, so the game falls back to the per-user data
// folder returned by SDL_GetPrefPath().
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace save {

// Current progress file.  --save overrides it for scripted runs.
std::string& path();

// --save/--savedir point discovery at another folder.
std::string& dirOverride();

// Folder new saves are written to: the override, else the executable folder
// when it is writable, else the per-user data folder.
std::filesystem::path dir();

// Every folder the game reads saves from, in priority order.  A --savedir
// override narrows this to that single folder.
std::vector<std::filesystem::path> searchDirs();

// True when a probe file can be created in `folder` and removed again.
bool writableDirectory(const std::filesystem::path& folder);

bool looksLikeSave(const std::filesystem::path& file);

// Most recently written progress file among `folders`.
std::optional<std::filesystem::path> findLatestSaveIn(
    const std::vector<std::filesystem::path>& folders);

// Most recently written progress file in searchDirs(), if any.
std::optional<std::filesystem::path> findLatestSave();

// Player name stored in a save file (empty when it cannot be read).
std::string savedPlayerName(const std::filesystem::path& file);

// A player name must be usable as a file name on Windows and Linux.
std::string sanitizeName(const std::string& raw);

// `<name>.sav`, or `<name>2.sav`, `<name>3.sav`, ... when the name is taken.
std::string uniquePathForName(const std::string& rawName);

}  // namespace save
