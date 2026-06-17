# SteamSwitch UI Refresh — Implementation Plan

Adapt the Proton-Dark mockups (`ui_refresh/`) into the existing Tkinter app.
**No changes to `core/*` or any launch/switch/offline logic** — presentation only,
all in `app.py`. Visual verification is Windows-only (no Tkinter in the dev sandbox).

## Settled decisions
- Keep the **portrait cover grid** for Library (not the text-row mockup), preserving
  `core/covers.py` and the badge/`_fit_portrait` work.
- **No standalone "Switch to Account"** action — switching stays launch-only (pick a
  game). Account cards are informational + "Add New Account".
- **Settings tab** surfaces only existing state/functions (offline default, detected
  Steam root, account count, version) + a **Language** row.
- **Arabic = later.** Language selector is a stub now (English only); Arabic needs RTL
  work (Tkinter has weak bidi support) — its own task.
- Rounded corners: square containers + 1px outline borders (on-spec per DESIGN.md);
  optionally round the cover *image* corners via Pillow mask where present.

## Phase 1 — visual restyle (safe)
1. **Palette swap** (`app.py:33-45`) to Proton-Dark tokens (BG `#0b1326`, PANEL
   `#131b2e`, CARD `#171f33`, ACCENT `#adc6ff`, ACCENT2 `#4d8eff`, TEXT `#dae2fd`,
   GOOD `#10b981`, BAD `#ef4444`, new OUTLINE `#424754`, BASE `#060e20`).
2. **Left sidebar + view-switch** (`_build_chrome`, `app.py:208`): topbar across the
   top; below it a horizontal main = sidebar (brand, Library/Settings nav, bottom
   account chip) + content pane. `self._view` toggles Library/Accounts/Settings into
   the same content pane.
3. **Restyle controls**: search field (BASE bg, outline, magnifier, "Search games in
   library…"), new **Refresh** button (wired to `refresh_all`), Filter ▾ secondary
   style, Launch-offline as a **Canvas-drawn pill toggle** (keeps `offline_var`),
   Stop-launch ghost/error style (`_set_cancel_enabled` unchanged).
4. **Content header** ("MY GAMES" + "Showing N of M") and **footer status bar**
   (green dot + SYSTEM READY via `set_status`, version right-aligned).
5. **Accounts view** (convert `AccountsWindow` → `render_accounts()` in content):
   2-col card grid, avatars (from Steam `config/avatarcache/<id>.png`, fallback to
   colored initial), ✓ready/⚠needs-login badge (`switcher.can_autologin`), game
   count, "LOGGED IN" highlight on current; "+ Add New Account" card → `add_account`.
6. **Settings view**: offline default toggle, read-only Steam root/account count/
   version, disabled Language=English row (Arabic stub).

## Phase 2 — optional richer data (new core plumbing; defer)
- Playtime from `userdata/<id>/config/localconfig.vdf` (no API key).
- "Update available" from appmanifest `StateFlags`.
- Drop "Last Session" (no reliable local source).

## Untouched
core/* entirely; `play()`, `cancel_launch()`, `_load_cover()`, cancel guard, offline
flag flow, cover resolution, account-color assignment.

## Testing
`python tests/smoke_test.py` (stays 47/47), `py_compile app.py`, eyeball on Windows.

## Commit breakdown
1) palette + footer  2) sidebar + view-switch  3) toggle + button/search restyle
4) accounts-in-content + avatars  5) settings view  6) (opt) rounded cover corners
7) (Phase 2) playtime/status data
