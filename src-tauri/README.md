# SteamSwitch — Tauri shell

A thin native host (Rust) for the existing `web/` UI. It opens the window
(WebView2 initialises **asynchronously**, so no "Not Responding" freeze), spawns
the Python sidecar (`server.py`), reads the sidecar's `STEAMSWITCH_SIDECAR_READY
<url>` line, injects that URL as `window.__SIDECAR__`, and kills the sidecar on
close. All app logic stays in `core/*` behind the sidecar — there is no business
logic in Rust.

```
src-tauri/src/main.rs   spawn sidecar + open window + inject URL + cleanup
        tauri.conf.json  window/bundle config (frontendDist = ../web)
        capabilities/    Tauri v2 permissions (core only; no plugins)
../server.py             Python sidecar (HTTP + SSE over core/*)
../web/bridge.js         shim: window.pywebview.api.* -> HTTP, SSE -> window.on*
```

## One-time toolchain (build machine only — end users need none)

1. **Rust** — https://rustup.rs (`rustup` installs `cargo`).
2. **Tauri CLI** — `cargo install tauri-cli --version "^2"` (gives `cargo tauri`).
3. **WebView2 runtime** — already on Windows 10/11.
4. **Python + PyInstaller** — for building the sidecar (`pip install pyinstaller`).
5. **App icons** — `build.py` auto-generates `src-tauri/icons/*` from
   `assets/steamswitch.png` on first run. (To do it by hand:
   `cargo tauri icon assets/steamswitch.png`.)

## Develop (fast iteration)

From the repo root:

```
cargo tauri dev
```

In dev the shell runs `python server.py` from the repo root automatically, so you
just need Python + the `core/` deps (stdlib only). Edit `web/*` and reload.

## Build a release

One command from the repo root does all three steps (build the sidecar, build the
Tauri app, and copy the sidecar next to each built executable):

```
python build.py
```

It prints where it placed `server[.exe]`. The portable app is **the SteamSwitch
executable + `server.exe` together** — distribute them in the same folder.

(For a polished single-file installer later, wire `server.exe` in as a Tauri
**externalBin** so it's bundled automatically. Not done yet because declaring
externalBin would require the sidecar binary to exist before `cargo tauri dev` runs,
and dev currently needs only Python. The copy step keeps dev friction-free.)

## Notes

- The sidecar binds `127.0.0.1` on an OS-assigned free port (`--port 0`) and prints
  the real URL; the shell reads it. No fixed port to clash on.
- CSP is disabled (`app.security.csp = null`) because the UI talks to a localhost
  origin; acceptable for a local desktop app.
