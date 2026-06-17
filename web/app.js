/* SteamSwitch front-end. Talks to Python via window.pywebview.api.*, which returns
   Promises. Python pushes progress back by calling the window.on* globals below. */

"use strict";

const STATE = { games: [], accounts: [], current: null, version: "", logo: null, steam_root: null };
const UI = { view: "library", search: "", sort: "az", filter: "all", offline: false, launching: false, loaded: false };

const $ = (sel, root = document) => root.querySelector(sel);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const esc = (s) => String(s == null ? "" : s).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

/* ------------------------------------------------------------- bridge */
function api() { return (window.pywebview && window.pywebview.api) || null; }

async function call(name, ...args) {
  const a = api();
  if (!a || typeof a[name] !== "function") return null;
  try { return await a[name](...args); }
  catch (e) { console.error(name, e); return null; }
}

/* Python -> page */
window.onState = (s) => {
  applyState(s);
  render();
  setStatus(STATE.games.length ? `${STATE.games.length} games installed.`
            : (STATE.steam_root ? "No installed games found."
                                : "Steam install not found on this PC."),
            STATE.games.length ? "" : "bad");
};
window.onCover = ({ appid, url }) => {
  if (url) coverCache[appid] = url;
  if (!url) return;
  document.querySelectorAll(`.card[data-appid="${appid}"]`).forEach(c => setCardImage(c, url));
};
window.onStatus = ({ text, kind }) => setStatus(text, kind);
window.onLaunchStart = () => { UI.launching = true; $("#btn-stop").disabled = false; };
window.onLaunchDone = ({ ok, message, needs_login }) => {
  UI.launching = false;
  $("#btn-stop").disabled = true;
  setStatus(message, ok ? "good" : "bad");
  if (needs_login) alert(message);
};

function applyState(s) {
  if (!s) return;
  UI.loaded = true;
  STATE.games = s.games || [];
  STATE.accounts = s.accounts || [];
  STATE.current = s.current_account || null;
  STATE.version = s.version || "";
  STATE.logo = s.logo || null;
  STATE.steam_root = s.steam_root || null;
  if (UI.filter !== "all" && UI.filter !== "unmapped"
      && !STATE.accounts.some(a => a.steamid64 === UI.filter)) UI.filter = "all";
}

/* ------------------------------------------------------------- status */
function setStatus(text, kind = "") {
  $("#status-text").textContent = text || "";
  $(".statusbar").classList.toggle("bad", kind === "bad");
}

/* --------------------------------------------------------- chrome render */
function renderChrome() {
  $("#version").textContent = STATE.version;
  const logo = $("#brand-logo");
  if (logo && STATE.logo && logo.getAttribute("src") !== STATE.logo) logo.src = STATE.logo;
  document.querySelectorAll(".nav-item").forEach(b =>
    b.classList.toggle("active", b.dataset.view === UI.view));
  // account chip
  const name = STATE.current || "No account";
  const cur = STATE.accounts.find(a => a.account_name === STATE.current || a.persona_name === STATE.current);
  $("#chip-count").textContent = STATE.accounts.length ? `ACCOUNTS (${STATE.accounts.length})` : "ACCOUNTS";
  $("#chip-name").textContent = name;
  const av = $("#chip-avatar");
  av.textContent = name.slice(0, 2).toUpperCase();
  av.style.background = cur ? cur.color : "#2a3a6a";
  // offline toggle
  $("#toggle-offline").classList.toggle("on", UI.offline);
}

/* ------------------------------------------------------------- views */
function render() {
  renderChrome();
  const c = $("#content");
  c.innerHTML = "";
  if (UI.view === "library") renderLibrary(c);
  else if (UI.view === "accounts") renderAccounts(c);
  else if (UI.view === "settings") renderSettings(c);
}

function visibleGames() {
  const q = UI.search.trim().toLowerCase();
  let list = STATE.games.filter(g => g.name.toLowerCase().includes(q));
  if (UI.filter === "unmapped") list = list.filter(g => !g.mapped);
  else if (UI.filter !== "all") list = list.filter(g => g.owner_sid === UI.filter);
  list.sort((a, b) => a.name.toLowerCase().localeCompare(b.name.toLowerCase()));
  if (UI.sort === "za") list.reverse();
  return list;
}

