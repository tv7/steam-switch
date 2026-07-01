# SteamSwitch — C++/Qt rewrite (`src/`)

Native C++ multi-store launcher. Replaces the Python `core/` + Tauri shell +
`server.py` sidecar + `web/` UI with a single Qt 6 application. The Steam logic is
a **behaviour-faithful port** of the Python `core/` (same constraints — see the
repo `CLAUDE.md`); the other stores (Epic/GOG/Xbox) are added on top.

## Layout

| Path | Role |
|------|------|
| `core/` | Pure C++ engine, **no Qt** — headless + unit-testable. VDF/JSON parsers, SHA-256 (`sha256.*`), OS platform layer, Steam paths/games/accounts/switcher/launcher/covers, Epic (`epic_games.*`), GOG registry (`gog_games.*`) and Xbox/Game Pass (`xbox_games.*`) enumeration, the `IStore` interface + `SteamStore`/`EpicStore`/`GogStore`/`XboxStore`. |
| `core/platform_{win,posix}.cpp` | OS specifics: registry, process control, `EnumWindows`, `ShellExecute`/`xdg-open`. Windows is the real one; POSIX mirrors the Python stubs. |
| `core/http.{h,cpp}` | Injectable HTTP — the host installs a fetcher so `core/` stays Qt-free (the Qt UI installs a `QNetwork` one; tests inject a stub). |
| `ui/` | Qt 6 + QML. `Backend` (the in-process replacement for `server.py`+`bridge.js`), `GameModel`, `QtFetcher`, and `qml/` views. |
| `tools/ssdiag.cpp` | Headless diagnostic CLI — the Phase 1 parity gate (`ssdiag accounts` ≈ `python -m core.accounts`). |
| `tests/` | Header-only test harness + cases against a synthetic Steam tree (no real Steam). |

## Build

### Full app (Windows — the priority platform)

One-time toolchain: **CMake ≥ 3.21**, a C++17 compiler (MSVC), and **Qt 6.5+**
(Core/Gui/Quick/Network). Install Qt via the Qt online installer or
[`aqtinstall`](https://github.com/miurahr/aqtinstall); point CMake at it with
`-DCMAKE_PREFIX_PATH=<qt>/<ver>/msvc2022_64`.

```
cmake -S src -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2022_64
cmake --build build --config Release
```

If Qt 6 isn't found, CMake still builds **`sscore` + `ssdiag` + `sstests`** (no GUI)
— useful for CI / headless verification.

### Headless core only (Linux/macOS, no Qt)

```
cmake -S src -B build && cmake --build build
ctest --test-dir build              # runs sstests
./build/ssdiag accounts             # diagnostic
```

Or straight with the compiler (what the dev sandbox uses):

```
g++ -std=c++17 -Isrc src/tests/*.cpp \
    src/core/*.cpp src/core/stores/*.cpp src/core/platform_posix.cpp -o /tmp/sstests && /tmp/sstests
```

## Verifying the Steam port (parity gate)

`ssdiag` and the Python tools both honour `$STEAM_ROOT`, so they can be run against
the same tree and diffed:

```
STEAM_ROOT=/path/to/steam ./build/ssdiag accounts > cpp.txt
STEAM_ROOT=/path/to/steam python3 -m core.accounts > py.txt
diff cpp.txt py.txt          # must be empty
```

This must match on the user's real PC before the Qt UI is trusted over the Python
app. (On a synthetic tree, including the family-share playtime tiebreak, the output
is already byte-identical.)

## Status

- **Done + tested headless:** VDF, JSON, Steam paths/games/accounts (with the
  family-share playtime tiebreak), switcher (loginusers/config writes, offline
  flag, backups, `can_autologin`), launcher (online + offline flag method), cover
  resolver, `IStore`/`SteamStore`, `ssdiag`. 29 core tests pass; ssdiag output
  matches the Python diagnostic.
- **Epic store — done + tested headless:** `core/epic_games.*` enumerates installed
  games from the launcher's `*.item` manifests
  (`%PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests`, override with
  `$SS_EPIC_MANIFESTS`), filters DLC/addons, and `stores/epic_store.cpp` launches via
  `com.epicgames.launcher://apps/<AppName>?action=launch&silent=true` (no account
  switching). `Backend` aggregates it alongside Steam; non-Steam games get a stable
  synthetic id so the appid-keyed UI can address them. Verify on real hardware with
  `ssdiag epic`.
- **GOG store — done + tested headless:** `core/gog_games.*` enumerates installed
  games from the Windows registry (`HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<gameID>`,
  32-bit fallback under `SOFTWARE\GOG.com\Games`) — no Galaxy SQLite dependency.
  `stores/gog_store.cpp` launches via GOG Galaxy's
  `GalaxyClient.exe /command=runGame /gameId=<id> /path=<dir>` when Galaxy is
  installed, else runs the DRM-free exe directly in its install dir. Needed two new
  platform primitives: `regSubKeys()` (registry enumeration) and a
  working-directory `spawnDetached()` overload. Registry-only ⇒ empty off-Windows;
  the pure Game/argv builders are unit-tested. Verify with `ssdiag gog`.
- **Xbox / Game Pass store — done + tested headless:** `core/xbox_games.*`
  enumerates installed titles by scanning the Xbox install roots
  (`<drive>:\XboxGames`, override `$SS_XBOX_ROOTS`) for each game's
  `Content\MicrosoftGame.config`, then computes the launch **AUMID**
  (`PackageFamilyName!AppId`) — the PackageFamilyName's publisher hash comes from
  `core/sha256.*` + a base32, so no WinRT/PackageManager dependency. Launch is
  `explorer shell:AppsFolder\<AUMID>` (UWP titles are license-gated, so — unlike GOG
  — the exe can't be run directly). The publisher-hash pipeline is unit-tested
  against Microsoft's well-known `8wekyb3d8bbwe`. Verify with `ssdiag xbox` and
  cross-check AUMIDs against PowerShell `get-StartApps`.
- **Multi-store UI redesign — "ORBIT" (imported from the Claude Design project):**
  the Qt/QML shell was rebuilt around a store-agnostic, dynamic-accent glass design.
  Frameless window with a custom title bar; a glass sidebar (Library / Accounts /
  Settings + a live STORES panel); a **Library** screen (search, refresh, offline
  toggle, store + account filter chips, cover grid); a **Detail** screen (big cover,
  Play now/offline, owning account + truthful stat cards — no fabricated
  ratings/descriptions); an **Accounts** screen of per-store cards; **Settings**
  (offline-default + language/RTL); and a slide-in **Manage** panel (per-store sync
  status + Steam multi-account list + add-account). Each game gets a stable derived
  accent (from its id) driving the ambient backdrop + placeholder gradient; real
  cover art layers on top. Typography is **Space Grotesk (display) + Manrope (body)**,
  instanced to static TTFs and bundled (Cairo covers Arabic). Backend gained a
  `stores` model + a store filter on the proxy. All QML passes `qmllint` (Qt 6.11)
  clean; not yet run on a display here (no Qt/GUI in the sandbox) — verify the
  frameless window (translucency + `startSystemMove`/`startSystemResize`) and the
  `Canvas`-drawn icons on the real Windows build.
- **All four stores done + the multi-store UI is in.** **Next:** `windeployqt`
  portable-zip packaging, then retire the Python/Tauri stack. (Per-store cover art
  is still a follow-up — non-Steam games show the derived gradient placeholder.)
