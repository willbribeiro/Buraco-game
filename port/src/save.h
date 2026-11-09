// Save-file discovery and per-player file names.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace save {

// Current progress file.  --save overrides it for scripted runs.
std::string& path();

// --save/--savedir point discovery at another folder.
std::string& dirOverride();

// The game folder: saves live next to buraco.exe (or the override).
std::filesystem::path dir();

bool looksLikeSave(const std::filesystem::path& file);

// Most recently written progress file in the game folder, if any.
std::optional<std::filesystem::path> findLatestSave();

// Player name stored in a save file (empty when it cannot be read).
std::string savedPlayerName(const std::filesystem::path& file);

// A player name must be usable as a Windows file name.
std::string sanitizeName(const std::string& raw);

// `<name>.sav`, or `<name>2.sav`, `<name>3.sav`, ... when the name is taken.
std::string uniquePathForName(const std::string& rawName);

}  // namespace save
