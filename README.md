# SteamSwitch

A native desktop window that lists every game installed on this PC. Click a game
and it switches to the Steam account that owns it, logs in, and launches — no
manual account swapping. Optional "launch offline" toggle.

**No installation needed.** The GUI uses Tkinter, which ships with Python — there
is nothing to `pip install` to run it.

```
python app.py
```

On Windows you can also just **double-click `SteamSwitch.bat`** — it starts the
app with no console window (and tells you what to do if Python isn't installed).

## Portable single-file build (no Python at all)

To get one double-click executable you can drop anywhere:

```
python build.py        # -> dist/SteamSwitch.exe  (run this on Windows)
```

The resulting `.exe` is fully self-contained — no Python, no installer, and
cover-art support (Pillow) is bundled in. PyInstaller can't cross-compile, so
build the Windows `.exe` on a Windows machine.

## Status

- [x] Cross-platform core: Steam discovery, game enumeration, VDF parsing
- [x] Windows account switching (registry `AutoLoginUser` + `loginusers.vdf`)
- [x] Native Tkinter GUI (cover-art grid, account drawer, offline toggle)
- [x] Offline mode via Steam's `WantsOfflineMode` flag (no clicking, no calibration)
- [ ] Linux account switching (next phase)

## How it works

| Step | Where |
|------|-------|
| Find Steam + library folders | `core/steam_paths.py` |
| List installed games (`appmanifest_*.acf`) | `core/games.py` |
| List accounts (`loginusers.vdf`) + map games→accounts via Steam Web API | `core/accounts.py` |
| Switch account (registry + loginusers) and control the Steam process | `core/switcher.py` |
| Orchestrate switch → launch | `core/launcher.py` |
| Native desktop GUI (Tkinter) | `app.py` |
| Portable .exe builder | `build.py` |

## First-time setup

1. Log into **each** Steam account once with **"Remember me"** checked. This is
   mandatory — see the limitation below.
2. Run `python app.py` (or the built `SteamSwitch.exe`). Installed games are
   mapped to their owning account **automatically** from Steam's local data — no
   API key needed.
3. If a game stays **unmapped** (can happen with some family-shared installs),
   open **Accounts**, paste that account's free **Steam Web API key**
   (<https://steamcommunity.com/dev/apikey>) and **Refresh owned** — or pin the
   game to an account manually.
4. Click any game to play.

## Two hard limitations (by Steam's design)

1. **We never type your password.** Steam blocks scripted password entry (2FA).
   Silent auto-login only works because Steam keeps a saved login token after you
   log in once with "Remember me". Tokens expire occasionally; when that happens
   the launcher **detects it** (it reads `ActiveProcess\ActiveUser`, which Steam
   sets to your account on successful login and leaves at `0` on the login screen)
   and pops up "Finish login in Steam" instead of silently failing. Enter your
   password + Steam Guard once in the Steam window, then click the game again. A
   pre-flight check also warns up front if an account was never logged in with
   "Remember me", before it touches your running Steam session.

2. **Offline mode uses Steam's `WantsOfflineMode` flag.** Steam has no working CLI
   flag for offline (it ignores `-offline`), and cold-starting offline with no live
   session just hangs. So the app does what works: it logs the account in **online**
   first (which mints the session offline needs), sets `WantsOfflineMode=1` while
   Steam is running, then restarts Steam so it comes up **offline**, and only then
   launches the game. No clicking, no calibration — resolution-independent. If Steam
   can't come up offline (usually a corrupt Steam cache — clear the download cache /
   delete `Steam\appcache`), the app cleans up and doesn't launch. Windows-only for
   now.

## Troubleshooting

### Offline launch: Steam hangs on the "Connecting…" screen

By far the most common offline problem. When you launch a game **offline**, the app
restarts Steam so it comes up in offline mode — and on some PCs Steam then gets
**stuck on the connecting/loading splash** and never reaches its window. This is a
known, machine-specific Steam issue (almost always a **corrupt Steam cache**), not a
bug in the app — the same steps work fine on a clean PC. The app detects the hang,
cleans up (it clears the offline flag and closes the stuck Steam so your next start
is normal), and does **not** launch the game.

To fix it, clear Steam's caches:

1. **Clear the download cache (try this first):**
   open Steam → **Settings → Downloads → Clear Download Cache** → confirm. Steam
   restarts and you log back in. This alone resolves most cases.
2. **Delete the `appcache` folder (if step 1 didn't help):**
   - Fully exit Steam (right-click the tray icon → **Exit**, and confirm no
     `steam.exe` is left in Task Manager).
   - Open your Steam install folder (default `C:\Program Files (x86)\Steam`).
   - Delete (or rename) the **`appcache`** folder. Steam rebuilds it on next start.
   - Start Steam normally once (online) to let it rebuild, then try the offline
     launch again.

Deleting `appcache` is safe — it's only Steam's cache; your games, logins, and saves
are untouched.

### Other offline notes

- Offline mode needs the account to have logged in **online at least once recently**
  (the app does this for you), so Steam has a valid cached session to go offline with.
- Steam's offline mode has a built-in time limit (you must reconnect every couple of
  weeks); that's Steam's behavior, not the app's.

### "Finish login in Steam" / a game won't switch accounts

The saved login token for that account expired. Log into the account once in the
Steam window with **"Remember me"** checked (enter your password + Steam Guard that
one time), then click the game again.

## Safety

- Before the first write, `loginusers.vdf` is backed up to `loginusers.vdf.bak`.
- All account/key data stays local in `data/mapping.json`.

## Dev

Run modules directly to inspect what the core sees:

```
python -m core.steam_paths     # Steam root, exe, libraries
python -m core.games           # installed games
python tests/smoke_test.py     # parser tests against a synthetic Steam dir
```
