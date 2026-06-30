// ssdiag — headless diagnostic for the C++ Steam core. The Phase 1 validation
// gate: its output is meant to match the Python diagnostics so we can confirm the
// port resolves paths / games / accounts / mapping identically on a real PC before
// building the Qt UI on top.
//
//   ssdiag paths      ~ python -m core.steam_paths
//   ssdiag games      ~ python -m core.games
//   ssdiag accounts   ~ python -m core.accounts   (mapping + how each game resolved)
//   ssdiag covers <id>  resolve one cover (prints byte count / source)
//
// Build with the core sources + platform_{win,posix}.cpp.

#include "../core/covers.h"
#include "../core/model.h"
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
    else if (cmd == "covers" && argc > 2) cmdCovers(std::stoll(argv[2]));
    else { std::printf("usage: ssdiag [paths|games|accounts|covers <appid>]\n"); return 2; }
    return 0;
}
