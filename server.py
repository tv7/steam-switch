"""SteamSwitch sidecar — a localhost HTTP + SSE bridge over the Python core.

This replaces webapp.py's pywebview bridge for the Tauri shell. The Steam logic
in `core/*` is untouched; this only changes the transport:

  * the front-end calls   POST /api/<method>   (JSON body = a list of args)
    instead of `window.pywebview.api.<method>(...)`;
  * the sidecar pushes progress as Server-Sent Events on GET /events
    ({"fn": "<name>", "payload": ...}) instead of `window.evaluate_js`.

The Tauri (Rust) window loads the `web/` UI and talks to this process over
127.0.0.1. For development this server ALSO serves `web/` statically, so the whole
thing can be exercised in a plain browser (no Tauri needed).

Pure standard library (http.server). Run:

    python server.py [--host 127.0.0.1] [--port 8731]

On start it prints a line `STEAMSWITCH_SIDECAR_READY <url>` to stdout so the shell
can discover the bound URL/port.
"""

from __future__ import annotations

import argparse
import base64
import json
import logging
import mimetypes
import os
import queue
import sys
import threading
import time
import traceback
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

from core import games, accounts, switcher, launcher, steam_paths, covers

APP_VERSION = "tauri"

# Distinct, vivid account colors, assigned by position (see webapp.py).
_ACCOUNT_PALETTE = [
    "#58a6ff", "#57cc99", "#ff8a65", "#c792ea", "#ff6384",
    "#ffd166", "#26c6da", "#f071b2", "#7cb342", "#9575cd",
]
_UNMAPPED_COLOR = "#c94f4f"

# Methods the front-end may POST to /api/<name>. A whitelist so the HTTP surface
# can't reach arbitrary attributes.
_ALLOWED = {"request_state", "request_cover", "play", "cancel",
            "set_language", "add_account"}

_LOG = logging.getLogger("steamswitch")


def _base_dir() -> Path:
    """Source dir, or the PyInstaller extraction dir when frozen."""
    return Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))


def _data_dir() -> Path:
    """A writable folder for app settings (next to the binary when frozen)."""
    base = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) \
        else Path(__file__).resolve().parent
    d = base / "data"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _load_settings() -> dict:
    try:
        return json.loads((_data_dir() / "settings.json").read_text("utf-8"))
    except (OSError, ValueError):
        return {}


def _save_settings(data: dict) -> None:
    try:
        (_data_dir() / "settings.json").write_text(
            json.dumps(data, indent=2), encoding="utf-8")
    except OSError:
        pass


def _asset_data_url(name: str) -> str | None:
    """Read a bundled image asset and return it as a data: URL."""
    try:
        data = (_base_dir() / "assets" / name).read_bytes()
    except OSError:
        return None
    mime = "image/png" if name.endswith(".png") else "image/x-icon"
    return f"data:{mime};base64," + base64.b64encode(data).decode("ascii")