function renderLibrary(c) {
  const list = visibleGames();
  const head = el("div", "view-head");
  head.append(el("span", "view-title", "MY GAMES"),
              el("span", "view-sub", `Showing ${list.length} of ${STATE.games.length} games`));
  c.append(head);

  if (!STATE.games.length) {
    c.append(el("div", "empty", !UI.loaded ? "Loading library…"
      : (STATE.steam_root ? "No installed games found." : "Steam install not found on this PC.")));
    return;
  }

  const grid = el("div", "grid");
  for (const g of list) {
    const card = el("div", "card");
    card.dataset.appid = g.appid;
    card.append(el("div", "placeholder", esc(g.name)));
    const badge = el("div", "badge");
    const sw = el("span", "swatch"); sw.style.background = g.owner_color;
    badge.append(sw, el("span", "nm", esc(g.owner_name || "unmapped")));
    card.append(badge);
    card.addEventListener("click", () => play(g));
    grid.append(card);
  }
  c.append(grid);
  lazyLoadCovers(grid);
}

const coverCache = {};   // appid -> data URL, so re-renders don't re-fetch
let coverObserver = null;

function setCardImage(card, url) {
  if (!card.isConnected || card.querySelector("img")) return;
  const img = new Image();
  img.onload = () => { const ph = $(".placeholder", card); if (ph) ph.remove(); card.prepend(img); };
  img.src = url;
}

function lazyLoadCovers(grid) {
  if (coverObserver) coverObserver.disconnect();
  coverObserver = new IntersectionObserver((entries, obs) => {
    for (const en of entries) {
      if (!en.isIntersecting) continue;
      const card = en.target;
      obs.unobserve(card);
      const appid = card.dataset.appid;
      if (coverCache[appid]) setCardImage(card, coverCache[appid]);
      else call("request_cover", appid);   // result arrives via window.onCover
    }
  }, { rootMargin: "300px" });
  grid.querySelectorAll(".card").forEach(c => coverObserver.observe(c));
}

function renderAccounts(c) {
  const head = el("div", "view-head");
  head.append(el("span", "view-title", "MANAGE ACCOUNTS"));
  c.append(head);
  c.append(el("div", "view-sub", "Your saved Steam accounts. Pick a game in Library to switch to the account that owns it; add another below."));

  const grid = el("div", "acct-grid");
  grid.style.marginTop = "14px";

  const add = el("div", "acct-add",
    `<div class="plus">＋</div><div class="t">Add New Account</div><div class="s">Steam Account</div>`);
  add.addEventListener("click", addAccount);
  grid.append(add);

  for (const a of STATE.accounts) {
    const card = el("div", "acct-card" + (a.current ? " current" : ""));
    const av = el("div", "acct-avatar", esc(a.persona_name.slice(0, 2).toUpperCase()));
    av.style.background = a.color;
    const meta = el("div", "acct-meta");
    meta.append(el("div", "acct-name", esc(a.persona_name)),
                el("div", "acct-login", esc(a.account_name)));
    let badgeTxt, badgeColor;
    if (a.current) { badgeTxt = "● LOGGED IN"; badgeColor = "var(--primary-soft)"; }
    else if (a.ready) { badgeTxt = "✓ ready"; badgeColor = "var(--good)"; }
    else { badgeTxt = "⚠ needs login"; badgeColor = "var(--warn)"; }
    const badge = el("div", "acct-badge", badgeTxt); badge.style.color = badgeColor;
    const top = el("div", "acct-top"); top.append(av, meta, badge);
    card.append(top, el("div", "acct-divider"),
      el("div", "acct-count", `${a.game_count} installed game${a.game_count === 1 ? "" : "s"} mapped here`));
    grid.append(card);
  }
  c.append(grid);

  if (!STATE.accounts.length)
    c.append(el("div", "empty", "No accounts found yet. Click “＋ Add New Account” and log in with “Remember me” checked."));
}

function renderSettings(c) {
  const head = el("div", "view-head");
  head.append(el("span", "view-title", "SETTINGS"));
  c.append(head);
  c.append(el("div", "view-sub", "Current configuration and detected Steam state."));

  const wrap = el("div", "settings");
  const row = (k, v, cls) => `<div class="set-row"><span class="set-key">${esc(k)}</span><span class="set-val ${cls || ""}">${esc(v)}</span></div>`;

  wrap.append(el("div", "set-section", "STEAM"));
  const p1 = el("div", "panel", [
    row("Install path", STATE.steam_root || "Not found", STATE.steam_root ? "good" : "bad"),
    row("Accounts detected", STATE.accounts.length),
    row("Active account", STATE.current || "None"),
    row("Installed games", STATE.games.length),
  ].join(""));
  wrap.append(p1);

  wrap.append(el("div", "set-section", "LAUNCH"));
  const p2 = el("div", "panel");
  const lrow = el("div", "set-row");
  lrow.append(el("span", "set-key", "Launch games offline"));
  const t = el("label", "toggle" + (UI.offline ? " on" : ""), `<span class="switch"><span class="knob"></span></span>`);
  t.addEventListener("click", () => { setOffline(!UI.offline); t.classList.toggle("on", UI.offline); });
  lrow.append(t);
  p2.append(lrow);
  wrap.append(p2);

  wrap.append(el("div", "set-section", "LANGUAGE"));
  const p3 = el("div", "panel");
  const grow = el("div", "set-row");
  grow.append(el("span", "set-key", "Interface language"));
  const sel = el("select"); sel.disabled = true; sel.append(new Option("English", "en"));
  grow.append(sel);
  p3.append(grow);
  p3.append(el("div", "set-note", "Arabic (RTL) is planned for a future update."));
  wrap.append(p3);

  c.append(wrap);
}

