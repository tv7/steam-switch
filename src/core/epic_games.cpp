#include "epic_games.h"

#include "json.h"
#include "platform.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ss::epic {

namespace {

// Read a whole file to a string, or "" on any error.
std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::string getStr(const json::Value& o, const std::string& key) {
    if (const json::Value* v = o.get(key)) return v->asString();
    return "";
}

// True if the manifest's AppCategories array contains `want` (case-sensitive —
// Epic uses lowercase "games"/"addons"/"applications").
bool hasCategory(const json::Value& o, const std::string& want) {
    const json::Value* cats = o.get("AppCategories");
    if (!cats || !cats->isArray()) return false;
    for (const auto& c : cats->arr)
        if (c.isString() && c.str == want) return true;
    return false;
}

// A manifest is a launchable base game (not DLC/addon) when it's categorised as a
// game, isn't an addon, and — for titles that expose it — is its own main game.
bool isBaseGame(const json::Value& m) {
    const json::Value* cats = m.get("AppCategories");
    if (cats && cats->isArray()) {                 // when categories are present, trust them
        if (hasCategory(m, "addons")) return false;
        if (!hasCategory(m, "games")) return false;
    }
    std::string appName = getStr(m, "AppName");
    std::string mainGame = getStr(m, "MainGameAppName");
    if (!mainGame.empty() && mainGame != appName) return false;  // DLC tied to a base game
    return true;
}

}  // namespace

fs::path manifestsDir() {
    if (auto env = platform::getEnv("SS_EPIC_MANIFESTS"))
        return fs::path(*env);
    std::string base = platform::getEnv("PROGRAMDATA").value_or("C:\\ProgramData");
    return fs::path(base) / "Epic" / "EpicGamesLauncher" / "Data" / "Manifests";
}

std::string launchUri(const std::string& appName) {
    // silent=true suppresses the launcher's own splash; the launcher must be able
    // to start if it isn't already (the OS handles the protocol registration).
    return "com.epicgames.launcher://apps/" + appName + "?action=launch&silent=true";
}

std::vector<Game> installedGames() {
    std::vector<Game> out;
    fs::path dir = manifestsDir();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return out;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".item") continue;

        bool ok = false;
        json::Value m = json::parse(readFile(entry.path()), &ok);
        if (!ok || !m.isObject()) continue;

        std::string appName = getStr(m, "AppName");
        std::string name = getStr(m, "DisplayName");
        std::string install = getStr(m, "InstallLocation");
        if (appName.empty() || name.empty() || install.empty()) continue;
        if (!isBaseGame(m)) continue;

        Game g;
        g.store = Store::Epic;
        g.appid = 0;                       // Epic has no numeric appid; identity is launchId
        g.launchId = appName;
        g.name = name;
        g.installdir = install;
        // bIsIncompleteInstall is true while a download/repair is in flight.
        const json::Value* incomplete = m.get("bIsIncompleteInstall");
        g.fullyInstalled = !(incomplete && incomplete->asBool(false));
        // CatalogItemId keys this game's entry (with its cover art URLs) in the
        // launcher's catalog cache — see store_covers.cpp.
        g.coverHint = getStr(m, "CatalogItemId");
        out.push_back(std::move(g));
    }

    std::sort(out.begin(), out.end(),
              [](const Game& a, const Game& b) { return a.name < b.name; });
    return out;
}

}  // namespace ss::epic