class Bridge:
    """Same Api logic as webapp.py, but instead of pushing to JS through pywebview's
    evaluate_js it broadcasts Server-Sent Events to every connected /events client.

    Every public method still returns immediately and does the real work on a
    background thread, pushing results out as events — so a slow scan/launch never
    blocks the HTTP handler."""

    def __init__(self):
        self.accounts: list = []
        self.account_colors: dict[str, str] = {}
        self.games: list = []
        self.current_account: str | None = None
        self._launching = False
        self._cancel = threading.Event()
        self._state_lock = threading.Lock()
        self.pool = ThreadPoolExecutor(max_workers=6)

        # SSE fan-out: one queue per connected client.
        self._clients: set[queue.Queue] = set()
        self._clients_lock = threading.Lock()
        # Last full state, replayed to clients that connect after it was built, so a
        # reconnect/refresh (or an EventSource that opens slightly late) isn't blank.
        self._last_state: dict | None = None

    # ----------------------------------------------------------- SSE plumbing
    def add_client(self, q: queue.Queue) -> None:
        with self._clients_lock:
            self._clients.add(q)
            last = self._last_state
        if last is not None:                 # replay current state to the newcomer
            try:
                q.put_nowait(json.dumps({"fn": "onState", "payload": last}))
            except queue.Full:
                pass

    def remove_client(self, q: queue.Queue) -> None:
        with self._clients_lock:
            self._clients.discard(q)

    def _push(self, fn: str, payload) -> None:
        """Broadcast an event to all connected clients (fire-and-forget)."""
        if fn == "onState":
            self._last_state = payload
        msg = json.dumps({"fn": fn, "payload": payload})
        with self._clients_lock:
            dead = []
            for q in self._clients:
                try:
                    q.put_nowait(msg)
                except queue.Full:
                    dead.append(q)
            for q in dead:
                self._clients.discard(q)

    # ----------------------------------------------------------- helpers
    def _color_for(self, sid: str | None) -> str:
        return self.account_colors.get(sid, _UNMAPPED_COLOR) if sid else _UNMAPPED_COLOR

    def _account_name(self, sid: str | None) -> str | None:
        a = next((x for x in self.accounts if x.steamid64 == sid), None)
        return a.persona_name if a else None

    def _resolve_current(self):
        login = (self.current_account or "").lower()
        if login:
            for a in self.accounts:
                if a.account_name.lower() == login:
                    return a
        return next((a for a in self.accounts if a.most_recent), None)

    def _reload(self):
        self.accounts = accounts.list_accounts()
        self.account_colors = {
            a.steamid64: _ACCOUNT_PALETTE[i % len(_ACCOUNT_PALETTE)]
            for i, a in enumerate(self.accounts)
        }
        self.current_account = switcher.current_account_name()
        self.games = games.installed_games()
        accounts.local_owner_map(refresh=True)
        accounts.userdata_owner_map(refresh=True)

    # ------------------------------------------------------ state (internal)
    def _build_state(self) -> dict:
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

        current = self._resolve_current()
        current_sid = current.steamid64 if current else None
        accounts_out = []
        for a in self.accounts:
            ready, _why = switcher.can_autologin(a.account_name)
            accounts_out.append({
                "steamid64": a.steamid64,
                "account_name": a.account_name,
                "persona_name": a.persona_name,
                "color": self._color_for(a.steamid64),
                "ready": bool(ready),
                "current": a.steamid64 == current_sid,
                "game_count": counts.get(a.steamid64, 0),
            })

        root = steam_paths.steam_root()
        return {
            "version": APP_VERSION,
            "logo": _asset_data_url("steamswitch.png"),
            "language": _load_settings().get("language", "en"),
            "steam_root": str(root) if root else None,
            "current_account": current.persona_name if current else None,
            "accounts": accounts_out,
            "games": games_out,
        }

    # -------------------------------------------------------- request-facing
    # Each returns immediately; results stream back as SSE events. Signatures
    # mirror the old js_api so the front-end shim can call them 1:1.
    def request_state(self) -> dict:
        def work():
            _LOG.info("request_state: scan start")
            t0 = time.time()
            try:
                state = self._build_state()
            except Exception as exc:
                _LOG.error("request_state failed: %s\n%s", exc, traceback.format_exc())
                self._push("onStatus",
                           {"text": f"Failed to read Steam data: {exc}", "kind": "bad"})
                return
            _LOG.info("request_state: done in %.2fs (%d games, %d accounts)",
                      time.time() - t0, len(state["games"]), len(state["accounts"]))
            self._push("onState", state)
        threading.Thread(target=work, name="state-scan", daemon=True).start()
        return {"ok": True}

    def request_cover(self, appid: int) -> dict:
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
                threading.Timer(4.0, self.request_state).start()

        threading.Thread(target=work, name="launch", daemon=True).start()
        return {"ok": True}

    def cancel(self) -> dict:
        if self._launching:
            self._cancel.set()
            self._push("onStatus", {"text": "Cancelling launch…", "kind": ""})
        return {"ok": True}

    def set_language(self, lang: str) -> dict:
        s = _load_settings()
        s["language"] = lang
        _save_settings(s)
        return {"ok": True}

    def add_account(self) -> dict:
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


