"""Switch the active Steam account and control the Steam process.

Windows is fully implemented (registry AutoLoginUser + loginusers.vdf MostRecent).
Linux is stubbed for the next phase (it works the same way minus the registry).

Hard limit: we never type a password. Silent login only works if the target
account was previously logged in with "Remember me" so Steam holds a valid token.
`can_autologin()` / a failed launch is how we detect when that's no longer true.
"""

from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path

from . import vdf, steam_paths


# Convert between SteamID64 and the 32-bit account id Steam stores in ActiveUser.
STEAMID64_BASE = 76561197960265728


def accountid_from_steamid64(steamid64: str | int) -> int:
    return int(steamid64) - STEAMID64_BASE


# CREATE_NO_WINDOW: keep helper processes (tasklist, steam -shutdown) from
# flashing a console window when we run under pythonw / the packaged .exe.
# Without this, polling tasklist once a second pops a cmd window every second.
_CREATE_NO_WINDOW = 0x08000000


def _no_window() -> dict:
    return {"creationflags": _CREATE_NO_WINDOW} if sys.platform.startswith("win") else {}


# ---------------------------------------------------------------------------
# Reading current account
# ---------------------------------------------------------------------------

def current_account_name() -> str | None:
    """The account Steam will auto-login as (AutoLoginUser on Windows)."""
    if sys.platform.startswith("win"):
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
                val, _ = winreg.QueryValueEx(key, "AutoLoginUser")
                return val or None
        except OSError:
            return None
    # Unix: fall back to MostRecent in loginusers.vdf
    root = steam_paths.steam_root()
    if not root:
        return None
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return None
    users = vdf.load(f).get("users", {})
    for info in users.values():
        if isinstance(info, dict) and info.get("MostRecent") == "1":
            return info.get("AccountName")
    return None


# ---------------------------------------------------------------------------
# Live login state — the core of login-failure detection.
#
# Steam records the currently logged-in user's 32-bit account id at
# ActiveProcess\ActiveUser. It is 0 when nobody is logged in (i.e. Steam is
# sitting at the login screen). On Windows that lives in the registry; on
# Linux/macOS it lives in ~/.steam/registry.vdf.
# ---------------------------------------------------------------------------

def active_user_accountid() -> int | None:
    """Account id Steam is currently logged in as. 0 = login screen / logged out.
    Returns None if the value can't be read (Steam never run, etc.)."""
    if sys.platform.startswith("win"):
        try:
            import winreg
            with winreg.OpenKey(
                winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam\ActiveProcess"
            ) as key:
                val, _ = winreg.QueryValueEx(key, "ActiveUser")
                return int(val)
        except (OSError, ValueError):
            return None
    # Unix: dig through registry.vdf
    for cand in (
        Path.home() / ".steam" / "registry.vdf",
        Path.home() / ".steam" / "steam" / "registry.vdf",
    ):
        if cand.exists():
            try:
                reg = vdf.load(cand)
                node = reg.get("Registry", reg)
                for part in ("HKCU", "Software", "Valve", "Steam", "ActiveProcess"):
                    node = node.get(part, {})
                return int(node.get("ActiveUser", "0"))
            except (ValueError, AttributeError):
                return None
    return None


def is_logged_in_as(steamid64: str | int) -> bool:
    """True only if Steam is right now logged in as this exact account."""
    active = active_user_accountid()
    return active is not None and active != 0 and active == accountid_from_steamid64(steamid64)


def can_autologin(account_name: str) -> tuple[bool, str]:
    """Pre-flight: can Steam silently log into this account without a password?

    Returns (ok, reason). Silent login needs the account present in
    loginusers.vdf with RememberPassword enabled (i.e. logged in once with
    "Remember me"). This catches the common failure *before* we restart Steam.
    """
    root = steam_paths.steam_root()
    if not root:
        return False, "Steam install not found"
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return False, "no saved Steam logins on this PC"
    users = vdf.load(f).get("users", {})
    for info in users.values():
        if isinstance(info, dict) and info.get("AccountName", "").lower() == account_name.lower():
            if info.get("RememberPassword") != "1":
                return False, '"Remember me" is not enabled for this account'
            return True, ""
    return False, "this account has never been logged in on this PC"


def wait_for_login(steamid64: str | int, timeout: float = 40.0, poll: float = 1.0,
                   should_cancel=None) -> bool:
    """Block until Steam reports it is logged in as this account, or timeout.

    A False result means auto-login didn't complete — almost always an expired
    token that now needs a manual password + Steam Guard entry once, OR the
    caller cancelled via should_cancel().
    """
    deadline = time.time() + timeout
    while time.time() < deadline:
        if should_cancel and should_cancel():
            return False
        if is_logged_in_as(steamid64):
            return True
        time.sleep(poll)
    return is_logged_in_as(steamid64)


# ---------------------------------------------------------------------------
# loginusers.vdf updates (MostRecent + optional offline flag)
# ---------------------------------------------------------------------------

def _loginusers_path(root: Path | None = None) -> Path | None:
    root = root or steam_paths.steam_root()
    if not root:
        return None
    f = root / "config" / "loginusers.vdf"
    return f if f.exists() else None


