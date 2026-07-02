// User settings persisted to settings.json in the app data dir. Port of
// server.py's _load_settings/_save_settings — today it holds the UI language,
// the same key the web app used ("language", default "en").

#pragma once

#include <map>
#include <string>

namespace ss::settings {

// The selected UI language code (e.g. "en", "ar"); "en" if unset/unreadable.
std::string language();

// Persist the selected UI language. Best-effort (swallows I/O errors like Python).
void setLanguage(const std::string& lang);

// Whether the first-run onboarding has been completed (default false = show it).
bool onboarded();

// Persist the onboarding-completed flag. Best-effort.
void setOnboarded(bool value);

// What the CINEMA hero banner shows when the library opens: "last" (last-played
// game) or "random"; "last" if unset/garbage.
std::string heroMode();
void setHeroMode(const std::string& mode);

// "Launch offline by default" (the Settings toggle; default false).
bool offlineDefault();
void setOfflineDefault(bool value);

// ---- ORBIT launch history ----------------------------------------------------
// Steam knows real playtime/lastPlayed (localconfig.vdf); the other stores don't
// expose one, so ORBIT records its own successful launches — a truthful
// "last played (via ORBIT)" for shelf ordering. Keys are store-scoped so they
// never collide across stores: "<storeName>:<launchId>" (e.g. "Epic:Fortnite",
// "Steam:730").

// {gameKey: unix seconds of the last successful launch}; empty if none recorded.
std::map<std::string, long long> launchHistory();

// Unix seconds this game was last launched through ORBIT (0 = never).
long long lastLaunched(const std::string& gameKey);

// Record a successful launch. Best-effort persist.
void recordLaunch(const std::string& gameKey, long long unixSeconds);

}  // namespace ss::settings