class Handler(BaseHTTPRequestHandler):
    """Serves the web/ UI statically, dispatches POST /api/<method> to the Bridge,
    and streams events on GET /events. The Bridge instance is on self.server.bridge."""

    protocol_version = "HTTP/1.1"      # keep-alive (needed for SSE)

    def log_message(self, *args):      # quiet; we use _LOG instead
        pass

    # --- CORS (the Tauri webview origin differs from 127.0.0.1) ---
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.send_header("Content-Length", "0")
        self.end_headers()

    def _json(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self._cors()
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    # ----------------------------------------------------------- GET
    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/events":
            return self._serve_events()
        if path in ("/health", "/api/health"):
            return self._json({"ok": True, "version": APP_VERSION})
        return self._serve_static(path)

    def _serve_static(self, path):
        root = (_base_dir() / "web").resolve()
        rel = path.lstrip("/") or "index.html"
        target = (root / rel).resolve()
        if root not in target.parents and target != root:    # path-traversal guard
            return self._json({"error": "forbidden"}, 403)
        if target.is_dir():
            target = target / "index.html"
        if not target.is_file():
            return self._json({"error": "not found", "path": path}, 404)
        try:
            data = target.read_bytes()
        except OSError:
            return self._json({"error": "unreadable"}, 500)
        ctype = mimetypes.guess_type(str(target))[0] or "application/octet-stream"
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self._cors()
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _serve_events(self):
        q: queue.Queue = queue.Queue(maxsize=2000)
        bridge = self.server.bridge
        bridge.add_client(q)
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self._cors()
        self.end_headers()
        try:
            self.wfile.write(b": connected\n\n")
            self.wfile.flush()
            while True:
                try:
                    msg = q.get(timeout=15)
                    self.wfile.write(f"data: {msg}\n\n".encode("utf-8"))
                except queue.Empty:
                    self.wfile.write(b": ping\n\n")        # keepalive / dead-peer detect
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            bridge.remove_client(q)

    # ----------------------------------------------------------- POST
    def do_POST(self):
        path = urlparse(self.path).path
        if not path.startswith("/api/"):
            return self._json({"error": "not found"}, 404)
        method = path[len("/api/"):]
        if method not in _ALLOWED:
            return self._json({"error": f"unknown method '{method}'"}, 404)

        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b""
        try:
            args = json.loads(raw) if raw else []
        except ValueError:
            return self._json({"error": "bad JSON body"}, 400)
        if not isinstance(args, list):
            args = [args]

        try:
            result = getattr(self.server.bridge, method)(*args)
        except TypeError as exc:
            return self._json({"error": f"bad args for {method}: {exc}"}, 400)
        except Exception as exc:
            _LOG.error("api %s failed: %s\n%s", method, exc, traceback.format_exc())
            return self._json({"error": str(exc)}, 500)
        return self._json(result if result is not None else {"ok": True})


def main():
    logging.basicConfig(stream=sys.stderr, level=logging.INFO,
                        format="%(asctime)s %(levelname)s [%(threadName)s] %(message)s")
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8731)
    args = ap.parse_args()

    bridge = Bridge()
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    httpd.daemon_threads = True
    httpd.bridge = bridge          # the handler reaches the bridge via self.server

    host, port = httpd.server_address
    url = f"http://{host}:{port}"
    # Discovery line for the shell (and humans).
    print(f"STEAMSWITCH_SIDECAR_READY {url}", flush=True)
    _LOG.info("sidecar listening on %s", url)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        httpd.shutdown()


if __name__ == "__main__":
    main()