def is_loginusers_readonly(root: Path | None = None) -> bool:
    """True if loginusers.vdf is read-only. Diagnostic only (core/offline_diff):
    a read-only lock was an abandoned offline approach — we don't set it anymore."""
    import os
    f = _loginusers_path(root)
    return bool(f) and not os.access(f, os.W_OK)


def _update_loginusers(account_name: str, *, root: Path) -> None:
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return
    data = vdf.load(f)
    users = data.get("users", {})
    for info in users.values():
        if not isinstance(info, dict):
            continue
        is_target = info.get("AccountName", "").lower() == account_name.lower()
        info["MostRecent"] = "1" if is_target else "0"
        if is_target:
            info["RememberPassword"] = "1"
            info["AllowAutoLogin"] = "1"
            # Clear any stale offline flag: a leftover "1" makes Steam HANG on this
            # (online) cold start. The offline flow sets WantsOfflineMode=1 itself,
            # later, via set_offline_mode() — after this online login mints the
            # session — so clearing it here is always correct for switch_account.
            info["WantsOfflineMode"] = "0"
            info["SkipOfflineModeWarning"] = "0"
        else:
            # Leave the other accounts remembered, but make sure none of them is
            # *also* flagged for auto-login — when several are, modern Steam stops
            # silently logging in and shows its account picker instead.
            info["AllowAutoLogin"] = "0"
    # Back up once before the first write.
    backup = f.with_suffix(".vdf.bak")
    if not backup.exists():
        backup.write_bytes(f.read_bytes())
    vdf.dump(data, f)


def set_offline_mode(account: str | int, want_offline: bool = True,
                     *, root: Path | None = None) -> bool:
    """Set WantsOfflineMode for `account` in loginusers.vdf and return True on write.

    `account` is matched against either the SteamID64 key or the AccountName.
    This is the flag method: Steam reads WantsOfflineMode at startup and comes up
    offline when it's "1" AND a valid cached session exists (i.e. the account just
    logged in online). SkipOfflineModeWarning is set alongside it so Steam doesn't
    stop on the "Start in Offline Mode?" dialog.

    Write order (validated manually): set this WHILE Steam is running and logged in
    online as the account, THEN exit Steam, THEN restart it — Steam comes up offline.
    Callers do: wait_for_login (online) -> set_offline_mode -> shutdown_steam ->
    start_steam -> confirm.
    """
    root = root or steam_paths.steam_root()
    if not root:
        return False
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return False
    data = vdf.load(f)
    users = data.get("users", {})
    target = str(account).lower()
    wrote = False
    for sid, info in users.items():
        if not isinstance(info, dict):
            continue
        if str(sid).lower() == target or info.get("AccountName", "").lower() == target:
            info["WantsOfflineMode"] = "1" if want_offline else "0"
            info["SkipOfflineModeWarning"] = "1" if want_offline else "0"
            wrote = True
    if not wrote:
        return False
    backup = f.with_suffix(".vdf.bak")
    if not backup.exists():
        backup.write_bytes(f.read_bytes())
    vdf.dump(data, f)
    return True


def wants_offline_mode(account: str | int | None = None,
                       *, root: Path | None = None) -> bool:
    """True if `account` (or, if None, any account) has WantsOfflineMode=1 in
    loginusers.vdf. This is the success signal: Steam keeps the flag at "1" while
    it is offline, so reading it after the offline restart confirms it took."""
    root = root or steam_paths.steam_root()
    if not root:
        return False
    f = root / "config" / "loginusers.vdf"
    if not f.exists():
        return False
    try:
        users = vdf.load(f).get("users", {})
    except Exception:
        return False
    target = None if account is None else str(account).lower()
    for sid, info in users.items():
        if not isinstance(info, dict):
            continue
        if target is not None and str(sid).lower() != target \
                and info.get("AccountName", "").lower() != target:
            continue
        if info.get("WantsOfflineMode") == "1":
            return True
    return False


def _set_registry_autologin(account_name: str) -> None:
    import winreg
    with winreg.OpenKey(
        winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", 0, winreg.KEY_SET_VALUE
    ) as key:
        winreg.SetValueEx(key, "AutoLoginUser", 0, winreg.REG_SZ, account_name)
        winreg.SetValueEx(key, "RememberPassword", 0, winreg.REG_DWORD, 1)


def _child(node: object, name: str):
    """Case-insensitive child lookup in a parsed VDF dict (keys vary in case)."""
    if not isinstance(node, dict):
        return None
    for k, v in node.items():
        if k.lower() == name.lower() and isinstance(v, dict):
            return v
    return None


