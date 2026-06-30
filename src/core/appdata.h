// Where the app keeps its writable data (mapping.json, cover cache). Mirrors the
// Python `data/` dir next to the code; here it defaults to $SS_DATA_DIR or a
// "data" folder beside the binary, and is overridable for tests.

#pragma once

#include <filesystem>

namespace ss::appdata {

namespace fs = std::filesystem;

// The writable data directory (created on demand).
fs::path dir();

// Override the data directory (tests point this at a temp dir).
void setDir(const fs::path& d);

}  // namespace ss::appdata
