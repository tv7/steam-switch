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

# Proton-Dark palette (see ui_refresh/proton_dark/DESIGN.md). Tonal layering goes
# BASE (darkest, inputs) < BG (canvas) < PANEL (sidebar/footer) < CARD < CARD_HOVER;
# 1px OUTLINE borders define sections instead of shadows. Text/status colors are
# chosen to stay readable on those backgrounds.
BG = "#0b1326"            # background — the deep ink canvas
PANEL = "#131b2e"         # surface-container-low — sidebar, topbar chrome, footer
CARD = "#171f33"          # surface-container — cards / tiles
CARD_HOVER = "#222a3d"    # surface-container-high — hover / active nav
BASE = "#060e20"          # surface-container-lowest — input fields
ACCENT = "#adc6ff"        # primary — accent text, active nav, focus
ACCENT2 = "#4d8eff"       # primary-container — solid buttons / account chip
ACCENT2_HOVER = "#6ba0ff"
TEXT = "#dae2fd"          # on-surface — primary text
MUTED = "#c2c6d6"         # on-surface-variant — secondary text / placeholder
OUTLINE = "#424754"       # outline-variant — 1px borders
DIM = "#2d3449"           # inactive control fill (toggle OFF / disabled button)
BAD = "#ef4444"           # error
BAD_HOVER = "#dc2626"
GOOD = "#10b981"          # success / online / installed
WARN = "#d9a441"          # amber: account needs re-login

APP_VERSION = "proton-dark"

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


