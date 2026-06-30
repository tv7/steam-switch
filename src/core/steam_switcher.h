// Switch the active Steam account and control the Steam process. Port of
// core/switcher.py. Windows is fully implemented (registry AutoLoginUser +
// loginusers.vdf MostRecent); POSIX does the loginusers/registry.vdf half only
// (the registry write is a follow-up — Next steps #1), matching the Python state.
//
// Hard limit (CLAUDE.md #1): we NEVER type a password. Silent login only works if
// the target account was previously logged in with "Remember me" so Steam holds a
// valid token. canAutologin() / a failed launch is how we detect when it isn't.

#pragma once

#include "steam_paths.h"

#include <functional>
#include <optional>
#include <string>

namespace ss::steam::switcher {

using ShouldCancel = std::function<bool()>;

// The account Steam will auto-login as (AutoLoginUser on Windows; MostRecent on POSIX).
std::optional<std::string> currentAccountName();

// Account id Steam is currently logged in as. 0 = login screen; nullopt if unreadable.
// This is an ONLINE-login signal — an offline cold start does NOT set it (CLAUDE.md).
std::optional<int> activeUserAccountId();

// True only if Steam is right now logged in as this exact account.
bool isLoggedInAs(const std::string& steamid64);

// Pre-flight: can Steam silently log into this account without a password?
// Returns (ok, reason). Needs the account in loginusers.vdf with RememberPassword.
std::pair<bool, std::string> canAutologin(const std::string& accountName);

// Block until Steam reports it's logged in as this account, or timeout / cancel.
bool waitForLogin(const std::string& steamid64, double timeoutSec = 40.0,
                  double pollSec = 1.0, const ShouldCancel& shouldCancel = {});

// Offline flag method (CLAUDE.md #3): set/read WantsOfflineMode in loginusers.vdf.
// `account` matches either the SteamID64 key or the AccountName.
bool setOfflineMode(const std::string& account, bool wantOffline,
                    const std::optional<fs::path>& root = std::nullopt);
bool wantsOfflineMode(const std::optional<std::string>& account = std::nullopt,
                      const std::optional<fs::path>& root = std::nullopt);

// Toggle Steam's account picker (AlwaysShowUserChooser in config/config.vdf).
// Login here needs the picker ON (CLAUDE.md #2). Backs up config.vdf once.
bool setAccountPicker(const fs::path& root, bool show);

// True once Steam's main window is present (offline 'really up' signal).
bool steamWindowPresent();

// Ensure Steam is fully closed: tries `steam -shutdown`, force-kills after a delay.
bool shutdownSteam(double timeoutSec = 30.0, double forceAfterSec = 12.0);

// Start Steam with its window visible (no flags — see switcher.py rationale).
void startSteam();

// Restart Steam at its LOGIN screen so the user can sign into a NEW account
// (empty AutoLoginUser + picker off — the TcNo trick). Windows registry only.
bool restartToAddAccount();

// Make `accountName` the account Steam auto-logs into. Restart Steam to take effect.
void switchAccount(const std::string& accountName);

}  // namespace ss::steam::switcher
