// Xbox / Game Pass (Microsoft Store) enumeration + launch. These are UWP/MSIX
// packages: they're license-gated, so — unlike GOG — you can't run the exe
// directly; you launch by AUMID (PackageFamilyName!AppId) through the shell.
//
// We enumerate installed games by scanning the Xbox install roots (default
// <drive>:\XboxGames, override $SS_XBOX_ROOTS) for each game's
//   <root>\<Game>\Content\MicrosoftGame.config
// which carries the package Identity (Name + Publisher) and the app id. The
// PackageFamilyName's publisher hash is computed from the Publisher string
// (SHA-256 + a base32), so we never need WinRT / the PackageManager API.
//
// Config parse + AUMID math are pure/unit-tested; the folder scan is Windows-shaped
// but honours $SS_XBOX_ROOTS so it's testable anywhere.

#pragma once

#include "model.h"

#include <optional>
#include <string>
#include <vector>

namespace ss::xbox {

// Parsed MicrosoftGame.config fields we care about.
struct Config {
    std::string identityName;   // Identity/@Name
    std::string publisher;      // Identity/@Publisher (a full X.500 DN)
    std::string displayName;    // ShellVisuals/@DefaultDisplayName (falls back to Name)
    std::string appId;          // first Executable/@Id — the AUMID application id
    std::string logoPath;       // ShellVisuals logo, relative to the Content dir
                                // (best available of Square480x480Logo/StoreLogo/
                                // Square150x150Logo; "" if none declared)
    std::string storeId;        // <StoreId> element text — keys the game's real
                                // cover art in the MS Store display catalog
};

// ---- pure builders (unit-tested) -------------------------------------------
// The 13-char publisher hash (e.g. Microsoft's is "8wekyb3d8bbwe").
std::string publisherHash(const std::string& publisher);
// PackageFamilyName = "<Name>_<publisherHash>".
std::string packageFamilyName(const std::string& identityName, const std::string& publisher);
// AUMID = "<PackageFamilyName>!<appId>".
std::string aumid(const Config& c);
// Extract the fields above from MicrosoftGame.config XML text (best-effort).
std::optional<Config> parseConfig(const std::string& xml);

// ---- enumeration / launch --------------------------------------------------
// Install roots to scan (\$SS_XBOX_ROOTS split on ';', else the drives' \XboxGames).
std::vector<std::string> installRoots();
// Installed Game Pass games. Each Game has store=Xbox, launchId=AUMID, appid=0.
std::vector<Game> installedGames();
// Launch a game by its AUMID via the shell (explorer shell:AppsFolder\<AUMID>).
PlayResult launch(const std::string& aumid, const std::string& name, const Notify& notify = {});

}  // namespace ss::xbox
