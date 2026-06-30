#include "steam_games.h"

#include "vdf.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace ss::steam {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::optional<Game> parseManifest(const fs::path& path, const fs::path& library) {
    vdf::Value data = vdf::load(path.string());
    const vdf::Value* app = data.get("AppState");
    if (!app) return std::nullopt;
    std::string appidStr = app->getStr("appid");
    if (appidStr.empty()) return std::nullopt;

    int state = 0;
    try { state = std::stoi(app->getStr("StateFlags", "0")); } catch (...) { state = 0; }

    Game g;
    g.store = Store::Steam;
    try { g.appid = std::stoll(appidStr); } catch (...) { return std::nullopt; }
    g.name = app->getStr("name", "App " + appidStr);
    g.installdir = app->getStr("installdir");
    g.library = library.string();
    g.fullyInstalled = (state & kStateFullyInstalled) != 0;
    g.lastOwner = app->getStr("LastOwner", "0");
    return g;
}

}  // namespace

std::vector<Game> installedGames(const std::optional<fs::path>& root) {
    std::map<int64_t, Game> games;  // dedupe by appid, first wins
    for (const auto& lib : libraryFolders(root)) {
        fs::path steamapps = lib / "steamapps";
        std::error_code ec;
        if (!fs::exists(steamapps, ec)) continue;
        for (const auto& entry : fs::directory_iterator(steamapps, ec)) {
            const fs::path& p = entry.path();
            std::string fn = p.filename().string();
            if (fn.rfind("appmanifest_", 0) != 0 || p.extension() != ".acf") continue;
            if (auto g = parseManifest(p, lib)) {
                if (games.find(g->appid) == games.end()) games[g->appid] = *g;
            }
        }
    }
    std::vector<Game> out;
    out.reserve(games.size());
    for (auto& kv : games) out.push_back(kv.second);
    std::sort(out.begin(), out.end(),
              [](const Game& a, const Game& b) { return lower(a.name) < lower(b.name); });
    return out;
}

}  // namespace ss::steam
