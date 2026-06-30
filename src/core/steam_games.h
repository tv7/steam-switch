// Enumerate installed Steam games from appmanifest_*.acf. Port of core/games.py.

#pragma once

#include "model.h"
#include "steam_paths.h"

#include <optional>
#include <vector>

namespace ss::steam {

// StateFlags is a bitfield; bit 2 (value 4) = "fully installed".
constexpr int kStateFullyInstalled = 4;

// Every installed game across all library folders, sorted by name (lowercased).
std::vector<Game> installedGames(const std::optional<fs::path>& root = std::nullopt);

}  // namespace ss::steam
