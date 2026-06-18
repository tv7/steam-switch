"""Build the SteamSwitch release — the Python sidecar + the Tauri app.

Steps:
  1. python build_sidecar.py   -> dist/server[.exe]            (PyInstaller)
  2. cargo tauri build         -> src-tauri/target/release/...  (Tauri/Rust)
  3. copy the sidecar next to each built SteamSwitch executable, so the release
     finds it at runtime (main.rs looks for `server[.exe]` beside the app exe).

Prereqs (one-time, see src-tauri/README.md):
  * Rust + the Tauri CLI:  cargo install tauri-cli --version "^2"
  * Python + PyInstaller:  pip install pyinstaller
  * App icons:             cargo tauri icon assets/steamswitch.png

PyInstaller and Tauri both can't cross-compile — run this on the target OS.
(The old pywebview build lived here; the app is now Tauri + a Python sidecar.)
"""

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXE = ".exe" if sys.platform.startswith("win") else ""


def run(cmd, **kw):
    print(">>", " ".join(str(c) for c in cmd))
    subprocess.check_call(cmd, **kw)


def main():
    # 1. sidecar -------------------------------------------------------------
    run([sys.executable, str(ROOT / "build_sidecar.py")])
    sidecar = ROOT / "dist" / ("server" + EXE)
    if not sidecar.exists():
        sys.exit(f"Sidecar not found at {sidecar} — build_sidecar.py failed?")

    # 2. tauri ---------------------------------------------------------------
    cargo = shutil.which("cargo")
    if not cargo:
        sys.exit("`cargo` not found. Install Rust (https://rustup.rs) and the Tauri CLI:\n"
                 '    cargo install tauri-cli --version "^2"\n'
                 "See src-tauri/README.md.")

    # App icons: cargo tauri build fails if src-tauri/icons/ isn't populated. Generate
    # them from the source PNG on first build (idempotent — skipped once present).
    icons_dir = ROOT / "src-tauri" / "icons"
    needed = ["32x32.png", "128x128.png", "128x128@2x.png", "icon.icns", "icon.ico"]
    if not all((icons_dir / n).exists() for n in needed):
        src_png = ROOT / "assets" / "steamswitch.png"
        print("Icons missing — generating from", src_png)
        run([cargo, "tauri", "icon", str(src_png)], cwd=ROOT)

    run([cargo, "tauri", "build"], cwd=ROOT)

    # 3. assemble a clean portable folder: dist/SteamSwitch/ = just the app exe +
    #    server[.exe], ready to zip and upload to GitHub Releases. (cargo tauri build
    #    also makes an NSIS/MSI installer under target/release/bundle/, but it does
    #    NOT include the sidecar — externalBin isn't wired — so use this folder.)
    rel = ROOT / "src-tauri" / "target" / "release"
    app_exe = next((rel / n for n in ("SteamSwitch" + EXE, "steamswitch" + EXE)
                    if (rel / n).exists()), None)
    if app_exe is None:
        sys.exit(f"Couldn't find the built app exe in {rel} — did `cargo tauri build` succeed?")

    portable = ROOT / "dist" / "SteamSwitch"
    shutil.rmtree(portable, ignore_errors=True)
    portable.mkdir(parents=True, exist_ok=True)
    shutil.copy2(app_exe, portable / ("SteamSwitch" + EXE))
    shutil.copy2(sidecar, portable / sidecar.name)
    # Also drop the sidecar next to the raw exe so running target/release directly works.
    shutil.copy2(sidecar, rel / sidecar.name)

    # Zip it: dist/SteamSwitch-portable.zip (unzips to a SteamSwitch/ folder).
    archive = shutil.make_archive(
        str(ROOT / "dist" / "SteamSwitch-portable"), "zip",
        root_dir=ROOT / "dist", base_dir="SteamSwitch")

    print("\nDone. Portable app folder (the two files that must stay together):")
    print("   ", portable / ("SteamSwitch" + EXE))
    print("   ", portable / sidecar.name)
    print("\nReady to share — upload this zip to GitHub Releases:")
    print("   ", archive)


if __name__ == "__main__":
    main()
