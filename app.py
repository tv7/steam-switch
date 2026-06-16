"""SteamSwitch — Steam multi-account launcher, native desktop GUI (Tkinter).

Run:  python app.py        (Tkinter ships with Python — nothing to install)

Or build a single portable .exe with no Python required:  python build.py

Cover art shows up when the Pillow library is available (it's bundled into the
.exe build). Without Pillow the app still works fully, showing clean text tiles
instead of images.
"""

from __future__ import annotations

import io
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import tkinter as tk
from tkinter import ttk, messagebox

from core import games, accounts, switcher, launcher, steam_paths, covers

try:
    from PIL import Image, ImageTk, ImageDraw, ImageFont
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
WARN = "#d9a441"            # amber: "needs attention" (account needs re-login)
MUTED = "#8aa0b6"           # secondary text / placeholder, legible on dark panels
DIM = "#33414f"            # inactive control fill (disabled button / toggle OFF)

CARD_W, CARD_H = 150, 225

# High-quality downscale filter, across Pillow versions (Resampling added in 9.1).
if HAVE_PIL:
    _RESAMPLE = getattr(getattr(Image, "Resampling", Image), "LANCZOS", None) \
        or getattr(Image, "BICUBIC", None)


def _asset(name: str) -> Path:
    """Path to a bundled asset, working from source and from a PyInstaller .exe
    (which extracts bundled data under sys._MEIPASS)."""
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return base / "assets" / name


# A fixed palette of distinct, vivid colors, legible on the dark gradient. Accounts
# are assigned colors by their position in the list (see App.reload_accounts), which
# guarantees they look different — a SteamID hash-to-hue clustered into lookalikes
# because real SteamIDs differ only in their last digits.
_ACCOUNT_PALETTE = [
    (88, 166, 255),   # blue
    (87, 204, 153),   # green
    (255, 138, 101),  # orange
    (199, 146, 234),  # purple
    (255, 99, 132),   # pink
    (255, 209, 102),  # yellow
    (38, 198, 218),   # cyan
    (240, 113, 178),  # magenta
    (124, 179, 66),   # lime
    (149, 117, 205),  # violet
]
_UNMAPPED_COLOR = (201, 79, 79)   # BAD red


def _hex(rgb: tuple) -> str:
    return "#%02x%02x%02x" % rgb


# TrueType fonts give crisp anti-aliased text; try the platform's, fall back to
# Pillow's bitmap default. Cached per size.
_FONT_CACHE: dict = {}
_FONT_PATHS = (
    r"C:\Windows\Fonts\seguisb.ttf",   # Segoe UI Semibold (matches the app's font)
    r"C:\Windows\Fonts\segoeuib.ttf",  # Segoe UI Bold
    r"C:\Windows\Fonts\arialbd.ttf",   # Arial Bold
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
)


def _cover_font(size: int):
    if size in _FONT_CACHE:
        return _FONT_CACHE[size]
    font = None
    for p in _FONT_PATHS:
        try:
            font = ImageFont.truetype(p, size)
            break
        except Exception:
            continue
    if font is None:
        try:
            font = ImageFont.load_default()
        except Exception:
            font = None
    _FONT_CACHE[size] = font
    return font


def _truncate(draw, text: str, font, max_w: int) -> str:
    """Trim `text` (adding an ellipsis) until it fits in `max_w` pixels."""
    if draw.textlength(text, font=font) <= max_w:
        return text
    while text and draw.textlength(text + "…", font=font) > max_w:
        text = text[:-1]
    return (text + "…") if text else ""


