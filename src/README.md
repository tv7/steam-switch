# SteamSwitch — C++/Qt rewrite (`src/`)

Native C++ multi-store launcher. Replaces the Python `core/` + Tauri shell +
`server.py` sidecar + `web/` UI with a single Qt 6 application. The Steam logic is
a **behaviour-faithful port** of the Python `core/` (same constraints — see the
repo `CLAUDE.md`); the other stores (Epic/GOG/Xbox) are added on top.

## Layout

| Path | Role |
|------|------|
| `core/` | Pure C++ engine, **no Qt** — headless + unit-testable. VDF/JSON parsers, OS platform layer, Steam paths/games/accounts/switcher/launcher/covers, the `IStore` interface + `SteamStore`. |
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
  resolver, `IStore`/`SteamStore`, `ssdiag`. 24 core tests pass; ssdiag output
  matches the Python diagnostic.
- **Written, builds on Windows w/ Qt (not yet run here — no Qt/display in sandbox):**
  the Qt/QML UI (grid, covers, search/filter, accounts hub, launch/cancel, offline,
  RTL scaffolding).
- **Next:** Epic / GOG / Xbox stores (implement `IStore`), then `windeployqt`
  portable-zip packaging, then retire the Python/Tauri stack.
