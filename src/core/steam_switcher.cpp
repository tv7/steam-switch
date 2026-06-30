#include "steam_switcher.h"

#include "model.h"
#include "platform.h"
#include "vdf.h"

#include <chrono>
#include <thread>

namespace ss::steam::switcher {

namespace {

constexpr const char* kSteamKey = "Software\\Valve\\Steam";
constexpr const char* kActiveProcKey = "Software\\Valve\\Steam\\ActiveProcess";
const char* kSteamImage =
#if defined(_WIN32)
    "steam.exe";
#else
    "steam";
#endif

void sleepSec(double s) {
    std::this_thread::sleep_for(std::chrono::milliseconds((long long)(s * 1000)));
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Back up a Valve file -> "<file>.bak" once, before our first write.
void backupOnce(const fs::path& f) {
    fs::path bak = f;
    bak += ".bak";
    std::error_code ec;
    if (!fs::exists(bak, ec)) fs::copy_file(f, bak, ec);
}

#if defined(_WIN32)
void setRegistryAutologin(const std::string& accountName) {
    platform::regWriteString(platform::Hive::CurrentUser, kSteamKey, "AutoLoginUser", accountName);
    platform::regWriteDword(platform::Hive::CurrentUser, kSteamKey, "RememberPassword", 1);
}
#endif

void clearAutologinUser() {
    // Empty AutoLoginUser so Steam boots to its sign-in screen (TcNo trick).
    platform::regWriteString(platform::Hive::CurrentUser, kSteamKey, "AutoLoginUser", "");
    platform::regWriteDword(platform::Hive::CurrentUser, kSteamKey, "RememberPassword", 1);
}

// loginusers.vdf MostRecent/AllowAutoLogin update for switch_account.
void updateLoginusers(const std::string& accountName, const fs::path& root) {
    fs::path f = root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return;
    vdf::Value data = vdf::load(f.string());
    vdf::Value* users = data.get("users");
    if (!users) return;
    for (auto& kv : users->map) {
        vdf::Value& info = kv.second;
        if (!info.is_map) continue;
        bool isTarget = lower(info.getStr("AccountName")) == lower(accountName);
        info.setStr("MostRecent", isTarget ? "1" : "0");
        if (isTarget) {
            info.setStr("RememberPassword", "1");
            info.setStr("AllowAutoLogin", "1");
            // Clear any stale offline flag: a leftover "1" makes Steam HANG on this
            // (online) cold start. The offline flow sets it later via setOfflineMode.
            info.setStr("WantsOfflineMode", "0");
            info.setStr("SkipOfflineModeWarning", "0");
        } else {
            // Make sure no OTHER account is also flagged auto-login (several -> Steam
            // shows its picker instead of logging in silently).
            info.setStr("AllowAutoLogin", "0");
        }
    }
    backupOnce(f);
    vdf::dump(data, f.string());
}

// Case-insensitive descent helper for registry.vdf.
const vdf::Value* descend(const vdf::Value* node, std::initializer_list<const char*> path) {
    for (const char* key : path) {
        if (!node) return nullptr;
        node = node->getCI(key);
    }
    return node;
}

}  // namespace

std::optional<std::string> currentAccountName() {
#if defined(_WIN32)
    if (auto v = platform::regReadString(platform::Hive::CurrentUser, kSteamKey, "AutoLoginUser"))
        if (!v->empty()) return v;
    return std::nullopt;
#else
    auto root = steamRoot();
    if (!root) return std::nullopt;
    fs::path f = *root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return std::nullopt;
    vdf::Value data = vdf::load(f.string());
    const vdf::Value* users = data.get("users");
    if (!users) return std::nullopt;
    for (const auto& kv : users->map)
        if (kv.second.is_map && kv.second.getStr("MostRecent") == "1")
            return kv.second.getStr("AccountName");
    return std::nullopt;
#endif
}

std::optional<int> activeUserAccountId() {
#if defined(_WIN32)
    if (auto v = platform::regReadDword(platform::Hive::CurrentUser, kActiveProcKey, "ActiveUser"))
        return (int)*v;
    return std::nullopt;
#else
    fs::path home(platform::homeDir());
    for (const fs::path& cand : {home / ".steam" / "registry.vdf",
                                 home / ".steam" / "steam" / "registry.vdf"}) {
        if (!fs::exists(cand)) continue;
        vdf::Value reg = vdf::load(cand.string());
        const vdf::Value* node = descend(&reg, {"Registry", "HKCU", "Software", "Valve", "Steam", "ActiveProcess"});
        if (!node) node = descend(&reg, {"HKCU", "Software", "Valve", "Steam", "ActiveProcess"});
        if (node) { try { return std::stoi(node->getStr("ActiveUser", "0")); } catch (...) { return std::nullopt; } }
    }
    return std::nullopt;
#endif
}

bool isLoggedInAs(const std::string& steamid64) {
    auto active = activeUserAccountId();
    if (!active || *active == 0) return false;
    return *active == (int)accountIdFromSteamId64(std::stoll(steamid64));
}

std::pair<bool, std::string> canAutologin(const std::string& accountName) {
    auto root = steamRoot();
    if (!root) return {false, "Steam install not found"};
    fs::path f = *root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return {false, "no saved Steam logins on this PC"};
    vdf::Value data = vdf::load(f.string());
    const vdf::Value* users = data.get("users");
    if (users) {
        for (const auto& kv : users->map) {
            const vdf::Value& info = kv.second;
            if (info.is_map && lower(info.getStr("AccountName")) == lower(accountName)) {
                if (info.getStr("RememberPassword") != "1")
                    return {false, "\"Remember me\" is not enabled for this account"};
                return {true, ""};
            }
        }
    }
    return {false, "this account has never been logged in on this PC"};
}

bool waitForLogin(const std::string& steamid64, double timeoutSec, double pollSec,
                  const ShouldCancel& shouldCancel) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((long long)(timeoutSec * 1000));
    while (std::chrono::steady_clock::now() < deadline) {
        if (shouldCancel && shouldCancel()) return false;
        if (isLoggedInAs(steamid64)) return true;
        sleepSec(pollSec);
    }
    return isLoggedInAs(steamid64);
}

bool setOfflineMode(const std::string& account, bool wantOffline, const std::optional<fs::path>& rootIn) {
    auto root = rootIn ? rootIn : steamRoot();
    if (!root) return false;
    fs::path f = *root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return false;
    vdf::Value data = vdf::load(f.string());
    vdf::Value* users = data.get("users");
    if (!users) return false;
    std::string target = lower(account);
    bool wrote = false;
    for (auto& kv : users->map) {
        vdf::Value& info = kv.second;
        if (!info.is_map) continue;
        if (lower(kv.first) == target || lower(info.getStr("AccountName")) == target) {
            info.setStr("WantsOfflineMode", wantOffline ? "1" : "0");
            info.setStr("SkipOfflineModeWarning", wantOffline ? "1" : "0");
            wrote = true;
        }
    }
    if (!wrote) return false;
    backupOnce(f);
    vdf::dump(data, f.string());
    return true;
}

bool wantsOfflineMode(const std::optional<std::string>& account, const std::optional<fs::path>& rootIn) {
    auto root = rootIn ? rootIn : steamRoot();
    if (!root) return false;
    fs::path f = *root / "config" / "loginusers.vdf";
    if (!fs::exists(f)) return false;
    vdf::Value data = vdf::load(f.string());
    const vdf::Value* users = data.get("users");
    if (!users) return false;
    std::optional<std::string> target = account ? std::optional<std::string>(lower(*account)) : std::nullopt;
    for (const auto& kv : users->map) {
        const vdf::Value& info = kv.second;
        if (!info.is_map) continue;
        if (target && lower(kv.first) != *target && lower(info.getStr("AccountName")) != *target)
            continue;
        if (info.getStr("WantsOfflineMode") == "1") return true;
    }
    return false;
}

bool setAccountPicker(const fs::path& root, bool show) {
    std::string desired = show ? "1" : "0";
    fs::path f = root / "config" / "config.vdf";
    if (!fs::exists(f)) return false;
    vdf::Value data = vdf::load(f.string());
    vdf::Value* node = &data;
    for (const char* part : {"InstallConfigStore", "Software", "Valve", "Steam"}) {
        node = node->getCI(part);
        if (!node) return false;   // structure not found -> bail without writing
    }
    if (node->getStr("AlwaysShowUserChooser") == desired) return true;  // already set
    node->setStr("AlwaysShowUserChooser", desired);
    backupOnce(f);
    vdf::dump(data, f.string());
    return true;
}

bool steamWindowPresent() { return platform::steamWindowPresent(); }

bool shutdownSteam(double timeoutSec, double forceAfterSec) {
    if (!platform::processRunning(kSteamImage)) return true;
    if (auto exe = steamExecutable())
        platform::runWait({exe->string(), "-shutdown"});
    auto start = std::chrono::steady_clock::now();
    bool forced = false;
    auto elapsed = [&]() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    };
    while (elapsed() < timeoutSec) {
        if (!platform::processRunning(kSteamImage)) return true;
        if (!forced && elapsed() >= forceAfterSec) { platform::forceKill(kSteamImage); forced = true; }
        sleepSec(1.0);
    }
    if (platform::processRunning(kSteamImage)) { platform::forceKill(kSteamImage); sleepSec(2.0); }
    return !platform::processRunning(kSteamImage);
}

void startSteam() {
    auto exe = steamExecutable();
    if (!exe) return;  // launcher treats a missing window as failure downstream
    platform::spawnDetached({exe->string()});
}

bool restartToAddAccount() {
    auto root = steamRoot();
    if (!root) return false;
    if (!shutdownSteam()) return false;
    clearAutologinUser();                 // empty AutoLoginUser -> sign-in screen
    setAccountPicker(*root, false);       // the login screen, not the saved-account picker
    startSteam();
    return true;
}

void switchAccount(const std::string& accountName) {
    auto root = steamRoot();
    if (!root) return;
#if defined(_WIN32)
    setRegistryAutologin(accountName);
#endif
    updateLoginusers(accountName, *root);
    setAccountPicker(*root, true);        // login needs the picker ON
}

}  // namespace ss::steam::switcher
