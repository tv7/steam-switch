#include "steam_accounts.h"

#include "appdata.h"
#include "http.h"
#include "json.h"
#include "steam_games.h"
#include "vdf.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>

namespace ss::steam {

namespace {

std::optional<std::map<int64_t, std::string>> g_localOwner;
std::optional<std::map<int64_t, std::vector<std::string>>> g_userdata;
std::map<std::string, std::map<int64_t, Usage>> g_usage;   // steamid64 -> per-app usage

bool isDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

fs::path mappingFile() { return appdata::dir() / "mapping.json"; }

json::Value loadMapping() {
    std::error_code ec;
    if (fs::exists(mappingFile(), ec)) {
        std::ifstream f(mappingFile(), std::ios::binary);
        if (f) {
            std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            bool ok = false;
            json::Value v = json::parse(text, &ok);
            if (ok && v.isObject()) {
                if (!v.get("api_keys")) v.set("api_keys", json::Value::makeObject());
                if (!v.get("overrides")) v.set("overrides", json::Value::makeObject());
                if (!v.get("owned")) v.set("owned", json::Value::makeObject());
                return v;
            }
        }
    }
    json::Value v = json::Value::makeObject();
    v.set("api_keys", json::Value::makeObject());
    v.set("overrides", json::Value::makeObject());
    v.set("owned", json::Value::makeObject());
    return v;
}

void saveMapping(const json::Value& m) {
    std::ofstream f(mappingFile(), std::ios::binary | std::ios::trunc);
    if (f) f << json::dump(m, 2) << "\n";
}

}  // namespace

void clearCaches() { g_localOwner.reset(); g_userdata.reset(); g_usage.clear(); }

std::vector<Account> listAccounts(const std::optional<fs::path>& rootIn) {
    auto root = rootIn ? rootIn : steamRoot();
    std::vector<Account> out;
    if (!root) return out;
    fs::path f = *root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return out;
    vdf::Value data = vdf::load(f.string());
    const vdf::Value* users = data.get("users");
    if (!users) return out;
    for (const auto& kv : users->map) {
        const vdf::Value& info = kv.second;
        if (!info.is_map) continue;
        Account a;
        a.steamid64 = kv.first;
        a.accountName = info.getStr("AccountName");
        a.personaName = info.getStr("PersonaName", info.getStr("AccountName"));
        a.mostRecent = info.getStr("MostRecent", "0") == "1";
        a.rememberPassword = info.getStr("RememberPassword", "0") == "1";
        out.push_back(std::move(a));
    }
    return out;
}

const std::map<int64_t, std::string>& localOwnerMap(const std::optional<fs::path>& root, bool refresh) {
    if (g_localOwner && !refresh) return *g_localOwner;
    std::map<int64_t, std::string> out;
    for (const auto& g : installedGames(root))
        if (!g.lastOwner.empty() && g.lastOwner != "0") out[g.appid] = g.lastOwner;
    g_localOwner = std::move(out);
    return *g_localOwner;
}

const std::map<int64_t, std::vector<std::string>>& userdataOwnerMap(
    const std::optional<fs::path>& rootIn, bool refresh) {
    if (g_userdata && !refresh) return *g_userdata;
    std::map<int64_t, std::vector<std::string>> out;
    auto root = rootIn ? rootIn : steamRoot();
    if (root) {
        fs::path udroot = *root / "userdata";
        std::error_code ec;
        if (fs::is_directory(udroot, ec)) {
            for (const auto& accEntry : fs::directory_iterator(udroot, ec)) {
                if (!accEntry.is_directory()) continue;
                std::string accName = accEntry.path().filename().string();
                if (!isDigits(accName)) continue;
                std::string steamid64 = std::to_string(std::stoll(accName) + kSteamId64Base);
                for (const auto& appEntry : fs::directory_iterator(accEntry.path(), ec)) {
                    if (!appEntry.is_directory()) continue;
                    std::string appName = appEntry.path().filename().string();
                    if (!isDigits(appName)) continue;
                    out[std::stoll(appName)].push_back(steamid64);
                }
            }
        }
    }
    g_userdata = std::move(out);
    return *g_userdata;
}

Usage appUsage(int64_t appid, const std::string& steamid64, const std::optional<fs::path>& rootIn) {
    Usage u;
    auto root = rootIn ? rootIn : steamRoot();
    if (!root) return u;
    long long accountid32 = std::stoll(steamid64) - kSteamId64Base;
    fs::path cfg = *root / "userdata" / std::to_string(accountid32) / "config" / "localconfig.vdf";
    std::error_code ec;
    if (fs::exists(cfg, ec)) {
        vdf::Value data = vdf::load(cfg.string());
        const vdf::Value* node = &data;
        const char* path[] = {"UserLocalConfigStore", "Software", "Valve", "Steam", "apps"};
        for (const char* key : path) {
            node = node->getCI(key);
            if (!node) break;
        }
        if (node) node = node->getCI(std::to_string(appid));
        if (node && node->is_map) {
            if (const vdf::Value* pt = node->getCI("Playtime"))
                { try { u.playtime = std::stoll(pt->str); } catch (...) { u.playtime = 0; } }
            if (const vdf::Value* lp = node->getCI("LastPlayed"))
                { try { u.lastPlayed = std::stoll(lp->str); } catch (...) { u.lastPlayed = 0; } }
        }
    }
    fs::path folder = *root / "userdata" / std::to_string(accountid32) / std::to_string(appid);
    if (fs::exists(folder, ec)) {
        auto t = fs::last_write_time(folder, ec);
        if (!ec) u.mtime = (double)t.time_since_epoch().count();
    }
    return u;
}

const std::map<int64_t, Usage>& accountUsageMap(const std::string& steamid64,
                                                const std::optional<fs::path>& rootIn,
                                                bool refresh) {
    auto it = g_usage.find(steamid64);
    if (it != g_usage.end() && !refresh) return it->second;
    std::map<int64_t, Usage>& out = g_usage[steamid64];
    out.clear();
    auto root = rootIn ? rootIn : steamRoot();
    if (!root) return out;
    long long accountid32 = 0;
    try { accountid32 = std::stoll(steamid64) - kSteamId64Base; } catch (...) { return out; }
    fs::path cfg = *root / "userdata" / std::to_string(accountid32) / "config" / "localconfig.vdf";
    std::error_code ec;
    if (!fs::exists(cfg, ec)) return out;
    vdf::Value data = vdf::load(cfg.string());
    const vdf::Value* node = &data;
    const char* path[] = {"UserLocalConfigStore", "Software", "Valve", "Steam", "apps"};
    for (const char* key : path) {
        node = node->getCI(key);
        if (!node) return out;
    }
    for (const auto& kv : node->map) {
        if (!isDigits(kv.first) || !kv.second.is_map) continue;
        Usage u;
        if (const vdf::Value* pt = kv.second.getCI("Playtime"))
            { try { u.playtime = std::stoll(pt->str); } catch (...) { u.playtime = 0; } }
        if (const vdf::Value* lp = kv.second.getCI("LastPlayed"))
            { try { u.lastPlayed = std::stoll(lp->str); } catch (...) { u.lastPlayed = 0; } }
        if (u.playtime || u.lastPlayed) out[std::stoll(kv.first)] = u;
    }
    return out;
}

void setApiKey(const std::string& steamid64, const std::string& apiKey) {
    json::Value m = loadMapping();
    m.get("api_keys")->set(steamid64, json::Value::makeString(apiKey));
    saveMapping(m);
}

void setOverride(int64_t appid, const std::string& steamid64) {
    json::Value m = loadMapping();
    m.get("overrides")->set(std::to_string(appid), json::Value::makeString(steamid64));
    saveMapping(m);
}

std::optional<std::string> accountForGame(int64_t appid, const std::vector<Account>* accountsIn) {
    json::Value m = loadMapping();
    if (const json::Value* ov = m.get("overrides")->get(std::to_string(appid)))
        if (ov->isString() && !ov->str.empty()) return ov->str;

    std::vector<Account> local = accountsIn ? *accountsIn : listAccounts();
    std::vector<std::string> localIds;
    for (const auto& a : local) localIds.push_back(a.steamid64);
    auto isLocal = [&](const std::string& sid) {
        return std::find(localIds.begin(), localIds.end(), sid) != localIds.end();
    };

    const auto owners = localOwnerMap();   // copy: returned ref is into a static cache
    auto it = owners.find(appid);
    if (it != owners.end() && isLocal(it->second)) return it->second;

    // LastOwner missing or not local (family share): use userdata.
    std::vector<std::string> players;
    const auto ud = userdataOwnerMap();
    auto uit = ud.find(appid);
    if (uit != ud.end())
        for (const auto& sid : uit->second) if (isLocal(sid)) players.push_back(sid);

    if (players.size() == 1) return players[0];
    if (players.size() > 1) {
        // Several local accounts ran it (e.g. both borrowed the family share): pick
        // the one that actually plays it — most playtime, then last-played, then mtime.
        std::sort(players.begin(), players.end(), [&](const std::string& a, const std::string& b) {
            Usage ua = appUsage(appid, a), ub = appUsage(appid, b);
            if (ua.playtime != ub.playtime) return ua.playtime > ub.playtime;
            if (ua.lastPlayed != ub.lastPlayed) return ua.lastPlayed > ub.lastPlayed;
            return ua.mtime > ub.mtime;
        });
        return players[0];
    }

    if (const json::Value* owned = m.get("owned")) {
        for (const auto& kv : owned->obj) {
            if (!kv.second.isArray()) continue;
            for (const auto& el : kv.second.arr) {
                long long a = 0;
                try { a = std::stoll(el.str.empty() ? std::to_string((long long)el.num) : el.str); } catch (...) {}
                if (a == appid) return kv.first;
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> allOwners(int64_t appid) {
    std::vector<std::string> owners;
    json::Value m = loadMapping();
    if (const json::Value* owned = m.get("owned")) {
        for (const auto& kv : owned->obj) {
            if (!kv.second.isArray()) continue;
            for (const auto& el : kv.second.arr) {
                long long a = 0;
                try { a = std::stoll(el.str.empty() ? std::to_string((long long)el.num) : el.str); } catch (...) {}
                if (a == appid) { owners.push_back(kv.first); break; }
            }
        }
    }
    const auto lo = localOwnerMap();
    auto it = lo.find(appid);
    if (it != lo.end() && std::find(owners.begin(), owners.end(), it->second) == owners.end())
        owners.insert(owners.begin(), it->second);
    return owners;
}

std::vector<int64_t> fetchOwnedGames(const std::string& steamid64, const std::string& apiKey) {
    std::string url =
        "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/?key=" + apiKey +
        "&steamid=" + steamid64 + "&include_played_free_games=1&format=json";
    auto raw = http::get(url);
    std::vector<int64_t> out;
    if (!raw) return out;
    bool ok = false;
    json::Value v = json::parse(*raw, &ok);
    if (!ok) return out;
    const json::Value* resp = v.get("response");
    if (!resp) return out;
    const json::Value* games = resp->get("games");
    if (!games || !games->isArray()) return out;
    for (const auto& g : games->arr)
        if (const json::Value* a = g.get("appid"))
            out.push_back((int64_t)a->num);
    return out;
}

std::vector<int64_t> refreshOwned(const std::string& steamid64) {
    json::Value m = loadMapping();
    const json::Value* key = m.get("api_keys")->get(steamid64);
    if (!key || !key->isString() || key->str.empty()) return {};
    std::vector<int64_t> appids = fetchOwnedGames(steamid64, key->str);
    json::Value arr = json::Value::makeArray();
    for (int64_t a : appids) arr.arr.push_back(json::Value::makeNumber((double)a));
    m.get("owned")->set(steamid64, std::move(arr));
    saveMapping(m);
    return appids;
}

}  // namespace ss::steam
