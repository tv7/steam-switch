"""SteamSwitch — pywebview front-end (HTML/CSS/JS UI over the Python core).

This is the Proton-Dark UI. It renders the interface in a native webview (Edge
WebView2 on Windows) and talks to the existing `core/*` modules through a thin
JS↔Python bridge (the `Api` class below). All Steam logic — account switching,
launching, the offline flag method, cover resolution — lives in `core/`; this
file only wires it to the web UI.

Run:   python webapp.py        (needs `pip install pywebview`; on Windows it uses
                                the built-in Edge WebView2 runtime)
"""

from __future__ import annotations

import base64
import json
import os
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from core import games, accounts, switcher, launcher, steam_paths, covers

try:
    import webview
except ImportError:      # pragma: no cover - import guard
    webview = None

APP_VERSION = "proton-dark"

# Distinct, vivid account colors, assigned by position: a SteamID hash-to-hue
# clusters into lookalikes because real SteamIDs differ only in their last digits,
# so position gives reliably different colors.
_ACCOUNT_PALETTE = [
    "#58a6ff", "#57cc99", "#ff8a65", "#c792ea", "#ff6384",
    "#ffd166", "#26c6da", "#f071b2", "#7cb342", "#9575cd",
]
_UNMAPPED_COLOR = "#c94f4f"


def _base_dir() -> Path:
    """Source dir, or the PyInstaller extraction dir when frozen into an .exe."""
    return Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))


def _asset_data_url(name: str) -> str | None:
    """Read a bundled image asset and return it as a data: URL (so the page can
    show it without relative file:// paths, identically from source and from the
    frozen .exe)."""
    try:
        data = (_base_dir() / "assets" / name).read_bytes()
    except OSError:
        return None
    mime = "image/png" if name.endswith(".png") else "image/x-icon"
    return f"data:{mime};base64," + base64.b64encode(data).decode("ascii")


