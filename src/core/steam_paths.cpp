#include "steam_paths.h"

#include "platform.h"
#include "vdf.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ss::steam {

namespace {

#if defined(_WIN32)
std::optional<fs::path> steamRootWindows() {
    using platform::Hive;
    struct Cand { Hive hive; const char* subkey; const char* value; };
    const Cand cands[] = {
        {Hive::CurrentUser, "Software\\Valve\\Steam", "SteamPath"},
        {Hive::LocalMachine, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath"},
        {Hive::LocalMachine, "SOFTWARE\\Valve\\Steam", "InstallPath"},
    };
    for (const auto& c : cands) {
        if (auto v = platform::regReadString(c.hive, c.subkey, c.value)) {
            fs::path p(*v);
            if (fs::exists(p)) return p;
        }
    }
    return std::nullopt;
}
#else
std::optional<fs::path> steamRootUnix() {
    fs::path home(platform::homeDir());
    const fs::path cands[] = {
        home / ".steam" / "steam",
        home / ".steam" / "root",
        home / ".local" / "share" / "Steam",
        home / "Library" / "Application Support" / "Steam",  // macOS
    };
    for (const auto& p : cands) {
        std::error_code ec;
        if (fs::exists(p / "steamapps", ec) || fs::exists(p / "config", ec))
            return fs::canonical(p, ec);
    }
    return std::nullopt;
}
#endif

}  // namespace

std::optional<fs::path> steamRoot() {
    if (auto env = platform::getEnv("STEAM_ROOT")) {
        fs::path p(*env);
        if (fs::exists(p)) return p;
    }
#if defined(_WIN32)
    return steamRootWindows();
#else
    return steamRootUnix();
#endif
}

std::optional<fs::path> steamExecutable(const std::optional<fs::path>& rootIn) {
    auto root = rootIn ? rootIn : steamRoot();
    if (!root) return std::nullopt;
#if defined(_WIN32)
    fs::path exe = *root / "steam.exe";
    if (fs::exists(exe)) return exe;
    return std::nullopt;
#else
    fs::path sh = *root / "steam.sh";
    if (fs::exists(sh)) return sh;
    return fs::path("steam");
#endif
}

std::vector<fs::path> libraryFolders(const std::optional<fs::path>& rootIn) {
    auto root = rootIn ? rootIn : steamRoot();
    std::vector<fs::path> libs;
    if (!root) return libs;

    const fs::path candidates[] = {
        *root / "steamapps" / "libraryfolders.vdf",
        *root / "config" / "libraryfolders.vdf",
    };
    for (const auto& cand : candidates) {
        if (!fs::exists(cand)) continue;
        vdf::Value data = vdf::load(cand.string());
        const vdf::Value* folders = data.get("libraryfolders");
        if (!folders) folders = &data;
        for (const auto& kv : folders->map) {
            const vdf::Value& entry = kv.second;
            if (!entry.is_map) continue;
            std::string p = entry.getStr("path");
            if (p.empty()) continue;
            fs::path lp(p);
            std::error_code ec;
            if (fs::exists(lp, ec) &&
                std::find(libs.begin(), libs.end(), lp) == libs.end())
                libs.push_back(lp);
        }
        break;
    }
    // Always include the root install as a library.
    std::error_code ec;
    if (std::find(libs.begin(), libs.end(), *root) == libs.end() &&
        fs::exists(*root / "steamapps", ec))
        libs.insert(libs.begin(), *root);
    return libs;
}

}  // namespace ss::steam
