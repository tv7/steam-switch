// ssdiag — headless diagnostic for the C++ Steam core. The Phase 1 validation
// gate: its output is meant to match the Python diagnostics so we can confirm the
// port resolves paths / games / accounts / mapping identically on a real PC before
// building the Qt UI on top.
//
//   ssdiag paths      ~ python -m core.steam_paths
//   ssdiag games      ~ python -m core.games
//   ssdiag accounts   ~ python -m core.accounts   (mapping + how each game resolved)
//   ssdiag epic         list installed Epic games + their launch URIs
//   ssdiag gog          list installed GOG games + their launch commands
//   ssdiag xbox         list installed Xbox/Game Pass games + their AUMIDs
//   ssdiag covers <id>  resolve one cover (prints byte count / source)
//
// Build with the core sources + platform_{win,posix}.cpp.

#include "../core/covers.h"
#include "../core/epic_games.h"
#include "../core/gog_games.h"
#include "../core/model.h"
#include "../core/xbox_games.h"
#include "../core/steam_accounts.h"
#include "../core/steam_games.h"
#include "../core/steam_paths.h"
#include "../core/steam_switcher.h"

#include <cstdio>
#include <map>
#include <string>

using namespace ss;

static void cmdPaths() {
    auto root = steam::steamRoot();
    std::printf("Steam root: %s\n", root ? root->string().c_str() : "(not found)");
    auto exe = steam::steamExecutable(root);
    std::printf("Executable: %s\n", exe ? exe->string().c_str() : "(not found)");
    std::printf("Libraries:\n");
    for (const auto& lib : steam::libraryFolders(root))
        std::printf("  - %s\n", lib.string().c_str());
}

static void cmdGames() {
    for (const auto& g : steam::installedGames()) {
        std::printf("%8lld  %s%s  owner=%s\n", (long long)g.appid, g.name.c_str(),
                    g.fullyInstalled ? "" : "  (partial)",
                    (g.lastOwner.empty() || g.lastOwner == "0") ? "0" : g.lastOwner.c_str());
    }
}

static void cmdAccounts() {
    auto accts = steam::listAccounts();
    std::map<std::string, Account> byId;
    for (const auto& a : accts) byId[a.steamid64] = a;

    auto label = [&](const std::string& sid) -> std::string {
        auto it = byId.find(sid);
        return it != byId.end() ? it->second.accountName : sid;
    };

    std::printf("Known accounts (loginusers.vdf):\n");
    for (const auto& a : accts)
        std::printf("  %s  %s (%s)\n", a.steamid64.c_str(), a.accountName.c_str(), a.personaName.c_str());
    std::printf("\n%8s  %17s  game / resolution\n", "appid", "LastOwner");

    for (const auto& g : steam::installedGames()) {
        std::string owner = (g.lastOwner.empty() ? "0" : g.lastOwner);
        auto resolved = steam::accountForGame(g.appid, &accts);
        std::string how;
        if (resolved && byId.count(*resolved)) {
            if (byId.count(owner))
                how = "-> " + label(*resolved);
            else
                how = "-> " + label(*resolved) + " (via userdata; LastOwner " + owner +
                      " is a family-share lender)";
        } else if (owner == "0") {
            how = "UNMAPPED — no LastOwner and no local account has run it";
        } else {
            how = "UNMAPPED — owner not on this PC and no local account has run it (id " + owner + ")";
        }
        std::printf("%8lld  %17s  %s  [%s]\n", (long long)g.appid, owner.c_str(),
                    g.name.c_str(), how.c_str());
    }
}

static void cmdEpic() {
    std::printf("Epic manifests: %s\n\n", epic::manifestsDir().string().c_str());
    auto games = epic::installedGames();
    if (games.empty()) { std::printf("(no installed Epic games found)\n"); return; }
    for (const auto& g : games)
        std::printf("%-40s%s\n    launch: %s\n", g.name.c_str(),
                    g.fullyInstalled ? "" : "  (partial)", epic::launchUri(g.launchId).c_str());
}

static void cmdGog() {
    auto galaxy = gog::galaxyClientExe();
    std::printf("GOG Galaxy: %s\n\n", galaxy ? galaxy->c_str() : "(not installed — will launch exes directly)");
    auto games = gog::installedGames();
    if (games.empty()) { std::printf("(no installed GOG games found)\n"); return; }
    for (const auto& g : games) {
        std::printf("%-40s  id=%s\n    dir: %s\n", g.name.c_str(), g.launchId.c_str(),
                    g.installdir.c_str());
        if (auto e = gog::entry(g.launchId)) {
            auto argv = galaxy ? gog::galaxyRunGameArgv(*galaxy, *e) : gog::directLaunchArgv(*e);
            std::string cmd;
            for (const auto& a : argv) { if (!cmd.empty()) cmd += ' '; cmd += a; }
            std::printf("    launch: %s\n", cmd.c_str());
        }
    }
}

static void cmdXbox() {
    std::printf("Xbox install roots:\n");
    auto roots = xbox::installRoots();
    if (roots.empty()) std::printf("  (none — set $SS_XBOX_ROOTS or install a Game Pass game)\n");
    for (const auto& r : roots) std::printf("  - %s\n", r.c_str());
    std::printf("\n");
    auto games = xbox::installedGames();
    if (games.empty()) { std::printf("(no installed Xbox games found)\n"); return; }
    for (const auto& g : games)
        std::printf("%-40s\n    AUMID: %s\n    dir:   %s\n",
                    g.name.c_str(), g.launchId.c_str(), g.installdir.c_str());
    std::printf("\nTip: cross-check an AUMID against PowerShell `get-StartApps`.\n");
}

static void cmdCovers(int64_t appid) {
    auto bytes = covers::coverBytes(appid);
    if (bytes) std::printf("cover %lld: %zu bytes\n", (long long)appid, bytes->size());
    else std::printf("cover %lld: none\n", (long long)appid);
}

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "accounts";
    if (cmd == "paths") cmdPaths();
    else if (cmd == "games") cmdGames();
    else if (cmd == "accounts") cmdAccounts();
    else if (cmd == "epic") cmdEpic();
    else if (cmd == "gog") cmdGog();
    else if (cmd == "xbox") cmdXbox();
    else if (cmd == "covers" && argc > 2) cmdCovers(std::stoll(argv[2]));
    else { std::printf("usage: ssdiag [paths|games|accounts|epic|gog|xbox|covers <appid>]\n"); return 2; }
    return 0;
}
