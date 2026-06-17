"""Build a single portable executable — no Python, no install needed to run it.

Usage (run once on the machine matching your target OS):

    python build.py

Produces:  dist/SteamSwitch.exe   (Windows)
           dist/SteamSwitch       (Linux/macOS)

PyInstaller can't cross-compile, so build the Windows .exe on Windows.
This script installs PyInstaller + Pillow into the *current* Python if missing
(only needed to build; the resulting exe is self-contained).
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def ensure(pkg):
    try:
        __import__(pkg.split("==")[0].replace("-", "_").lower())
    except ImportError:
        print(f"Installing build dependency: {pkg}")
        subprocess.check_call([sys.executable, "-m", "pip", "install", pkg])


def main(web: bool = False):
    ensure("PyInstaller")
    ensure("Pillow")

    # Make sure the logo/icon assets exist (they're drawn with Pillow).
    assets = ROOT / "assets"
    icon = assets / "steamswitch.ico"
    if not icon.exists():
        subprocess.check_call([sys.executable, str(assets / "make_icon.py")])

    name = "SteamSwitch"
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--windowed",            # no console window
        "--name", name,
        "--noconfirm",
        "--clean",
        # bundle the icon assets so the app can load them at runtime
        "--add-data", f"{assets}{os.pathsep}assets",
    ]
    if web:
        # The pywebview UI: bundle the web/ assets and pull in pywebview's platform
        # backends (on Windows the Edge WebView2 backend needs pythonnet/clr).
        ensure("pywebview")
        cmd += ["--add-data", f"{ROOT / 'web'}{os.pathsep}web",
                "--collect-all", "webview"]
        entry = ROOT / "webapp.py"
    else:
        entry = ROOT / "app.py"
    if icon.exists():
        cmd += ["--icon", str(icon)]
    cmd.append(str(entry))
    print("Running:", " ".join(cmd))
    subprocess.check_call(cmd, cwd=ROOT)

    out = ROOT / "dist" / (name + (".exe" if sys.platform.startswith("win") else ""))
    print("\nDone. Portable executable:")
    print("   ", out)
    print("\nCopy that single file anywhere and double-click to run.")


if __name__ == "__main__":
    # `python build.py`          -> pywebview UI (HTML/CSS, the default)
    # `python build.py classic`  -> Tkinter UI (dependency-free fallback)
    main(web="classic" not in sys.argv[1:])
