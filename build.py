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

    # 3. place the sidecar next to every built SteamSwitch executable --------
    rel = ROOT / "src-tauri" / "target" / "release"
    placed = []
    if rel.is_dir():
        for exe_name in ("steamswitch" + EXE, "SteamSwitch" + EXE):
            cand = rel / exe_name
            if cand.exists():
                dst = rel / sidecar.name
                shutil.copy2(sidecar, dst)
                placed.append(dst)
                break
        # bundled app folders (portable / installer payloads)
        bundle = rel / "bundle"
        if bundle.is_dir():
            for exe in list(bundle.rglob("SteamSwitch" + EXE)) + list(bundle.rglob("steamswitch" + EXE)):
                dst = exe.parent / sidecar.name
                shutil.copy2(sidecar, dst)
                placed.append(dst)

    print("\nDone. Sidecar placed next to:")
    for p in placed:
        print("   ", p)
    if not placed:
        print("   (couldn't locate the built exe — copy", sidecar, "next to it manually)")
    print("\nThe portable app is the SteamSwitch executable + server" + EXE + " together.")
    print("Run it and confirm it works; report the exact output paths if the layout differs.")


if __name__ == "__main__":
    main()
