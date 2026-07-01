// Shared value types for the launcher core. No Qt — the UI maps these onto Qt
// models. Ports the dataclasses from core/games.py, core/accounts.py and
// core/launcher.py, and adds a Store enum for the multi-store library.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ss {

// Progress callback used by launch flows (optional everywhere).
using Notify = std::function<void(const std::string&)>;

// Which storefront a game/account belongs to. Steam is the only one with account
// switching; the others just enumerate + launch.
enum class Store { Steam, Epic, Gog, Xbox };

const char* storeName(Store s);

// Port of games.py::Game. `lastOwner` is the SteamID64 that owns the license
// (the local player for a normal game; the lender for a family share).
struct Game {
    Store store = Store::Steam;
    int64_t appid = 0;            // Steam appid; for other stores see launchId
    std::string launchId;         // store-specific launch token (Epic AppName, GOG id, Xbox AUMID)
    std::string name;
    std::string installdir;
    std::string library;          // library folder this game lives in
    bool fullyInstalled = false;
    std::string lastOwner;        // SteamID64 (Steam only)
    std::string coverHint;        // store-specific art hint: Epic = CatalogItemId,
                                  // Xbox = absolute path of the local logo PNG
                                  // (GOG needs none — launchId is the product id)
};

// Port of accounts.py::Account (Steam logins from loginusers.vdf).
struct Account {
    std::string steamid64;
    std::string accountName;      // login name (what AutoLoginUser expects)
    std::string personaName;      // display name
    bool mostRecent = false;
    bool rememberPassword = false;
};

// Port of launcher.py::PlayResult.
struct PlayResult {
    bool ok = false;
    std::string message;
    bool switched = false;
    bool needsLogin = false;      // user must finish login + Steam Guard manually

    static PlayResult fail(std::string msg, bool switched = false, bool needsLogin = false) {
        return PlayResult{false, std::move(msg), switched, needsLogin};
    }
    static PlayResult success(std::string msg, bool switched = false) {
        return PlayResult{true, std::move(msg), switched, false};
    }
};

// SteamID64 <-> 32-bit account id (used by ActiveUser / userdata folders).
constexpr int64_t kSteamId64Base = 76561197960265728LL;
inline int64_t accountIdFromSteamId64(int64_t steamid64) { return steamid64 - kSteamId64Base; }

}  // namespace ss
