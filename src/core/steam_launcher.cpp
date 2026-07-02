#include "steam_launcher.h"

#include "platform.h"
#include "steam_accounts.h"

#include <chrono>
#include <thread>

namespace ss::steam {

namespace sw = switcher;

namespace {

void launchAppid(int64_t appid) {
    platform::openUri("steam://rungameid/" + std::to_string(appid));
}

void sleepSec(double s) {
    std::this_thread::sleep_for(std::chrono::milliseconds((long long)(s * 1000)));
}

const Account* findAccount(const std::vector<Account>& accts, const std::string& sid) {
    for (const auto& a : accts) if (a.steamid64 == sid) return &a;
    return nullptr;
}

// Undo a failed offline attempt: force Steam down + clear the flag so the next
// start is NORMAL (a leftover WantsOfflineMode=1 re-hangs on a bad config).
[[maybe_unused]] void recoverOffline(const Account& account) {
    sw::shutdownSteam();
    sw::setOfflineMode(account.steamid64, false);
}

// Steam is up + online as `account`: set the offline flag, restart, then launch.
[[maybe_unused]] PlayResult goOfflineAndLaunch(int64_t appid, const Account& account, double loginWait,
                              const Notify& say, const sw::ShouldCancel& cancel) {
    auto cancelled = [&]() { return cancel && cancel(); };
    auto note = [&](const std::string& m) { if (say) say(m); };

    note("Setting Steam to offline mode for '" + account.personaName + "'…");
    if (!sw::setOfflineMode(account.steamid64, true))
        return PlayResult::fail("Couldn't set the offline flag for '" + account.personaName +
                                "' (account not found in loginusers.vdf).", true);
    if (cancelled()) return PlayResult::fail("Launch cancelled.", true);

    note("Restarting Steam in offline mode…");
    if (!sw::shutdownSteam())
        return PlayResult::fail("Couldn't close Steam to restart it offline. Close it manually "
                                "(tray icon -> Exit) and try again.", true);
    if (cancelled()) return PlayResult::fail("Launch cancelled.", true);
    sw::startSteam();
    note("Steam is coming up offline for '" + account.personaName + "'…");

    // Wait for Steam's UI to appear while it stays flagged offline. Offline login
    // does NOT report via ActiveUser, so gate on window-present + flag (CLAUDE.md).
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds((long long)(loginWait * 1000));
    bool windowUp = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (cancelled()) return PlayResult::fail("Launch cancelled.", true);
        if (!sw::wantsOfflineMode(account.steamid64))
            return PlayResult::fail("Steam came back ONLINE for '" + account.personaName +
                                    "' instead of offline. Not launching, to avoid using the "
                                    "shared account online. Try again.", true);
        if (sw::steamWindowPresent()) { windowUp = true; break; }
        sleepSec(1.0);
    }

    if (!windowUp) {
        recoverOffline(account);
        return PlayResult::fail("Steam didn't come up offline for '" + account.personaName +
                                "' (it may have hung on the connecting screen). Cleared the offline "
                                "flag so Steam starts normally next time. This is usually a corrupt "
                                "Steam cache — clear the download cache / delete Steam's appcache "
                                "folder, then try again.", true);
    }

    sleepSec(2.0);
    if (!sw::wantsOfflineMode(account.steamid64))
        return PlayResult::fail("Steam went online for '" + account.personaName + "' during startup. "
                                "Not launching, to avoid using the shared account online. Try again.", true);

    launchAppid(appid);
    return PlayResult::success("Launching '" + account.personaName + "' offline — Steam is offline "
                               "now, so the shared session and its cloud saves aren't touched.", true);
}

PlayResult playOffline(int64_t appid, const Account& account, double loginWait,
                       const Notify& say, const sw::ShouldCancel& cancel) {
#if !defined(_WIN32)
    (void)appid; (void)account; (void)loginWait; (void)say; (void)cancel;
    return PlayResult::fail("Offline mode is Windows-only for now.");
#else
    auto cancelled = [&]() { return cancel && cancel(); };
    auto note = [&](const std::string& m) { if (say) say(m); };

    // Fast path: already logged in as the right account.
    if (sw::isLoggedInAs(account.steamid64)) {
        if (sw::wantsOfflineMode(account.steamid64)) {
            launchAppid(appid);
            return PlayResult::success("Launching '" + account.personaName + "' (already offline).");
        }
        return goOfflineAndLaunch(appid, account, loginWait, say, cancel);
    }

    auto [ok, why] = sw::canAutologin(account.accountName);
    if (!ok)
        return PlayResult::fail("Can't start '" + account.personaName + "': " + why +
                                ". Log into that account once online with \"Remember me\" checked, "
                                "then try again.", false, true);
    if (cancelled()) return PlayResult::fail("Launch cancelled.");

    note("Closing Steam to switch accounts…");
    if (!sw::shutdownSteam())
        return PlayResult::fail("Couldn't close Steam to switch accounts. Close it manually "
                                "(tray icon -> Exit) and try again.");
    if (cancelled()) return PlayResult::fail("Launch cancelled before switching accounts.");

    sw::switchAccount(account.accountName);
    if (cancelled())
        return PlayResult::fail("Launch cancelled. Steam is set to '" + account.personaName +
                                "' but wasn't started.", true);
    sw::startSteam();
    note("Steam is restarting. When the account picker appears, click '" + account.personaName +
         "' (no password needed) to log in.");

    bool loggedIn = sw::waitForLogin(account.steamid64, loginWait, 1.0, cancel);
    if (cancelled()) return PlayResult::fail("Launch cancelled.", true);
    if (!loggedIn)
        return PlayResult::fail("Steam isn't logged in as '" + account.personaName + "' yet — finish "
                                "the picker login (only enter a password if Steam asks, which means "
                                "the saved login expired), then click the game again.", true, true);

    return goOfflineAndLaunch(appid, account, loginWait, say, cancel);
#endif
}

}  // namespace