def _draw_account_badge(img, name: str, rgb: tuple):
    """Composite a soft bottom gradient + account-color dot + name onto a portrait
    cover (PIL RGB). Returns a new RGB image; readable over any artwork."""
    w, h = img.size
    base = img.convert("RGBA")

    # Gradient: transparent at the top edge -> near-opaque dark at the bottom. Build
    # it as a 1px-wide column and stretch (cheap), easing so the fade looks natural.
    band = max(40, int(h * 0.40))
    col = Image.new("RGBA", (1, band))
    cpx = col.load()
    for y in range(band):
        cpx[0, y] = (8, 12, 18, int(220 * (y / (band - 1)) ** 1.5))
    base.alpha_composite(col.resize((w, band)), (0, h - band))

    draw = ImageDraw.Draw(base)
    font = _cover_font(13)
    pad = 8
    bx = font.getbbox("Ag") if font else (0, 0, 0, 11)
    text_h = bx[3] - bx[1]
    text_y = h - pad - text_h
    dot = 7
    draw.ellipse([pad, text_y + (text_h - dot) // 2, pad + dot,
                  text_y + (text_h - dot) // 2 + dot], fill=rgb + (255,))
    tx = pad + dot + 6
    name = _truncate(draw, name, font, w - tx - pad)
    draw.text((tx + 1, text_y - bx[1] + 1), name, font=font, fill=(0, 0, 0, 200))
    draw.text((tx, text_y - bx[1]), name, font=font, fill=(255, 255, 255, 255))
    return base.convert("RGB")


def _fit_portrait(img, w: int, h: int):
    """Scale + center-crop `img` to exactly w x h (like CSS background 'cover').

    Portrait art (library_600x900) fills the card perfectly; landscape art (header /
    capsule, the only art some games have) fills it via a center crop instead of
    being stretched out of shape."""
    iw, ih = img.size
    scale = max(w / iw, h / ih)
    nw, nh = max(1, round(iw * scale)), max(1, round(ih * scale))
    img = img.resize((nw, nh), _RESAMPLE) if _RESAMPLE else img.resize((nw, nh))
    left, top = (nw - w) // 2, (nh - h) // 2
    return img.crop((left, top, left + w, top + h))


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
        self.account_colors: dict = {}   # steamid64 -> (r,g,b), assigned by position
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

        self.accounts_btn = tk.Button(bar, text="Accounts", command=self.open_accounts,
                                      bg=ACCENT2, fg="#fff", activebackground=ACCENT2_HOVER,
                                      activeforeground="#fff", relief="flat", padx=10)
        self.accounts_btn.pack(side="right", padx=12)

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

        # search (+ filter dropdown to its right)
        sb = tk.Frame(self, bg=BG)
        sb.pack(fill="x", padx=16, pady=(12, 4))

        # Filter ▾ : sort + filter-by-account. Search is unchanged; this only adds
        # ordering/filtering on top of the search results (see render_grid).
        self.sort_var = tk.StringVar(value="az")
        self.filter_var = tk.StringVar(value="all")
        fb = tk.Menubutton(sb, text="Filter ▾", bg=ACCENT2, fg="#fff",
                           activebackground=ACCENT2_HOVER, activeforeground="#fff",
                           relief="flat", padx=12)
        self.filter_menu = tk.Menu(fb, tearoff=0, bg=PANEL, fg=TEXT,
                                   activebackground=ACCENT2, activeforeground="#fff",
                                   selectcolor=ACCENT, bd=0)
        fb.config(menu=self.filter_menu)
        fb.pack(side="right", padx=(8, 0))
        self._build_filter_menu()

        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", lambda *_: self.render_grid())
        e = tk.Entry(sb, textvariable=self.search_var, bg=PANEL, fg=TEXT,
                     insertbackground=TEXT, relief="flat")
        e.pack(side="left", fill="x", expand=True, ipady=5)
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
        # Assign each account a distinct color by its position in the list.
        self.account_colors = {
            a.steamid64: _ACCOUNT_PALETTE[i % len(_ACCOUNT_PALETTE)]
            for i, a in enumerate(self.accounts)
        }
        self.current_account = switcher.current_account_name()
        # Keep the Filter menu's account list in sync; reset a now-missing selection.
        if hasattr(self, "filter_var"):
            if (self.filter_var.get() not in ("all", "unmapped")
                    and self.filter_var.get() not in {a.steamid64 for a in self.accounts}):
                self.filter_var.set("all")
            self._build_filter_menu()
        self.account_lbl.config(
            text=f"Logged in: {self.current_account}" if self.current_account
            else "No active account")
        if hasattr(self, "accounts_btn"):
            self.accounts_btn.config(
                text=f"Accounts ({len(self.accounts)})" if self.accounts else "Accounts")

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

    def _color_for(self, sid):
        """Distinct (r,g,b) for an account by its palette assignment; unmapped → red."""
        return self.account_colors.get(sid, _UNMAPPED_COLOR) if sid else _UNMAPPED_COLOR

    def _build_filter_menu(self):
        """(Re)build the Filter ▾ menu: sort options + one entry per account. Called
        once at startup and again whenever the account list changes."""
        m = self.filter_menu
        m.delete(0, "end")
        m.add_command(label="Sort", state="disabled")
        m.add_radiobutton(label="Name: A → Z", variable=self.sort_var, value="az",
                          command=self.render_grid)
        m.add_radiobutton(label="Name: Z → A", variable=self.sort_var, value="za",
                          command=self.render_grid)
        m.add_separator()
        m.add_command(label="Show", state="disabled")
        m.add_radiobutton(label="All accounts", variable=self.filter_var, value="all",
                          command=self.render_grid)
        for a in self.accounts:
            m.add_radiobutton(label=a.persona_name, variable=self.filter_var,
                              value=a.steamid64, command=self.render_grid)
        m.add_radiobutton(label="Unmapped", variable=self.filter_var, value="unmapped",
                          command=self.render_grid)

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

        # Filter by account (on top of the search results), then sort.
        acct = self.filter_var.get() if hasattr(self, "filter_var") else "all"
        if acct == "unmapped":
            visible = [g for g in visible
                       if not accounts.account_for_game(g.appid, self.accounts)]
        elif acct != "all":
            visible = [g for g in visible
                       if accounts.account_for_game(g.appid, self.accounts) == acct]
        reverse = hasattr(self, "sort_var") and self.sort_var.get() == "za"
        visible.sort(key=lambda g: g.name.lower(), reverse=reverse)

        for i, g in enumerate(visible):
            self._make_card(g, i // cols, i % cols)

    def _make_card(self, game, row, col):
        owner_sid = accounts.account_for_game(game.appid, self.accounts)
        owner = self._account_name(owner_sid) if owner_sid else None
        label_text = owner or "unmapped"
        rgb = self._color_for(owner_sid)

        card = tk.Frame(self.grid_frame, bg=CARD, width=CARD_W, height=CARD_H,
                        highlightthickness=1, highlightbackground="#000",
                        cursor="hand2")
        card.grid(row=row, column=col, padx=7, pady=7)
        card.grid_propagate(False)

        # cover / fallback fills the card
        body = tk.Label(card, bg=CARD, fg=TEXT, text=game.name, wraplength=CARD_W - 16,
                        font=("Segoe UI", 10), justify="center")
        body.place(relx=0, rely=0, relwidth=1, relheight=1)

        # Account label as a bottom bar. This is the fallback for the text tile (no
        # gradient/alpha in Tk); when a cover image loads, the name is baked into the
        # image (gradient + dot) and this bar is hidden. Account-colored text doubles
        # as the color code.
        bar = tk.Label(card, text=f"  {label_text}", anchor="w", bg="#0b0f14",
                       fg=_hex(rgb), font=("Segoe UI", 9, "bold"))
        bar.place(relx=0, rely=1.0, anchor="sw", relwidth=1)

        for w in (card, body, bar):
            w.bind("<Button-1>", lambda _e, gm=game: self.play(gm))

        self.cards.append(card)
        if HAVE_PIL:
            self.pool.submit(self._load_cover, game, body, label_text, rgb, bar)

    def _load_cover(self, game, label, name, rgb, bar):
        try:
            data = covers.cover_bytes(game.appid)
            if not data:
                return
            img = Image.open(io.BytesIO(data)).convert("RGB")
            img = _fit_portrait(img, CARD_W, CARD_H)
            img = _draw_account_badge(img, name, rgb)
            photo = ImageTk.PhotoImage(img)
        except Exception:
            return

        def apply():
            if label.winfo_exists():
                self.images[game.appid] = photo
                label.config(image=photo, text="")
                if bar.winfo_exists():
                    bar.place_forget()   # name is now baked into the cover image
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

    def add_account(self, window=None):
        """Restart Steam to its login screen so the user can add another account.
        Steam must restart for the chooser ('+ Add an account') to appear, so we
        confirm first, then do it off the UI thread (shutdown polls for a while)."""
        if self._launching:
            self.set_status("A launch is in progress — please wait…", "bad")
            return
        if not messagebox.askyesno(
                "Add an account",
                "This closes Steam and opens its login screen so you can add "
                "another account.\n\nLog in with “Remember me” checked so "
                "SteamSwitch can switch to it later.\n\nContinue?"):
            return
        if window is not None and window.winfo_exists():
            window.destroy()
        self.set_status("Closing Steam and opening the login screen…")
        self.pool.submit(self._add_account_worker)

    def _add_account_worker(self):
        try:
            ok = switcher.restart_to_add_account()
        except Exception as e:
            self.after(0, lambda e=e: self.set_status(f"Couldn't open Steam: {e}", "bad"))
            return

        def done():
            if ok:
                self.set_status(
                    "Steam is opening its login screen. Sign in to the account you "
                    "want to add with “Remember me” checked, then reopen Accounts.",
                    "good")
                # Give the user time to log in, then refresh so the new account shows.
                self.after(45000, self.reload_accounts)
            else:
                self.set_status(
                    "Couldn't close Steam — close it manually and try again.", "bad")
        self.after(0, done)

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
                 text="SteamSwitch can switch to any account you've logged into "
                      "Steam with “Remember me”. Add another below; it logs in "
                      "through Steam, then appears here ready to use."
                 ).pack(anchor="w", padx=16)

        # Footer (always visible) with the Add-account action.
        footer = tk.Frame(self, bg=PANEL)
        footer.pack(side="bottom", fill="x", padx=12, pady=(0, 12))
        tk.Button(footer, text="+ Add an account",
                  command=lambda: app.add_account(self),
                  bg=ACCENT2, fg="#fff", activebackground=ACCENT2_HOVER,
                  activeforeground="#fff", relief="flat", padx=12, pady=6
                  ).pack(fill="x")

        body = tk.Frame(self, bg=PANEL)
        body.pack(fill="both", expand=True, padx=12, pady=10)

        if not app.accounts:
            tk.Label(body, bg=PANEL, fg=MUTED, wraplength=400, justify="left",
                     text="No accounts found yet. Click “+ Add an account” below "
                          "and log in with “Remember me” checked."
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

        # Header line: persona name (left) + switch-readiness badge (right).
        top = tk.Frame(box, bg=BG)
        top.pack(fill="x", padx=10, pady=(8, 0))
        tk.Label(top, text=acc.persona_name, bg=BG, fg=ACCENT,
                 font=("Segoe UI", 10, "bold")).pack(side="left")
        ready, _why = switcher.can_autologin(acc.account_name)
        tk.Label(top, text="✓ ready" if ready else "⚠ needs login",
                 bg=BG, fg=GOOD if ready else WARN,
                 font=("Segoe UI", 8, "bold")).pack(side="right")

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