def set_account_picker(root: Path, show: bool) -> bool:
    """Toggle Steam's account picker (AlwaysShowUserChooser in config/config.vdf).

    Login here needs the picker ON: for accounts that log in via the picker's
    saved-token click, disabling it leaves Steam with nothing to click and no
    silent path, so it neither shows the picker nor logs in. switch_account
    always turns it on.

    Backs up config.vdf → .bak once. Bails without writing if the expected
    structure isn't found, so we never corrupt config.vdf. Returns True if the
    setting is (now) as requested.
    """
    desired = "1" if show else "0"
    f = root / "config" / "config.vdf"
    if not f.exists():
        return False
    try:
        data = vdf.load(f)
    except Exception:
        return False
    node = data
    for part in ("InstallConfigStore", "Software", "Valve", "Steam"):
        node = _child(node, part)
        if node is None:
            return False
    if node.get("AlwaysShowUserChooser") == desired:
        return True  # already set; nothing to write
    node["AlwaysShowUserChooser"] = desired
    backup = f.with_suffix(".vdf.bak")
    if not backup.exists():
        backup.write_bytes(f.read_bytes())
    vdf.dump(data, f)
    return True


# ---------------------------------------------------------------------------
# Process control
# ---------------------------------------------------------------------------

def _steam_running() -> bool:
    if sys.platform.startswith("win"):
        out = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq steam.exe"],
            capture_output=True, text=True, **_no_window(),
        ).stdout.lower()
        return "steam.exe" in out
    return subprocess.run(["pgrep", "-x", "steam"], capture_output=True).returncode == 0


def steam_window_present() -> bool:
    """True once Steam's main window has appeared — i.e. it reached its UI and is
    NOT stuck on the connecting splash. This is our 'Steam is really up' signal for
    the offline restart, because offline login does not reliably report via
    ActiveUser. Windows only; returns False elsewhere or if the window isn't found.

    Enumerates top-level windows for class 'SDL_app' titled exactly "Steam" (the
    main window). We're lenient on the class (Valve has changed it) but require the
    exact title so we don't match a friends/chat/game window.
    """
    if not sys.platform.startswith("win"):
        return False
    try:
        import ctypes
        u = ctypes.windll.user32
        HWND = ctypes.c_void_p
        u.IsWindowVisible.argtypes = [HWND]
        u.GetClassNameW.argtypes = [HWND, ctypes.c_wchar_p, ctypes.c_int]
        u.GetWindowTextW.argtypes = [HWND, ctypes.c_wchar_p, ctypes.c_int]
        found: list[int] = []

        CMPFUNC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

        def cb(hwnd, _lparam):
            if not u.IsWindowVisible(hwnd):
                return True
            cls = ctypes.create_unicode_buffer(256)
            u.GetClassNameW(hwnd, cls, 256)
            title = ctypes.create_unicode_buffer(256)
            u.GetWindowTextW(hwnd, title, 256)
            if title.value == "Steam" and (cls.value == "SDL_app" or "Steam" in cls.value):
                found.append(hwnd)
                return False
            return True

        u.EnumWindows(CMPFUNC(cb), 0)
        return bool(found)
    except Exception:
        return False


def _force_kill_steam() -> None:
    """Hard-kill Steam and its child processes (steamwebhelper, etc.)."""
    if sys.platform.startswith("win"):
        # /T kills the whole tree so steamwebhelper.exe doesn't keep it alive.
        subprocess.run(["taskkill", "/F", "/T", "/IM", "steam.exe"],
                       capture_output=True, **_no_window())
    else:
        subprocess.run(["pkill", "-9", "-x", "steam"], capture_output=True)


def shutdown_steam(timeout: float = 30.0, *, force_after: float = 12.0) -> bool:
    """Make sure Steam is fully closed before we switch accounts.

    Tries a clean `steam.exe -shutdown` first (lets Steam flush its config), but
    if it's still alive after `force_after` seconds — common when Steam is just
    sitting in the tray, or is busy/unresponsive — we force-kill it. Without this
    a still-running Steam ignores the account switch and the game won't launch.
    Returns True once no Steam process remains.
    """
    if not _steam_running():
        return True
    exe = steam_paths.steam_executable()
    if exe:
        subprocess.run([str(exe), "-shutdown"], capture_output=True, **_no_window())
    start = time.time()
    forced = False
    while time.time() - start < timeout:
        if not _steam_running():
            return True
        if not forced and time.time() - start >= force_after:
            _force_kill_steam()
            forced = True
        time.sleep(1.0)
    if _steam_running():
        _force_kill_steam()   # last resort at the deadline
        time.sleep(2.0)
    return not _steam_running()


def start_steam() -> None:
    """Start Steam with its window visible. We deliberately pass no flags: -silent
    hides the account picker we need the user to click, and -offline is ignored by
    modern Steam (offline is driven by the WantsOfflineMode flag — set_offline_mode
    — read at startup, not by a launch flag)."""
    exe = steam_paths.steam_executable()
    if not exe:
        raise RuntimeError("Steam executable not found")
    subprocess.Popen([str(exe)], **_no_window())


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def switch_account(account_name: str) -> None:
    """Make `account_name` the account Steam auto-logs into. Steam must be
    restarted afterwards for this to take effect (see launcher.play)."""
    root = steam_paths.steam_root()
    if not root:
        raise RuntimeError("Steam install not found")
    if sys.platform.startswith("win"):
        _set_registry_autologin(account_name)
    _update_loginusers(account_name, root=root)
    # Login needs the picker ON — the picker click is what logs these accounts in.
    set_account_picker(root, show=True)
