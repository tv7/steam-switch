# ORBIT — CINEMA redesign implementation plan

**Status: APPROVED, not started.** (Pinned 2026-07-02; pick up from Phase 0.)

The user chose the "CINEMA" design direction (see `mock1.html` + the `m1-*.html`
screens in this folder — self-contained HTML previews; open in a browser at
1440×900, or screenshot them like the mocks in the repo root). The app is being
renamed **ORBIT** (official name) and gets a real app icon.

Design system in one line: console-style dark UI — a full-bleed **hero** of the
last-played game with instant Play, horizontal **shelves** below (Continue
playing + one per store), top-bar tab nav (Library / Accounts / Settings),
Ctrl-K **search palette** where Enter = play.

Mock inventory:
- `mock1.html` — CINEMA library (chosen). `mock2` mission-control, `mock3`
  aurora, `mock4` aurora+detail-panel hybrid (rejected alternatives, kept for
  reference).
- `m1-detail.html` — game detail = hero takeover (replaces DetailView page).
- `m1-accounts.html` — accounts & stores cards.
- `m1-settings.html` — settings (NOTE: "confirm before switching" row was
  replaced by **"Run on Windows startup"** per user request).
- `m1-search.html` — Ctrl-K palette.

**Hard rule: everything must be truly functional — no mocked/fabricated data.**
Playtime/lastPlayed chips only where real data exists (Steam localconfig; ORBIT's
own launch history for other stores).

## Phase 0 — Identity
1. Rename app to ORBIT: CMake target → `Orbit.exe`, QML URI `SteamSwitch` →
   `Orbit`, setApplicationName/window title, docs. Core `ss::` namespaces + repo
   name unchanged.
2. App icon: orbit mark (dark disc, amber ring + orbiting dot) as SVG → PNGs
   16–256 (browser-render like the mocks) → pack `orbit.ico` (Python, PNG-entry
   ICO). Wire: Windows `.rc` in the exe (taskbar/Explorer), `setWindowIcon`
   (Alt-Tab/titlebar), Qt resource PNG for QML (top bar, onboarding, About).

## Phase 1 — Core data (headless + tests; target ~55 tests)
3. Expose per-game playtime/lastPlayed from `steam::appUsage` through the scan.
4. ORBIT launch history: `{gameId: lastLaunchedAt}` persisted in settings.json on
   successful `play()` — truthful ordering for non-Steam stores.
5. Hero/wide art variant in covers + store_covers: Steam `library_hero.jpg`
   (librarycache → CDN → appdetails header), Xbox displaycatalog
   TitledHeroArt/SuperHeroArt, Epic catcache DieselGameBox, GOG API background;
   cached `<id>_hero.jpg`.
6. Run on Windows startup: `HKCU\...\CurrentVersion\Run` value `ORBIT` = exe
   path; new `platform::regDeleteValue` primitive; hidden off-Windows.
7. Cover cache size + clear over `data/covers/`.

## Phase 2 — Backend surface
8. Roles `playtime`/`lastPlayed`; `requestHero`/`heroReady`; lastPlayed sort.
9. Invokables: `switchTo(account)` (switch+restart Steam, NO launch),
   `pinToAccount(appid, sid)` (→ `accounts::setOverride` + rescan),
   `setRunAtStartup`, `setHeroMode`, `clearCoverCache`, `coverCacheSize`,
   `lastScanTime`.
10. Register `GameFilterModel` as QML-instantiable so each shelf is its own live
    filter over the single GameModel.

## Phase 3 — QML rebuild (screen by screen, per the m1-* previews)
11. Shell: top bar replaces sidebar (logo icon + tabs + search + status pill +
    avatar + existing frameless window controls top-right). Delete SideNavItem,
    ManagePanel (content moves to Accounts).
12. Library: hero (per heroMode setting; real hero art, gradient fades,
    Play/Play-offline, truthful chips) + shelves. Keep empty states.
13. Detail-as-hero: selection re-casts hero (kicker, explainer, stat card,
    Play/Offline/Pin-to-account menu, "More on <account>" shelf, Back). Retire
    DetailView.qml.
14. Search palette: Ctrl-K overlay; ↑↓, Enter=play, Shift+Enter=offline,
    Tab=details, Esc.
15. Accounts: Steam cards (live badge, Switch now, counts, View library →),
    Add-account card (existing flow), detected-store rows, rescan.
16. Settings: Launching (offline default, run-at-startup), Interface (language,
    hero mode), Library (rescan + last scan, cover cache), Families tip.
17. Onboarding restyled to CINEMA + icon (same real detection logic).

## Phase 4 — Polish + handoff
18. Regenerate ar.ts/ar.qm (Arabic for all new strings), RTL check, qmllint
    clean, headless tests pass. Phase-sized commits on `cpp-qt-rewrite`.
19. Windows verification checklist: icon in taskbar/Alt-Tab/Explorer, hero art
    per store, shelf ordering, palette keys, switch-now, startup-run registry
    value, Arabic/RTL.

Out of scope (unchanged): Steam switch/offline logic, store scanning,
windeployqt packaging, Python/Tauri retirement.
