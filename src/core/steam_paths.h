// Locate the Steam install and its library folders. Port of core/steam_paths.py.
// Cross-platform: Windows reads the registry, POSIX checks the usual dirs; library
// folders come from libraryfolders.vdf (games can spread across drives).

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace ss::steam {

namespace fs = std::filesystem;

// Root Steam install dir, or nullopt if not found. Honors $STEAM_ROOT first
// (used by tests against a synthetic tree).
std::optional<fs::path> steamRoot();

// Path to the Steam executable (to start Steam and run -shutdown).
std::optional<fs::path> steamExecutable(const std::optional<fs::path>& root = std::nullopt);

// All Steam library folders (each holds a steamapps/ dir). The root install is
// always included.
std::vector<fs::path> libraryFolders(const std::optional<fs::path>& root = std::nullopt);

}  // namespace ss::steam
