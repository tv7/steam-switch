"""SteamSwitch — Steam multi-account launcher, native desktop GUI (Tkinter).

Run:  python app.py        (Tkinter ships with Python — nothing to install)

Or build a single portable .exe with no Python required:  python build.py

Cover art shows up when the Pillow library is available (it's bundled into the
.exe build). Without Pillow the app still works fully, showing clean text tiles
instead of images.
"""

from __future__ import annotations

import sys
import threading
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import tkinter as tk
from tkinter import ttk, messagebox

from core import games, accounts, switcher, launcher, steam_paths

try:
    from PIL import Image, ImageTk
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False

# Steam-ish palette. Background shades go PANEL (darkest) < BG < CARD; TEXT and
# the accent/status colors are all chosen to stay readable on those backgrounds.
BG = "#1b2838"
PANEL = "#16202d"
CARD = "#2a3f5a"
ACCENT = "#66c0f4"
ACCENT2 = "#417a9b"
ACCENT2_HOVER = "#4f8fb3"   # button hover — lighter blue, white text still reads
TEXT = "#c7d5e0"
BAD = "#c94f4f"
BAD_HOVER = "#b04545"       # darker red on hover so white text stays high-contrast
GOOD = "#5ba32b"
MUTED = "#8aa0b6"           # secondary text / placeholder, legible on dark panels
DIM = "#33414f"            # inactive control fill (disabled button / toggle OFF)

CARD_W, CARD_H = 150, 225
COVER_DIR = Path(__file__).resolve().parent / "data" / "covers"


def _asset(name: str) -> Path:
    """Path to a bundled asset, working from source and from a PyInstaller .exe
    (which extracts bundled data under sys._MEIPASS)."""
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return base / "assets" / name