PlayResult play(int64_t appid, bool offline, double loginWaitSec,
                const Notify& notify, const sw::ShouldCancel& shouldCancel) {
    auto cancelled = [&]() { return shouldCancel && shouldCancel(); };
    auto note = [&](const std::string& m) { if (notify) notify(m); };

    auto targetSid = accountForGame(appid);
    if (!targetSid)
        return PlayResult::fail("No account is mapped to this game yet. Its appmanifest records no "
                                "owner — pin the game to an account manually (or add an API key and "
                                "refresh owned games as a fallback).");

    auto accts = listAccounts();
    const Account* account = findAccount(accts, *targetSid);
    if (!account)
        return PlayResult::fail("Account " + *targetSid +
                                " owns this game but isn't logged in on this PC.");

    if (offline)
        return playOffline(appid, *account, loginWaitSec, notify, shouldCancel);

    // ----- Online launch -----
    if (sw::isLoggedInAs(account->steamid64)) {
        launchAppid(appid);
        return PlayResult::success("Launching on '" + account->personaName + "'.");
    }

    auto [ok, why] = sw::canAutologin(account->accountName);
    if (!ok)
        return PlayResult::fail("Can't auto-login to '" + account->personaName + "': " + why +
                                ". Log into that account in Steam once with \"Remember me\" checked, "
                                "then try again.", false, true);

    if (cancelled()) return PlayResult::fail("Launch cancelled.");

    note("Closing Steam to switch accounts…");
    if (!sw::shutdownSteam())
        return PlayResult::fail("Couldn't close Steam to switch accounts. Close it manually "
                                "(tray icon -> Exit) and try again.");
    if (cancelled()) return PlayResult::fail("Launch cancelled before switching accounts.");

    sw::switchAccount(account->accountName);
    if (cancelled())
        return PlayResult::fail("Launch cancelled. Steam is set to '" + account->personaName +
                                "' but wasn't started.", true);
    sw::startSteam();
    note("Steam is restarting. When the account picker appears, click '" + account->personaName +
         "' (no password needed) to log in.");

    bool loggedIn = sw::waitForLogin(account->steamid64, loginWaitSec, 1.0, shouldCancel);
    if (cancelled()) return PlayResult::fail("Launch cancelled.", true);
    if (!loggedIn) {
        auto active = sw::activeUserAccountId();
        int targetId = (int)accountIdFromSteamId64(std::stoll(account->steamid64));
        if (!active || *active != targetId) {
            if (!active || *active == 0)
                return PlayResult::fail("Steam is still finishing its restart, or it's showing the "
                                        "account picker. Click '" + account->personaName + "' there "
                                        "(no password needed), then click the game again. (Only enter "
                                        "a password if Steam asks — that means the saved login "
                                        "expired.)", true, true);
            return PlayResult::fail("Steam logged into a different account instead of '" +
                                    account->personaName + "'. Switch to it in the Steam window, then "
                                    "click the game again.", true, true);
        }
    }

    launchAppid(appid);
    return PlayResult::success("Launching on '" + account->personaName + "'.", true);
}

PlayResult switchTo(const std::string& steamid64, double loginWaitSec,
                    const Notify& notify, const sw::ShouldCancel& shouldCancel) {
    auto cancelled = [&]() { return shouldCancel && shouldCancel(); };
    auto note = [&](const std::string& m) { if (notify) notify(m); };

    auto accts = listAccounts();
    const Account* account = findAccount(accts, steamid64);
    if (!account)
        return PlayResult::fail("Account " + steamid64 + " isn't logged in on this PC.");

    if (sw::isLoggedInAs(account->steamid64))
        return PlayResult::success("Steam is already on '" + account->personaName + "'.");

    auto [ok, why] = sw::canAutologin(account->accountName);
    if (!ok)
        return PlayResult::fail("Can't auto-login to '" + account->personaName + "': " + why +
                                ". Log into that account in Steam once with \"Remember me\" checked, "
                                "then try again.", false, true);

    if (cancelled()) return PlayResult::fail("Switch cancelled.");

    note("Closing Steam to switch accounts…");
    if (!sw::shutdownSteam())
        return PlayResult::fail("Couldn't close Steam to switch accounts. Close it manually "
                                "(tray icon -> Exit) and try again.");
    if (cancelled()) return PlayResult::fail("Switch cancelled before switching accounts.");

    sw::switchAccount(account->accountName);
    if (cancelled())
        return PlayResult::fail("Switch cancelled. Steam is set to '" + account->personaName +
                                "' but wasn't started.", true);
    sw::startSteam();
    note("Steam is restarting. When the account picker appears, click '" + account->personaName +
         "' (no password needed) to log in.");

    bool loggedIn = sw::waitForLogin(account->steamid64, loginWaitSec, 1.0, shouldCancel);
    if (cancelled()) return PlayResult::fail("Switch cancelled.", true);
    if (!loggedIn)
        return PlayResult::fail("Steam isn't logged in as '" + account->personaName + "' yet — "
                                "finish the picker login (only enter a password if Steam asks, "
                                "which means the saved login expired).", true, true);

    return PlayResult::success("Steam is now on '" + account->personaName + "'.", true);
}

}  // namespace ss::steam
