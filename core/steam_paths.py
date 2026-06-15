"""Locate the Steam installation and all of its library folders.

Cross-platform: Windows reads the registry, Linux/macOS check the usual dirs.
Library folders are then read from libraryfolders.vdf (a single Steam install
can spread games across several drives).
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

from . import vdf


def _steam_root_windows() -> Path | None:
    try:
        import winreg
    except ImportError:
        return None
    candidates = [
        (winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam", "InstallPath"),
    ]
    for hive, subkey, value in candidates:
        try:
            with winreg.OpenKey(hive, subkey) as key:
                path, _ = winreg.QueryValueEx(key, value)
                p = Path(path)
                if p.exists():
                    return p
        except OSError:
            continue
    return None


def _steam_root_unix() -> Path | None:
    home = Path.home()
    candidates = [
        home / ".steam" / "steam",
        home / ".steam" / "root",
        home / ".local" / "share" / "Steam",
        home / "Library" / "Application Support" / "Steam",  # macOS
    ]
    for p in candidates:
        if (p / "steamapps").exists() or (p / "config").exists():
            return p.resolve()
    return None


def steam_root() -> Path | None:
    """Return the root Steam install directory, or None if not found."""
    env = os.environ.get("STEAM_ROOT")
    if env and Path(env).exists():
        return Path(env)
    if sys.platform.startswith("win"):
        return _steam_root_windows()
    return _steam_root_unix()


def steam_executable(root: Path | None = None) -> Path | None:
    """Path to the Steam executable (used to start Steam and to run -shutdown)."""
    root = root or steam_root()
    if not root:
        return None
    if sys.platform.startswith("win"):
        exe = root / "steam.exe"
        return exe if exe.exists() else None
    # Linux/macOS usually have `steam` on PATH; fall back to that.
    local = root / "steam.sh"
    if local.exists():
        return local
    return Path("steam")


def library_folders(root: Path | None = None) -> list[Path]:
    """All Steam library folders (each contains a steamapps/ dir)."""
    root = root or steam_root()
    if not root:
        return []
    libs: list[Path] = []
    for candidate in (
        root / "steamapps" / "libraryfolders.vdf",
        root / "config" / "libraryfolders.vdf",
    ):
        if candidate.exists():
            data = vdf.load(candidate)
            folders = data.get("libraryfolders", data)
            for _, entry in folders.items():
                if isinstance(entry, dict) and "path" in entry:
                    p = Path(entry["path"])
                    if p.exists() and p not in libs:
                        libs.append(p)
            break
    # Always include the root install as a library.
    if root not in libs and (root / "steamapps").exists():
        libs.insert(0, root)
    return libs


if __name__ == "__main__":
    r = steam_root()
    print("Steam root:", r)
    print("Executable:", steam_executable(r))
    print("Libraries:")
    for lib in library_folders(r):
        print("  -", lib)