def _pill(cv, x0, y0, x1, y1, fill):
    """Draw a filled rounded pill (two circle caps + a rectangle) on a Canvas."""
    r = (y1 - y0) / 2
    cv.create_oval(x0, y0, x0 + 2 * r, y1, fill=fill, outline="")
    cv.create_oval(x1 - 2 * r, y0, x1, y1, fill=fill, outline="")
    cv.create_rectangle(x0 + r, y0, x1 - r, y0 + 2 * r, fill=fill, outline="")


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
        self.geometry("1100x720")
        self.minsize(820, 520)
        self._set_app_icon()

        self.games: list = []
        self.accounts: list = []
        self.account_colors: dict = {}   # steamid64 -> (r,g,b), assigned by position
        self.current_account: str | None = None
        self.images: dict[int, object] = {}   # keep PhotoImage refs alive
        self.cards: list[tk.Widget] = []
        self._cols = 0
        self._view = "library"     # which content pane is showing
        self._nav: dict = {}       # key -> (row, accent-bar, label) for sidebar nav
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
        """Top-level layout: a full-height sidebar on the left, and a right column
        holding the topbar (search/controls), the swappable content pane, and the
        footer status bar."""
        main = tk.Frame(self, bg=BG)
        main.pack(fill="both", expand=True)

        self._build_sidebar(main)

        right = tk.Frame(main, bg=BG)
        right.pack(side="left", fill="both", expand=True)

        self._build_topbar(right)
        self._build_footer(right)        # side=bottom, before content claims the middle

        self.content = tk.Frame(right, bg=BG)
        self.content.pack(fill="both", expand=True)

        if not HAVE_PIL:
            self.set_status("Tip: covers appear when Pillow is installed "
                            "(it's bundled in the .exe build).")
        self.show_view("library")

    # ----- sidebar -----------------------------------------------------
    def _build_sidebar(self, parent):
        sb = tk.Frame(parent, bg=PANEL, width=210)
        sb.pack(side="left", fill="y")
        sb.pack_propagate(False)

        # Brand: the SteamSwitch mark + wordmark. Falls back to text alone if the
        # logo asset isn't present (e.g. assets/make_icon.py hasn't been run).
        self._logo_img = None
        try:
            if _asset("steamswitch_28.png").exists():
                self._logo_img = tk.PhotoImage(file=str(_asset("steamswitch_28.png")))
        except Exception:
            self._logo_img = None
        brand = tk.Frame(sb, bg=PANEL)
        brand.pack(fill="x", padx=16, pady=(18, 22))
        tk.Label(brand, text="  SteamSwitch", image=self._logo_img, compound="left",
                 bg=PANEL, fg="#ffffff", font=("Segoe UI", 13, "bold")).pack(side="left")

        # Primary navigation. "Accounts" isn't a nav item — it's reached via the
        # account chip pinned at the bottom of the sidebar.
        self._add_nav(sb, "library", "▦   Library")
        self._add_nav(sb, "settings", "⚙   Settings")

        self._build_account_chip(sb)

    def _add_nav(self, parent, key, text):
        row = tk.Frame(parent, bg=PANEL, cursor="hand2")
        row.pack(fill="x", padx=10, pady=2)
        bar = tk.Frame(row, bg=PANEL, width=3)         # left accent bar when active
        bar.pack(side="left", fill="y")
        lbl = tk.Label(row, text=text, bg=PANEL, fg=MUTED, font=("Segoe UI", 11),
                       anchor="w", padx=12, pady=9)
        lbl.pack(side="left", fill="x", expand=True)
        self._nav[key] = (row, bar, lbl)
        for w in (row, lbl):
            w.bind("<Button-1>", lambda _e, k=key: self.show_view(k))
            w.bind("<Enter>", lambda _e, k=key: self._nav_hover(k, True))
            w.bind("<Leave>", lambda _e, k=key: self._nav_hover(k, False))

    def _nav_hover(self, key, entering):
        if key == self._view:
            return
        row, _bar, lbl = self._nav[key]
        bg = CARD_HOVER if entering else PANEL
        row.config(bg=bg)
        lbl.config(bg=bg, fg=TEXT if entering else MUTED)

    def _update_nav(self):
        for key, (row, bar, lbl) in self._nav.items():
            active = (key == self._view)
            row.config(bg=CARD_HOVER if active else PANEL)
            bar.config(bg=ACCENT if active else PANEL)
            lbl.config(bg=CARD_HOVER if active else PANEL,
                       fg=TEXT if active else MUTED)

    def _build_account_chip(self, parent):
        """The current-account block pinned bottom-left; clicking it opens Accounts."""
        chip = tk.Frame(parent, bg=ACCENT2, cursor="hand2")
        chip.pack(side="bottom", fill="x", padx=12, pady=12)
        self.chip_avatar = tk.Label(chip, text="", bg="#2a3a6a", fg="#ffffff",
                                    width=4, height=2, font=("Segoe UI", 10, "bold"))
        self.chip_avatar.pack(side="left", padx=8, pady=8)
        txt = tk.Frame(chip, bg=ACCENT2)
        txt.pack(side="left", fill="x", expand=True)
        self.chip_count = tk.Label(txt, text="ACCOUNTS", bg=ACCENT2, fg="#d8e2ff",
                                   font=("Segoe UI", 7, "bold"), anchor="w")
        self.chip_count.pack(anchor="w")
        self.chip_name = tk.Label(txt, text="…", bg=ACCENT2, fg="#ffffff",
                                  font=("Segoe UI", 10, "bold"), anchor="w")
        self.chip_name.pack(anchor="w")
        for w in (chip, txt, self.chip_avatar, self.chip_count, self.chip_name):
            w.bind("<Button-1>", lambda _e: self.show_view("accounts"))

    # ----- topbar (search + controls) ---------------------------------
    def _build_topbar(self, parent):
        bar = tk.Frame(parent, bg=BG)
        bar.pack(fill="x", padx=24, pady=(16, 6))

        # Search field: a BASE-filled box with a 1px outline and a leading glyph.
        sframe = tk.Frame(bar, bg=BASE, highlightthickness=1,
                          highlightbackground=OUTLINE, highlightcolor=ACCENT)
        sframe.pack(side="left")
        tk.Label(sframe, text="🔍", bg=BASE, fg=MUTED).pack(side="left", padx=(8, 0))
        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", lambda *_: self._on_search())
        e = tk.Entry(sframe, textvariable=self.search_var, bg=BASE, fg=TEXT,
                     insertbackground=TEXT, relief="flat", width=38)
        e.pack(side="left", ipady=6, padx=(4, 8))
        self._placeholder(e, "Search games in library…")

        self._make_tool_btn(bar, "⟳  Refresh", self.refresh_all).pack(side="left", padx=(10, 0))

        # Filter ▾ : sort + filter-by-account. Search is unchanged; this only adds
        # ordering/filtering on top of the search results (see render_grid).
        self.sort_var = tk.StringVar(value="az")
        self.filter_var = tk.StringVar(value="all")
        fb = tk.Menubutton(bar, text="≡  Filter", bg=CARD, fg=TEXT,
                           activebackground=CARD_HOVER, activeforeground=TEXT,
                           relief="flat", padx=12, pady=4,
                           highlightthickness=1, highlightbackground=OUTLINE)
        self.filter_menu = tk.Menu(fb, tearoff=0, bg=PANEL, fg=TEXT,
                                   activebackground=ACCENT2, activeforeground="#fff",
                                   selectcolor=ACCENT, bd=0)
        fb.config(menu=self.filter_menu)
        fb.pack(side="left", padx=(10, 0))
        self._build_filter_menu()

        # Stop launch (right): abort a launch that's underway (e.g. wrong game).
        # Starts dim/disabled; _set_cancel_enabled() turns it red + clickable.
        self.cancel_btn = tk.Button(bar, text="⊘  Stop launch", command=self.cancel_launch,
                                    bg=DIM, fg="#fff", activebackground=BAD_HOVER,
                                    activeforeground="#fff", disabledforeground=MUTED,
                                    relief="flat", padx=12, pady=4, state="disabled")
        self.cancel_btn.pack(side="right")

        # Launch-offline pill toggle (right of-centre). Keeps offline_var so play()
        # is unchanged.
        self.offline_var = tk.BooleanVar(value=False)
        self._make_toggle(bar, self.offline_var, "LAUNCH OFFLINE").pack(side="right", padx=(0, 14))

    def _make_tool_btn(self, parent, text, cmd):
        """Secondary button: CARD fill, 1px outline, on-surface text."""
        return tk.Button(parent, text=text, command=cmd, bg=CARD, fg=TEXT,
                         activebackground=CARD_HOVER, activeforeground=TEXT,
                         relief="flat", padx=12, pady=4,
                         highlightthickness=1, highlightbackground=OUTLINE)

    def _make_toggle(self, parent, var, label):
        """A small Canvas-drawn pill switch bound to a BooleanVar (Tk has no native
        toggle). Blue + knob-right when on, dim + knob-left when off."""
        f = tk.Frame(parent, bg=BG)
        tk.Label(f, text=label, bg=BG, fg=MUTED,
                 font=("Segoe UI", 9, "bold")).pack(side="left", padx=(0, 8))
        cv = tk.Canvas(f, width=42, height=20, bg=BG, highlightthickness=0, cursor="hand2")
        cv.pack(side="left")

        def draw():
            cv.delete("all")
            on = var.get()
            _pill(cv, 1, 1, 41, 19, ACCENT2 if on else DIM)
            kx = 31 if on else 11
            cv.create_oval(kx - 7, 3, kx + 7, 17, fill="#ffffff", outline="")

        def toggle(_=None):
            var.set(not var.get())
            draw()

        cv.bind("<Button-1>", toggle)
        draw()
        return f

    # ----- footer ------------------------------------------------------
    def _build_footer(self, parent):
        footer = tk.Frame(parent, bg=PANEL)
        footer.pack(side="bottom", fill="x")
        self.status_dot = tk.Label(footer, text="●", bg=PANEL, fg=GOOD,
                                   font=("Segoe UI", 8))
        self.status_dot.pack(side="left", padx=(24, 6), pady=5)
        self.status = tk.Label(footer, text="", bg=PANEL, fg=TEXT, anchor="w")
        self.status.pack(side="left")
        tk.Label(footer, text=APP_VERSION, bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 8, "italic")).pack(side="right", padx=24)

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
            c = getattr(self, "canvas", None)
            if not c or not c.winfo_exists():
                return
            delta = -1 * (e.delta // 120) if e.delta else (1 if e.num == 5 else -1)
            c.yview_scroll(delta, "units")
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
        if hasattr(self, "status_dot"):
            self.status_dot.config(fg={"bad": BAD}.get(kind, GOOD))

    def _on_search(self):
        if self._view == "library":
            self.render_grid()

    def _set_cancel_enabled(self, on: bool):
        """Stop-launch button: red + clickable while a launch runs, dim otherwise
        (a permanently-red disabled button looks broken and its text goes dark)."""
        self.cancel_btn.config(state="normal" if on else "disabled",
                               bg=BAD if on else DIM)

    # ----- view switching ---------------------------------------------
    def show_view(self, key):
        self._view = key
        self._update_nav()
        for w in self.content.winfo_children():
            w.destroy()
        self.cards.clear()
        if key == "library":
            self._build_library()
        elif key == "accounts":
            self._build_accounts()
        elif key == "settings":
            self._build_settings()

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
        self._update_account_chip()
        # If the Accounts view is showing, rebuild it so badges/counts stay current.
        if self._view == "accounts":
            self.show_view("accounts")

    def _update_account_chip(self):
        if not hasattr(self, "chip_name"):
            return
        name = self.current_account or "No account"
        self.chip_count.config(
            text=f"ACCOUNTS ({len(self.accounts)})" if self.accounts else "ACCOUNTS")
        self.chip_name.config(text=name)
        sid = next((a.steamid64 for a in self.accounts
                    if a.account_name == self.current_account
                    or a.persona_name == self.current_account), None)
        self.chip_avatar.config(text=(name[:2].upper() if name else ""),
                                bg=_hex(self._color_for(sid)) if sid else "#2a3a6a")

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

    # ----------------------------------------------------------- library
    def _build_library(self):
        head = tk.Frame(self.content, bg=BG)
        head.pack(fill="x", padx=24, pady=(6, 8))
        tk.Label(head, text="MY GAMES", bg=BG, fg=MUTED,
                 font=("Segoe UI", 10, "bold")).pack(side="left")
        self.count_lbl = tk.Label(head, text="", bg=BG, fg=MUTED,
                                  font=("Segoe UI", 9))
        self.count_lbl.pack(side="right")

        wrap = tk.Frame(self.content, bg=BG)
        wrap.pack(fill="both", expand=True, padx=16)
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
        self._cols = 0
        self.render_grid()

    # ------------------------------------------------------------- render
    def render_grid(self):
        # Only the library view owns a grid; ignore stray calls from other views.
        if self._view != "library" or not getattr(self, "grid_frame", None) \
                or not self.grid_frame.winfo_exists():
            return
        for c in self.cards:
            c.destroy()
        self.cards.clear()
        cols = max(1, self._cols or 5)
        flt = self.search_var.get().strip().lower()
        if flt == "search games in library…":
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

        if hasattr(self, "count_lbl") and self.count_lbl.winfo_exists():
            self.count_lbl.config(
                text=f"Showing {len(visible)} of {len(self.games)} games")

        for i, g in enumerate(visible):
            self._make_card(g, i // cols, i % cols)

    def _make_card(self, game, row, col):
        owner_sid = accounts.account_for_game(game.appid, self.accounts)
        owner = self._account_name(owner_sid) if owner_sid else None
        label_text = owner or "unmapped"
        rgb = self._color_for(owner_sid)

        card = tk.Frame(self.grid_frame, bg=CARD, width=CARD_W, height=CARD_H,
                        highlightthickness=1, highlightbackground=OUTLINE,
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
    def _build_accounts(self):
        head = tk.Frame(self.content, bg=BG)
        head.pack(fill="x", padx=24, pady=(6, 2))
        tk.Label(head, text="MANAGE ACCOUNTS", bg=BG, fg=MUTED,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w")
        tk.Label(self.content, bg=BG, fg=MUTED, justify="left", anchor="w",
                 text="Your saved Steam accounts. Pick a game in Library to switch to "
                      "the account that owns it; add another below.",
                 font=("Segoe UI", 9)).pack(fill="x", padx=24, pady=(0, 8))

        grid = tk.Frame(self.content, bg=BG)
        grid.pack(fill="both", expand=True, padx=16)
        grid.columnconfigure(0, weight=1, uniform="acc")
        grid.columnconfigure(1, weight=1, uniform="acc")

        # how many installed games currently map to each account
        counts: dict[str, int] = {}
        for g in self.games:
            sid = accounts.account_for_game(g.appid, self.accounts)
            if sid:
                counts[sid] = counts.get(sid, 0) + 1

        # Cell 0 is the "add account" card; the rest are the accounts.
        self._add_account_card(grid, 0, 0)
        for idx, acc in enumerate(self.accounts, start=1):
            self._account_card(grid, acc, counts.get(acc.steamid64, 0),
                               idx // 2, idx % 2)

        if not self.accounts:
            tk.Label(grid, bg=BG, fg=MUTED, wraplength=380, justify="left",
                     text="No accounts found yet. Click “+ Add New Account” and log "
                          "in with “Remember me” checked.").grid(row=0, column=1,
                                                                 sticky="w", padx=10)

    def _add_account_card(self, parent, r, c):
        card = tk.Frame(parent, bg=BG, highlightthickness=1,
                        highlightbackground=OUTLINE, cursor="hand2", height=150)
        card.grid(row=r, column=c, sticky="nsew", padx=8, pady=8)
        card.grid_propagate(False)
        inner = tk.Frame(card, bg=BG)
        inner.place(relx=0.5, rely=0.5, anchor="center")
        tk.Label(inner, text="＋", bg=BG, fg=ACCENT,
                 font=("Segoe UI", 20, "bold")).pack()
        tk.Label(inner, text="Add New Account", bg=BG, fg=TEXT,
                 font=("Segoe UI", 11, "bold")).pack()
        tk.Label(inner, text="Steam Account", bg=BG, fg=MUTED,
                 font=("Segoe UI", 8)).pack()
        for w in (card, inner, *inner.winfo_children()):
            w.bind("<Button-1>", lambda _e: self.add_account())

    def _account_card(self, parent, acc, game_count, r, c):
        is_current = acc.account_name == self.current_account \
            or acc.persona_name == self.current_account
        border = ACCENT if is_current else OUTLINE
        card = tk.Frame(parent, bg=CARD, highlightthickness=1,
                        highlightbackground=border, height=150)
        card.grid(row=r, column=c, sticky="nsew", padx=8, pady=8)
        card.grid_propagate(False)

        top = tk.Frame(card, bg=CARD)
        top.pack(fill="x", padx=12, pady=(12, 4))

        rgb = self._color_for(acc.steamid64)
        tk.Label(top, text=acc.persona_name[:2].upper(), bg=_hex(rgb), fg="#0b1326",
                 width=4, height=2, font=("Segoe UI", 11, "bold")).pack(side="left")

        info = tk.Frame(top, bg=CARD)
        info.pack(side="left", fill="x", expand=True, padx=10)
        tk.Label(info, text=acc.persona_name, bg=CARD, fg="#ffffff",
                 font=("Segoe UI", 11, "bold"), anchor="w").pack(anchor="w")
        tk.Label(info, text=acc.account_name, bg=CARD, fg=MUTED,
                 font=("Segoe UI", 8), anchor="w").pack(anchor="w")

        # Status badge: LOGGED IN for the active account, else switch-readiness.
        if is_current:
            badge, color = "● LOGGED IN", ACCENT
        else:
            ready, _why = switcher.can_autologin(acc.account_name)
            badge = "✓ ready" if ready else "⚠ needs login"
            color = GOOD if ready else WARN
        tk.Label(top, text=badge, bg=CARD, fg=color,
                 font=("Segoe UI", 8, "bold")).pack(side="right", anchor="n")

        tk.Frame(card, bg=OUTLINE, height=1).pack(fill="x", padx=12, pady=(6, 0))
        tk.Label(card, text=f"{game_count} installed game"
                            f"{'' if game_count == 1 else 's'} mapped here",
                 bg=CARD, fg=TEXT, font=("Segoe UI", 9), anchor="w"
                 ).pack(fill="x", padx=12, pady=(8, 12))

    def open_accounts(self):
        self.show_view("accounts")

    def add_account(self):
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

    # ----------------------------------------------------------- settings
    def _build_settings(self):
        head = tk.Frame(self.content, bg=BG)
        head.pack(fill="x", padx=24, pady=(6, 2))
        tk.Label(head, text="SETTINGS", bg=BG, fg=MUTED,
                 font=("Segoe UI", 10, "bold")).pack(anchor="w")
        tk.Label(self.content, bg=BG, fg=MUTED, anchor="w",
                 text="Current configuration and detected Steam state.",
                 font=("Segoe UI", 9)).pack(fill="x", padx=24, pady=(0, 10))

        body = tk.Frame(self.content, bg=BG)
        body.pack(fill="both", expand=True, padx=24)

        # --- Detected state (read-only) ---
        root = steam_paths.steam_root()
        self._settings_section(body, "Steam")
        self._settings_row(body, "Install path", str(root) if root else "Not found",
                           good=bool(root))
        self._settings_row(body, "Accounts detected", str(len(self.accounts)))
        self._settings_row(body, "Active account", self.current_account or "None")
        self._settings_row(body, "Installed games", str(len(self.games)))
        self._settings_row(body, "Cover art (Pillow)",
                           "Available" if HAVE_PIL else "Text tiles only",
                           good=HAVE_PIL)

        # --- Launch options (existing functions) ---
        self._settings_section(body, "Launch")
        opt = tk.Frame(body, bg=BG)
        opt.pack(fill="x", pady=4)
        tk.Label(opt, text="Launch games offline", bg=BG, fg=TEXT,
                 font=("Segoe UI", 10)).pack(side="left")
        # Same offline_var the topbar toggle drives — this just mirrors it.
        self._make_toggle(opt, self.offline_var, "").pack(side="right")

        # --- Language (stub: English only for now; Arabic/RTL is a later task) ---
        self._settings_section(body, "Language")
        lang = tk.Frame(body, bg=BG)
        lang.pack(fill="x", pady=4)
        tk.Label(lang, text="Interface language", bg=BG, fg=TEXT,
                 font=("Segoe UI", 10)).pack(side="left")
        self.lang_var = tk.StringVar(value="English")
        om = tk.OptionMenu(lang, self.lang_var, "English")
        om.config(bg=CARD, fg=TEXT, activebackground=CARD_HOVER, activeforeground=TEXT,
                  relief="flat", highlightthickness=1, highlightbackground=OUTLINE,
                  state="disabled", disabledforeground=MUTED)
        om["menu"].config(bg=PANEL, fg=TEXT)
        om.pack(side="right")
        tk.Label(body, text="Arabic (RTL) is planned for a future update.",
                 bg=BG, fg=MUTED, font=("Segoe UI", 8, "italic"), anchor="w"
                 ).pack(fill="x", pady=(2, 0))

    def _settings_section(self, parent, title):
        tk.Label(parent, text=title.upper(), bg=BG, fg=ACCENT,
                 font=("Segoe UI", 9, "bold"), anchor="w"
                 ).pack(fill="x", pady=(14, 2))
        tk.Frame(parent, bg=OUTLINE, height=1).pack(fill="x")

    def _settings_row(self, parent, key, value, good=None):
        row = tk.Frame(parent, bg=BG)
        row.pack(fill="x", pady=3)
        tk.Label(row, text=key, bg=BG, fg=MUTED,
                 font=("Segoe UI", 10), anchor="w").pack(side="left")
        color = TEXT if good is None else (GOOD if good else BAD)
        tk.Label(row, text=value, bg=BG, fg=color,
                 font=("Segoe UI", 10), anchor="e").pack(side="right")

    def _on_close(self):
        self.pool.shutdown(wait=False, cancel_futures=True)
        self.destroy()


def main():
    App().mainloop()


if __name__ == "__main__":
    main()