def local_cover(appid: int) -> Path | None:
    """Steam caches cover art locally; reuse it so we work offline too."""
    root = steam_paths.steam_root()
    if not root:
        return None
    cache = root / "appcache" / "librarycache"
    for cand in (
        cache / f"{appid}_library_600x900.jpg",      # older flat layout
        cache / str(appid) / "library_600x900.jpg",  # newer per-app layout
    ):
        if cand.exists():
            return cand
    return None


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("SteamSwitch")
        self.configure(bg=BG)
        self.geometry("1000x700")
        self.minsize(640, 480)
        self._set_app_icon()

        self.games: list = []
        self.accounts: list = []
        self.current_account: str | None = None
        self.images: dict[int, object] = {}   # keep PhotoImage refs alive
        self.cards: list[tk.Widget] = []
        self._cols = 0
        self._launching = False   # guard: only one account-switch/launch at a time
        self._cancel = threading.Event()  # set to abort an in-progress launch
        self.pool = ThreadPoolExecutor(max_workers=6)

        self._build_chrome()
        self.after(100, self.refresh_all)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _set_app_icon(self):
        """Title-bar / taskbar icon. Best-effort — never crash if assets are
        missing. On Windows a multi-size .ico is crispest; PNG elsewhere."""
        try:
            if sys.platform.startswith("win") and _asset("steamswitch.ico").exists():
                self.iconbitmap(default=str(_asset("steamswitch.ico")))
                return
            png = _asset("steamswitch.png")
            if png.exists():
                self._icon_img = tk.PhotoImage(file=str(png))  # keep a ref alive
                self.iconphoto(True, self._icon_img)
        except Exception:
            pass

    # ------------------------------------------------------------------ UI
    def _build_chrome(self):
        bar = tk.Frame(self, bg=PANEL)
        bar.pack(fill="x")

        # Brand: the SteamSwitch mark + wordmark. Falls back to text alone if the
        # logo asset isn't present (e.g. assets/make_icon.py hasn't been run).
        self._logo_img = None
        try:
            if _asset("steamswitch_28.png").exists():
                self._logo_img = tk.PhotoImage(file=str(_asset("steamswitch_28.png")))
        except Exception:
            self._logo_img = None
        tk.Label(bar, text=" SteamSwitch", image=self._logo_img, compound="left",
                 bg=PANEL, fg="#fff", font=("Segoe UI", 13, "bold")
                 ).pack(side="left", padx=14, pady=8)

        self.account_lbl = tk.Label(bar, text="…", bg=PANEL, fg=ACCENT,
                                    font=("Segoe UI", 10))
        self.account_lbl.pack(side="left", padx=10)

        tk.Button(bar, text="Accounts", command=self.open_accounts,
                  bg=ACCENT2, fg="#fff", activebackground=ACCENT2_HOVER,
                  activeforeground="#fff", relief="flat", padx=10
                  ).pack(side="right", padx=12)

        # Cancel button: abort a launch that's underway (e.g. wrong game picked).
        # Starts dim/disabled (no launch yet); _set_cancel_enabled() turns it red
        # + clickable during a launch. disabledforeground keeps the label readable
        # while disabled (Tk ignores `fg` for disabled buttons).
        self.cancel_btn = tk.Button(bar, text="Stop launch", command=self.cancel_launch,
                                    bg=DIM, fg="#fff", activebackground=BAD_HOVER,
                                    activeforeground="#fff", disabledforeground=MUTED,
                                    relief="flat", padx=10, state="disabled")
        self.cancel_btn.pack(side="right", padx=4)

        # ttk.Checkbutton, not tk.Checkbutton: it uses the OS-native indicator — a
        # white box with a crisp dark tick drawn by Windows, independent of the
        # label colour. (A classic checkbutton draws the tick in `fg`, which has to
        # be light for the label on the dark bar, so the tick was invisible on
        # white.) The style keeps the label text/background on-theme.
        self.offline_var = tk.BooleanVar(value=False)
        ck_style = ttk.Style(self)
        ck_style.configure("Offline.TCheckbutton", background=PANEL, foreground=TEXT)
        ck_style.map("Offline.TCheckbutton",
                     background=[("active", PANEL), ("selected", PANEL),
                                 ("pressed", PANEL)],
                     foreground=[("active", ACCENT)])
        ttk.Checkbutton(bar, text="Launch offline", variable=self.offline_var,
                        style="Offline.TCheckbutton", takefocus=False
                        ).pack(side="right", padx=4)

        # search
        sb = tk.Frame(self, bg=BG)
        sb.pack(fill="x", padx=16, pady=(12, 4))
        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", lambda *_: self.render_grid())
        e = tk.Entry(sb, textvariable=self.search_var, bg=PANEL, fg=TEXT,
                     insertbackground=TEXT, relief="flat")
        e.pack(fill="x", ipady=5)
        e.insert(0, "")
        self._placeholder(e, "Filter games…")

        # scrollable grid
        wrap = tk.Frame(self, bg=BG)
        wrap.pack(fill="both", expand=True, padx=8)
        self.canvas = tk.Canvas(wrap, bg=BG, highlightthickness=0)
        vsb = ttk.Scrollbar(wrap, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=vsb.set)
        vsb.pack(side="right", fill="y")
        self.canvas.pack(side="left", fill="both", expand=True)
        self.grid_frame = tk.Frame(self.canvas, bg=BG)
        self._win = self.canvas.create_window((0, 0), window=self.grid_frame, anchor="nw")
        self.grid_frame.bind("<Configure>",
                             lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.bind("<Configure>", self._on_canvas_resize)
        self._bind_scroll()

        # status bar
        self.status = tk.Label(self, text="", bg=PANEL, fg=TEXT, anchor="w")
        self.status.pack(fill="x", side="bottom", ipady=4)
        if not HAVE_PIL:
            self.set_status("Tip: covers appear when Pillow is installed "
                            "(it's bundled in the .exe build).")

    def _placeholder(self, entry, text):
        def on_focus_in(_):
            if entry.get() == text:
                entry.delete(0, "end"); entry.config(fg=TEXT)
        def on_focus_out(_):
            if not entry.get():
                entry.insert(0, text); entry.config(fg=MUTED)
        entry.insert(0, text); entry.config(fg=MUTED)
        entry.bind("<FocusIn>", on_focus_in)
        entry.bind("<FocusOut>", on_focus_out)

    def _bind_scroll(self):
        def on_wheel(e):
            delta = -1 * (e.delta // 120) if e.delta else (1 if e.num == 5 else -1)
            self.canvas.yview_scroll(delta, "units")
        self.canvas.bind_all("<MouseWheel>", on_wheel)
        self.canvas.bind_all("<Button-4>", on_wheel)
        self.canvas.bind_all("<Button-5>", on_wheel)

    def _on_canvas_resize(self, event):
        self.canvas.itemconfig(self._win, width=event.width)
        cols = max(1, event.width // (CARD_W + 14))
        if cols != self._cols:
            self._cols = cols
            self.render_grid()

    def set_status(self, text, kind=""):
        color = {"bad": BAD, "good": GOOD}.get(kind, TEXT)
        self.status.config(text=text, fg=color)

    def _set_cancel_enabled(self, on: bool):
        """Stop-launch button: red + clickable while a launch runs, dim otherwise
        (a permanently-red disabled button looks broken and its text goes dark)."""
        self.cancel_btn.config(state="normal" if on else "disabled",
                               bg=BAD if on else DIM)

    # -------------------------------------------------------------- data
    def refresh_all(self):
        self.reload_accounts()
        self.reload_games()

    def reload_accounts(self):
        self.accounts = accounts.list_accounts()
        self.current_account = switcher.current_account_name()
        self.account_lbl.config(
            text=f"Logged in: {self.current_account}" if self.current_account
            else "No active account")

    def reload_games(self):
        self.games = games.installed_games()
        accounts.local_owner_map(refresh=True)     # rebuild appid->LastOwner
        accounts.userdata_owner_map(refresh=True)  # rebuild appid->local player
        if not steam_paths.steam_root():
            self.set_status("Steam install not found on this PC.", "bad")
        elif not self.games:
            self.set_status("No installed games found.", "bad")
        else:
            self.set_status(f"{len(self.games)} games installed.")
        self._cols = 0  # force a re-grid
        self.render_grid()

    def _account_name(self, sid):
        a = next((x for x in self.accounts if x.steamid64 == sid), None)
        return a.persona_name if a else None

    # ------------------------------------------------------------- render
    def render_grid(self):
        for c in self.cards:
            c.destroy()
        self.cards.clear()
        cols = max(1, self._cols or 5)
        flt = self.search_var.get().strip().lower()
        if flt == "filter games…":
            flt = ""
        visible = [g for g in self.games if flt in g.name.lower()]
        for i, g in enumerate(visible):
            self._make_card(g, i // cols, i % cols)

    def _make_card(self, game, row, col):
        owner_sid = accounts.account_for_game(game.appid, self.accounts)
        owner = self._account_name(owner_sid) if owner_sid else None

        card = tk.Frame(self.grid_frame, bg=CARD, width=CARD_W, height=CARD_H,
                        highlightthickness=1, highlightbackground="#000",
                        cursor="hand2")
        card.grid(row=row, column=col, padx=7, pady=7)
        card.grid_propagate(False)

        # cover / fallback fills the card
        body = tk.Label(card, bg=CARD, fg=TEXT, text=game.name, wraplength=CARD_W - 16,
                        font=("Segoe UI", 10), justify="center")
        body.place(relx=0, rely=0, relwidth=1, relheight=1)

        # account badge
        badge = tk.Label(card, text=owner or "unmapped", bg="#000" if owner else BAD,
                         fg="#fff", font=("Segoe UI", 7), padx=4)
        badge.place(x=4, y=4)

        for w in (card, body, badge):
            w.bind("<Button-1>", lambda _e, gm=game: self.play(gm))

        self.cards.append(card)
        if HAVE_PIL:
            self.pool.submit(self._load_cover, game, body)

    def _load_cover(self, game, label):
        try:
            data = None
            lc = local_cover(game.appid)
            if lc:
                data = lc.read_bytes()
            else:
                for url in (game.cover_url(), game.header_url()):
                    try:
                        with urllib.request.urlopen(url, timeout=10) as r:
                            data = r.read()
                        break
                    except Exception:
                        continue
            if not data:
                return
            img = Image.open(__import__("io").BytesIO(data)).convert("RGB")
            img = img.resize((CARD_W, CARD_H))
            photo = ImageTk.PhotoImage(img)
        except Exception:
            return

        def apply():
            if label.winfo_exists():
                self.images[game.appid] = photo
                label.config(image=photo, text="")
        self.after(0, apply)

    # --------------------------------------------------------------- play
    def play(self, game):
        if self._launching:
            # A switch is already underway; ignore extra clicks so we never run
            # two Steam restarts at once (that's what floods cmd windows / lags).
            self.set_status("A launch is already in progress — please wait…", "bad")
            return
        owner = accounts.account_for_game(game.appid, self.accounts)
        if not owner:
            messagebox.showinfo(
                "Not mapped",
                f'"{game.name}" isn\'t mapped to an account yet.\n\n'
                "Its appmanifest records no local owner — this can happen with some "
                "family-shared installs. There's no in-app way to map it manually "
                "yet, so this game can't be launched through SteamSwitch for now.")
            return
        offline = self.offline_var.get()
        self._launching = True
        self._cancel.clear()
        self._set_cancel_enabled(True)
        self.set_status(f'Starting "{game.name}"…')

        def notify(msg):
            self.after(0, lambda: self.set_status(msg))

        def work():
            try:
                res = launcher.play(game.appid, offline=offline, notify=notify,
                                    should_cancel=self._cancel.is_set)
            except Exception as exc:  # never leave the guard stuck on a crash
                res = launcher.PlayResult(False, f"Launch failed: {exc}")
            def done():
                self._launching = False
                self._set_cancel_enabled(False)
                self.set_status(res.message, "good" if res.ok else "bad")
                if res.needs_login:
                    messagebox.showwarning("Finish login in Steam", res.message)
                if res.switched:
                    self.after(4000, self.reload_accounts)
            self.after(0, done)
        threading.Thread(target=work, daemon=True).start()

    def cancel_launch(self):
        """Abort the launch in progress (wrong game picked, etc.)."""
        if self._launching:
            self._cancel.set()
            self.set_status("Cancelling launch…")
            self._set_cancel_enabled(False)

    # ----------------------------------------------------------- accounts
    def open_accounts(self):
        AccountsWindow(self)

    def _on_close(self):
        self.pool.shutdown(wait=False, cancel_futures=True)
        self.destroy()


class AccountsWindow(tk.Toplevel):
    def __init__(self, app: App):
        super().__init__(app)
        self.app = app
        self.title("Accounts")
        self.configure(bg=PANEL)
        self.geometry("440x560")
        self.transient(app)

        tk.Label(self, text="Accounts", bg=PANEL, fg="#fff",
                 font=("Segoe UI", 12, "bold")).pack(anchor="w", padx=16, pady=(14, 4))
        tk.Label(self, bg=PANEL, fg="#9fb3c8", justify="left", wraplength=400,
                 text="Installed games are mapped to their owning account "
                      "automatically from Steam's install data — no setup needed. "
                      "Log into each account in Steam once with “Remember me” "
                      "checked so the launcher can switch to it silently."
                 ).pack(anchor="w", padx=16)

        body = tk.Frame(self, bg=PANEL)
        body.pack(fill="both", expand=True, padx=12, pady=10)

        if not app.accounts:
            tk.Label(body, bg=PANEL, fg=BAD, wraplength=400, justify="left",
                     text="No accounts found in loginusers.vdf. Log into each "
                          "account in Steam once with “Remember me” checked."
                     ).pack(anchor="w")
            return

        # how many installed games currently map to each account
        counts: dict[str, int] = {}
        for g in app.games:
            sid = accounts.account_for_game(g.appid, app.accounts)
            if sid:
                counts[sid] = counts.get(sid, 0) + 1

        for acc in app.accounts:
            self._account_row(body, acc, counts.get(acc.steamid64, 0))

    def _account_row(self, parent, acc, game_count):
        box = tk.Frame(parent, bg=BG, highlightthickness=1, highlightbackground="#000")
        box.pack(fill="x", pady=6)
        tk.Label(box, text=acc.persona_name, bg=BG, fg=ACCENT,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=10, pady=(8, 0))
        tk.Label(box, text=f"{acc.account_name} · {acc.steamid64}", bg=BG,
                 fg=MUTED, font=("Segoe UI", 8)).pack(anchor="w", padx=10)
        tk.Label(box, text=f"{game_count} installed game"
                           f"{'' if game_count == 1 else 's'} mapped here",
                 bg=BG, fg=TEXT, font=("Segoe UI", 8)).pack(anchor="w", padx=10,
                                                            pady=(2, 8))


def main():
    App().mainloop()


if __name__ == "__main__":
    main()