/* ------------------------------------------------------------- actions */
async function play(g) {
  if (UI.launching) { setStatus("A launch is already in progress — please wait…", "bad"); return; }
  if (!g.mapped) {
    alert(`"${g.name}" isn't mapped to an account yet.\n\nIts appmanifest records no local owner — this can happen with some family-shared installs.`);
    return;
  }
  const res = await call("play", g.appid, UI.offline);
  if (res && !res.ok && res.message) setStatus(res.message, "bad");
}

function setOffline(on) {
  UI.offline = on;
  $("#toggle-offline").classList.toggle("on", on);
}

async function addAccount() {
  if (UI.launching) { setStatus("A launch is in progress — please wait…", "bad"); return; }
  if (!confirm("This closes Steam and opens its login screen so you can add another account.\n\nLog in with “Remember me” checked so SteamSwitch can switch to it later.\n\nContinue?")) return;
  await call("add_account");
}

function refresh() {
  setStatus("Refreshing…");
  call("request_state");   // onState re-renders + sets the final status
}

/* ------------------------------------------------------------- filter menu */
function toggleFilterMenu(show) {
  const m = $("#filter-menu");
  if (show === undefined) show = m.hidden;
  if (!show) { m.hidden = true; return; }
  m.innerHTML = "";
  const item = (label, sel, onClick, swatch) => {
    const b = el("button", "menu-item" + (sel ? " sel" : ""));
    b.innerHTML = `<span class="tick">✓</span>${swatch ? `<span class="swatch" style="background:${swatch}"></span>` : ""}<span>${esc(label)}</span>`;
    b.addEventListener("click", () => { onClick(); toggleFilterMenu(false); render(); });
    return b;
  };
  m.append(el("div", "menu-label", "SORT"));
  m.append(item("Name: A → Z", UI.sort === "az", () => UI.sort = "az"));
  m.append(item("Name: Z → A", UI.sort === "za", () => UI.sort = "za"));
  m.append(el("div", "menu-sep"));
  m.append(el("div", "menu-label", "SHOW"));
  m.append(item("All accounts", UI.filter === "all", () => UI.filter = "all"));
  for (const a of STATE.accounts)
    m.append(item(a.persona_name, UI.filter === a.steamid64, () => UI.filter = a.steamid64, a.color));
  m.append(item("Unmapped", UI.filter === "unmapped", () => UI.filter = "unmapped"));
  m.hidden = false;
}

/* ------------------------------------------------------------- wiring */
function wire() {
  document.querySelectorAll(".nav-item").forEach(b =>
    b.addEventListener("click", () => { UI.view = b.dataset.view; toggleFilterMenu(false); render(); }));
  $("#account-chip").addEventListener("click", () => { UI.view = "accounts"; render(); });
  $("#search").addEventListener("input", e => { UI.search = e.target.value; if (UI.view === "library") render(); });
  $("#btn-refresh").addEventListener("click", refresh);
  $("#btn-filter").addEventListener("click", e => { e.stopPropagation(); toggleFilterMenu(); });
  $("#toggle-offline").addEventListener("click", () => setOffline(!UI.offline));
  $("#btn-stop").addEventListener("click", () => call("cancel"));
  document.addEventListener("click", e => { if (!e.target.closest(".filter-wrap")) toggleFilterMenu(false); });
}

function boot() {
  wire();
  render();                // paint the empty shell immediately
  setStatus("Loading…");
  call("request_state");   // onState fills it in when the scan finishes
}

/* pywebview injects the api asynchronously; wait for it (or boot now if present). */
if (window.pywebview && window.pywebview.api) boot();
else window.addEventListener("pywebviewready", boot);
