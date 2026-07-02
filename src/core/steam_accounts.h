// Steam accounts + game->account mapping. Port of core/accounts.py.
//
// Accounts come from config/loginusers.vdf. Which account owns an installed game
// is resolved from local data first (appmanifest LastOwner, then userdata folders
// with a playtime tiebreak for family shares), with a cached Web-API fallback.

#pragma once

#include "model.h"
#include "steam_paths.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ss::steam {

// Accounts known to Steam (loginusers.vdf).
std::vector<Account> listAccounts(const std::optional<fs::path>& root = std::nullopt);

// {appid: steamid64} from each installed game's LastOwner. Cached; refresh rebuilds.
const std::map<int64_t, std::string>& localOwnerMap(
    const std::optional<fs::path>& root = std::nullopt, bool refresh = false);

// {appid: [steamid64,...]} from userdata/<accountid>/<appid>/ folders (the local
// accounts that actually have + ran the game). Cached; refresh rebuilds.
const std::map<int64_t, std::vector<std::string>>& userdataOwnerMap(
    const std::optional<fs::path>& root = std::nullopt, bool refresh = false);

// How much an account uses an app: (playtime_minutes, last_played_unix, folder_mtime),
// read from userdata/<id>/config/localconfig.vdf. Ties broken by this, playtime first.
struct Usage { long long playtime = 0; long long lastPlayed = 0; double mtime = 0; };
Usage appUsage(int64_t appid, const std::string& steamid64,
               const std::optional<fs::path>& root = std::nullopt);

// Bulk version for decorating the whole library: {appid: Usage} for one account,
// parsed from its localconfig.vdf in a single pass (no per-app folder mtime — that
// stays a tiebreak-only detail of appUsage). Cached per account; refresh rebuilds.
const std::map<int64_t, Usage>& accountUsageMap(
    const std::string& steamid64,
    const std::optional<fs::path>& root = std::nullopt, bool refresh = false);

// Mapping store (data/mapping.json): overrides / api_keys / owned.
void setApiKey(const std::string& steamid64, const std::string& apiKey);
void setOverride(int64_t appid, const std::string& steamid64);

// Owning account for a game, resolved from local data (no API key needed):
//   override -> LastOwner(if local) -> userdata owner (playtime tiebreak) -> owned cache.
// nullopt when nothing maps.
std::optional<std::string> accountForGame(
    int64_t appid, const std::vector<Account>* accounts = nullptr);

// Every account known to own this game (local LastOwner + cached Web API).
std::vector<std::string> allOwners(int64_t appid);

// Steam Web-API fallback (needs a stored key + an installed http fetcher).
std::vector<int64_t> fetchOwnedGames(const std::string& steamid64, const std::string& apiKey);
std::vector<int64_t> refreshOwned(const std::string& steamid64);

// Drop the in-process caches (tests switch between synthetic trees).
void clearCaches();

}  // namespace ss::steam