class Api:
    """The JS-facing bridge. Every public method is callable from the front-end as
    `window.pywebview.api.<name>(...)`.

    CRITICAL: pywebview may run js_api calls on the GUI/message-loop thread, so a
    bridge method that does file/registry/**network** I/O would freeze the window
    ("not responding" — especially under pythonw, which has no console). So the
    JS-facing methods here NEVER block: they kick the work onto a background thread
    and return immediately, then push results back into the page via
    `self.window.evaluate_js(...)` (the `on*` globals)."""

    def __init__(self):
        self.window = None
        self.accounts: list = []
        self.account_colors: dict[str, str] = {}
        self.games: list = []
        self.current_account: str | None = None
        self._launching = False
        self._cancel = threading.Event()
        self._state_lock = threading.Lock()   # guards _reload / get_state scans
        self.pool = ThreadPoolExecutor(max_workers=6)

    # ----------------------------------------------------------- helpers
    def _color_for(self, sid: str | None) -> str:
        return self.account_colors.get(sid, _UNMAPPED_COLOR) if sid else _UNMAPPED_COLOR

    def _account_name(self, sid: str | None) -> str | None:
        a = next((x for x in self.accounts if x.steamid64 == sid), None)
        return a.persona_name if a else None

    def _reload(self):
        """Refresh accounts + games + ownership maps from disk."""
        self.accounts = accounts.list_accounts()
        self.account_colors = {
            a.steamid64: _ACCOUNT_PALETTE[i % len(_ACCOUNT_PALETTE)]
            for i, a in enumerate(self.accounts)
        }
        self.current_account = switcher.current_account_name()
        self.games = games.installed_games()
        accounts.local_owner_map(refresh=True)
        accounts.userdata_owner_map(refresh=True)

    def _push(self, fn: str, payload):
        """Call a global JS function with a JSON payload (fire-and-forget)."""
        if self.window is not None:
            try:
                self.window.evaluate_js(f"window.{fn}({json.dumps(payload)})")
            except Exception:
                pass

    # ------------------------------------------------------ state (internal)
    def _build_state(self) -> dict:
        """Everything the UI needs for a full render. Scans disk/registry, so it
        runs on a worker (see request_state), never on the GUI thread."""
        with self._state_lock:
            return self._build_state_locked()

    def _build_state_locked(self) -> dict:
        self._reload()

        counts: dict[str, int] = {}
        games_out = []
        for g in self.games:
            sid = accounts.account_for_game(g.appid, self.accounts)
            if sid:
                counts[sid] = counts.get(sid, 0) + 1
            games_out.append({
                "appid": g.appid,
                "name": g.name,
                "owner_sid": sid,
                "owner_name": self._account_name(sid) if sid else None,
                "owner_color": self._color_for(sid),
                "mapped": bool(sid),
            })

        accounts_out = []
        for a in self.accounts:
            ready, _why = switcher.can_autologin(a.account_name)
            is_current = (a.account_name == self.current_account
                          or a.persona_name == self.current_account)
            accounts_out.append({
                "steamid64": a.steamid64,
                "account_name": a.account_name,
                "persona_name": a.persona_name,
                "color": self._color_for(a.steamid64),
                "ready": bool(ready),
                "current": is_current,
                "game_count": counts.get(a.steamid64, 0),
            })

        root = steam_paths.steam_root()
        return {
            "version": APP_VERSION,
            "logo": _asset_data_url("steamswitch.png"),
            "steam_root": str(root) if root else None,
            "current_account": self.current_account,
            "accounts": accounts_out,
            "games": games_out,
        }

    # -------------------------------------------------------- JS-facing
    def request_state(self) -> dict:
        """Kick off a full state scan on a worker; the result is pushed to the page
        as onState. Returns immediately so the GUI thread never blocks. Used on load
        and on refresh."""
        def work():
            try:
                state = self._build_state()
            except Exception as exc:
                self._push("onStatus",
                           {"text": f"Failed to read Steam data: {exc}", "kind": "bad"})
                return
            self._push("onState", state)
        threading.Thread(target=work, daemon=True).start()
        return {"ok": True}

    def request_cover(self, appid: int) -> dict:
        """Fetch one game's cover on a worker (covers.cover_bytes may hit the network)
        and push it to the page as onCover {appid, url}. url is null if none found.
        Returns immediately. Lazy-called per card as it scrolls into view."""
        appid = int(appid)

        def work():
            url = None
            try:
                data = covers.cover_bytes(appid)
                if data:
                    mime = ("image/png" if data[:8] == b"\x89PNG\r\n\x1a\n"
                            else "image/jpeg")
                    url = f"data:{mime};base64," + base64.b64encode(data).decode("ascii")
            except Exception:
                url = None
            self._push("onCover", {"appid": appid, "url": url})

        self.pool.submit(work)
        return {"ok": True}

    def play(self, appid: int, offline: bool = False) -> dict:
        """Switch to the owning account and launch the game. Returns immediately;
        progress + the final result are pushed to the page via onStatus/onLaunchDone.
        Single-launch guard + cooperative cancel via should_cancel."""
        appid = int(appid)
        if self._launching:
            return {"ok": False, "error": "busy",
                    "message": "A launch is already in progress — please wait…"}
        owner = accounts.account_for_game(appid, self.accounts)
        game = next((g for g in self.games if g.appid == appid), None)
        name = game.name if game else str(appid)
        if not owner:
            return {"ok": False, "error": "unmapped", "name": name,
                    "message": f'"{name}" isn\'t mapped to an account yet.'}

        self._launching = True
        self._cancel.clear()
        self._push("onLaunchStart", {"appid": appid, "name": name})
        self._push("onStatus", {"text": f'Starting "{name}"…', "kind": ""})

        def notify(msg):
            self._push("onStatus", {"text": msg, "kind": ""})

        def work():
            try:
                res = launcher.play(appid, offline=bool(offline), notify=notify,
                                    should_cancel=self._cancel.is_set)
            except Exception as exc:
                res = launcher.PlayResult(False, f"Launch failed: {exc}")
            self._launching = False
            self._push("onLaunchDone", {
                "ok": res.ok, "message": res.message,
                "needs_login": res.needs_login, "switched": res.switched,
            })
            if res.switched:
                # account changed — hand the page a fresh state after Steam settles
                threading.Timer(4.0, self.request_state).start()

        threading.Thread(target=work, daemon=True).start()
        return {"ok": True}

    def cancel(self) -> dict:
        """Abort an in-progress launch (wrong game picked, etc.)."""
        if self._launching:
            self._cancel.set()
            self._push("onStatus", {"text": "Cancelling launch…", "kind": ""})
        return {"ok": True}

    def add_account(self) -> dict:
        """Restart Steam to its login screen so the user can add another account.
        Runs off-thread (shutdown polls for a while); the page shows progress."""
        if self._launching:
            return {"ok": False, "message": "A launch is in progress — please wait…"}

        def work():
            self._push("onStatus", {"text": "Closing Steam and opening the login screen…",
                                    "kind": ""})
            try:
                ok = switcher.restart_to_add_account()
            except Exception as e:
                self._push("onStatus", {"text": f"Couldn't open Steam: {e}", "kind": "bad"})
                return
            if ok:
                self._push("onStatus", {
                    "text": ("Steam is opening its login screen. Sign in with "
                             "“Remember me” checked, then reopen Accounts."),
                    "kind": "good"})
                threading.Timer(45.0, self.request_state).start()
            else:
                self._push("onStatus", {
                    "text": "Couldn't close Steam — close it manually and try again.",
                    "kind": "bad"})

        self.pool.submit(work)
        return {"ok": True}


def main():
    # Under pythonw.exe (how SteamSwitch.bat launches us — no console) sys.stdout
    # and sys.stderr are None; any library that writes to them would crash. Point
    # them at the null device so logging from pywebview/pythonnet is harmless.
    for stream in ("stdout", "stderr"):
        if getattr(sys, stream) is None:
            setattr(sys, stream, open(os.devnull, "w"))

    if webview is None:
        sys.stderr.write(
            "pywebview is not installed. Install it with:\n\n"
            "    pip install -r requirements.txt\n\n"
            "On Windows it uses the built-in Edge WebView2 runtime.\n")
        sys.exit(1)

    api = Api()
    index = _base_dir() / "web" / "index.html"
    window = webview.create_window(
        "SteamSwitch", url=str(index), js_api=api,
        width=1100, height=720, min_size=(820, 520),
        background_color="#0b1326",
    )
    api.window = window

    # Window / taskbar icon. The frozen .exe gets its icon from PyInstaller's
    # --icon; this covers running from source where supported by the backend.
    # Windows (EdgeChromium) builds a System.Drawing.Icon and REQUIRES a real
    # .ico — a .png throws "must be a picture that can be used as a Icon"; other
    # backends (GTK/Qt) want a .png.
    icon = _base_dir() / "assets" / (
        "steamswitch.ico" if sys.platform.startswith("win") else "steamswitch.png")
    try:
        webview.start(icon=str(icon))
    except TypeError:           # older pywebview without an `icon` kwarg
        webview.start()


if __name__ == "__main__":
    main()
