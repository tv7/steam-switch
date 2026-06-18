/* SteamSwitch front-end. Talks to Python via window.pywebview.api.*, which returns
   Promises. Python pushes progress back by calling the window.on* globals below. */

"use strict";

const STATE = { games: [], accounts: [], current: null, version: "", logo: null, language: "en", steam_root: null };
const UI = { view: "library", search: "", sort: "az", filter: "all", offline: false, launching: false, addingAccount: false, loaded: false };

/* ----------------------------------------------------------- i18n */
const I18N = {
  en: {
    "nav.library": "Library", "nav.settings": "Settings",
    "search.ph": "Search games in library…",
    "btn.refresh": "Refresh", "btn.filter": "Filter", "btn.stop": "Stop launch",
    "toggle.offline": "LAUNCH OFFLINE",
    "filter.sort": "SORT", "filter.az": "Name: A → Z", "filter.za": "Name: Z → A",
    "filter.show": "SHOW", "filter.all": "All accounts", "filter.unmapped": "Unmapped",
    "chip.accounts": "ACCOUNTS", "chip.accountsN": "ACCOUNTS ({n})", "chip.noAccount": "No account",
    "badge.unmapped": "unmapped",
    "lib.title": "MY GAMES", "lib.count": "Showing {shown} of {total} games",
    "lib.loading": "Loading library…", "lib.none": "No installed games found.",
    "steam.notFound": "Steam install not found on this PC.",
    "acc.title": "MANAGE ACCOUNTS",
    "acc.sub": "Your saved Steam accounts. Pick a game in Library to switch to the account that owns it; add another below.",
    "acc.add": "Add New Account", "acc.addSub": "Steam Account",
    "acc.loggedIn": "● LOGGED IN", "acc.ready": "✓ ready", "acc.needsLogin": "⚠ needs login",
    "acc.count_one": "{n} installed game mapped here", "acc.count_other": "{n} installed games mapped here",
    "acc.empty": "No accounts found yet. Click “＋ Add New Account” and log in with “Remember me” checked.",
    "set.title": "SETTINGS", "set.sub": "Current configuration and detected Steam state.",
    "set.steam": "STEAM", "set.installPath": "Install path", "set.notFound": "Not found",
    "set.accountsDetected": "Accounts detected", "set.activeAccount": "Active account",
    "set.none": "None", "set.installedGames": "Installed games",
    "set.launch": "LAUNCH", "set.launchOffline": "Launch games offline",
    "set.language": "LANGUAGE", "set.interfaceLang": "Interface language",
    "status.loading": "Loading…", "status.refreshing": "Refreshing…",
    "status.gamesInstalled": "{n} games installed.",
    "status.busyLaunch": "A launch is already in progress — please wait…",
    "status.busyAdd": "A launch is in progress — please wait…",
    "play.unmapped": "\"{name}\" isn't mapped to an account yet.\n\nIts appmanifest records no local owner — this can happen with some family-shared installs.",
    "play.unmappedTitle": "Not mapped yet",
    "addAccount.confirm": "This closes Steam and opens its login screen so you can add another account.\n\nLog in with “Remember me” checked so SteamSwitch can switch to it later.\n\nContinue?",
    "addAccount.title": "Add Steam account",
    "ui.ok": "OK", "ui.cancel": "Cancel", "ui.continue": "Continue",
    "popup.launching": "Launching", "popup.addingAccount": "Adding account",
    "popup.loginNeeded": "Manual login needed",
  },
  ar: {
    "nav.library": "المكتبة", "nav.settings": "الإعدادات",
    "search.ph": "ابحث عن لعبة في المكتبة…",
    "btn.refresh": "تحديث", "btn.filter": "تصفية", "btn.stop": "إيقاف التشغيل",
    "toggle.offline": "تشغيل دون اتصال",
    "filter.sort": "الترتيب", "filter.az": "الاسم: تصاعدي", "filter.za": "الاسم: تنازلي",
    "filter.show": "عرض", "filter.all": "كل الحسابات", "filter.unmapped": "غير المرتبطة",
    "chip.accounts": "الحسابات", "chip.accountsN": "الحسابات ({n})", "chip.noAccount": "لا يوجد حساب",
    "badge.unmapped": "غير مرتبطة",
    "lib.title": "ألعابي", "lib.count": "عرض {shown} من {total} لعبة",
    "lib.loading": "جارٍ تحميل المكتبة…", "lib.none": "لا توجد ألعاب مثبّتة.",
    "steam.notFound": "لم يُعثر على تثبيت ستيم على هذا الجهاز.",
    "acc.title": "إدارة الحسابات",
    "acc.sub": "حساباتك المحفوظة في ستيم. اختر لعبة من المكتبة للتبديل إلى الحساب المالك لها، أو أضف حسابًا آخر أدناه.",
    "acc.add": "إضافة حساب جديد", "acc.addSub": "حساب ستيم",
    "acc.loggedIn": "● مُسجَّل الدخول", "acc.ready": "✓ جاهز", "acc.needsLogin": "⚠ يحتاج تسجيل دخول",
    "acc.count_one": "{n} لعبة مثبّتة مرتبطة بهذا الحساب", "acc.count_other": "{n} لعبة مثبّتة مرتبطة بهذا الحساب",
    "acc.empty": "لا توجد حسابات بعد. اضغط «＋ إضافة حساب جديد» وسجّل الدخول مع تفعيل خيار «تذكّرني».",
    "set.title": "الإعدادات", "set.sub": "الإعدادات الحالية وحالة ستيم المكتشفة.",
    "set.steam": "ستيم", "set.installPath": "مسار التثبيت", "set.notFound": "غير موجود",
    "set.accountsDetected": "الحسابات المكتشفة", "set.activeAccount": "الحساب النشط",
    "set.none": "لا شيء", "set.installedGames": "الألعاب المثبّتة",
    "set.launch": "التشغيل", "set.launchOffline": "تشغيل الألعاب دون اتصال",
    "set.language": "اللغة", "set.interfaceLang": "لغة الواجهة",
    "status.loading": "جارٍ التحميل…", "status.refreshing": "جارٍ التحديث…",
    "status.gamesInstalled": "{n} لعبة مثبّتة.",
    "status.busyLaunch": "هناك عملية تشغيل جارية بالفعل — يرجى الانتظار…",
    "status.busyAdd": "هناك عملية تشغيل جارية — يرجى الانتظار…",
    "play.unmapped": "«{name}» غير مرتبطة بأي حساب بعد.\n\nلا يسجّل ملف التعريف الخاص بها أي مالك محلي — قد يحدث هذا مع بعض ألعاب المشاركة العائلية.",
    "play.unmappedTitle": "غير مرتبطة بعد",
    "addAccount.confirm": "سيؤدي هذا إلى إغلاق ستيم وفتح شاشة تسجيل الدخول لإضافة حساب آخر.\n\nسجّل الدخول مع تفعيل «تذكّرني» حتى يتمكن SteamSwitch من التبديل إليه لاحقًا.\n\nمتابعة؟",
    "addAccount.title": "إضافة حساب ستيم",
    "ui.ok": "حسنًا", "ui.cancel": "إلغاء", "ui.continue": "متابعة",
    "popup.launching": "جارٍ التشغيل", "popup.addingAccount": "جارٍ إضافة حساب",
    "popup.loginNeeded": "يلزم تسجيل الدخول يدويًا",
  },
};
const LANGS = { en: "English", ar: "العربية" };
let LANG = "en";
try { LANG = localStorage.getItem("lang") === "ar" ? "ar" : "en"; } catch (e) { /* ignore */ }

function t(key, vars) {
  let s = (I18N[LANG] && I18N[LANG][key]) || I18N.en[key] || key;
  if (vars) for (const k in vars) s = s.split("{" + k + "}").join(vars[k]);
  return s;
}
function tn(base, n, vars) { return t(`${base}_${n === 1 ? "one" : "other"}`, { n, ...vars }); }

function applyLang(lang) {
  LANG = lang === "ar" ? "ar" : "en";
  try { localStorage.setItem("lang", LANG); } catch (e) { /* ignore */ }
  document.documentElement.lang = LANG;
  document.documentElement.dir = LANG === "ar" ? "rtl" : "ltr";
  document.querySelectorAll("[data-i18n]").forEach(e => e.textContent = t(e.dataset.i18n));
  document.querySelectorAll("[data-i18n-ph]").forEach(e => e.placeholder = t(e.dataset.i18nPh));
}
function setLang(lang) {
  applyLang(lang);
  STATE.language = LANG;
  call("set_language", LANG);
  render();
}

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
  applyLang(STATE.language);   // honor the saved language (sets dir + static strings)
  render();
  setStatus(STATE.games.length ? t("status.gamesInstalled", { n: STATE.games.length })
            : (STATE.steam_root ? t("lib.none") : t("steam.notFound")),
            STATE.games.length ? "" : "bad");
};
window.onCover = ({ appid, url }) => {
  if (url) coverCache[appid] = url;
  if (!url) return;
  document.querySelectorAll(`.card[data-appid="${appid}"]`).forEach(c => setCardImage(c, url));
};
/* Status lines are routed to the right surface:
   - during a launch (between onLaunchStart/onLaunchDone) → a step in the launch card;
   - during add-account → a step in that card; its terminal good/bad ends the card;
   - otherwise → a transient toast. */
window.onStatus = ({ text, kind }) => {
  if (UI.launching) { Notify.step(text); return; }
  if (UI.addingAccount) {
    if (kind === "good") { Notify.finish("success", text); UI.addingAccount = false; }
    else if (kind === "bad") { Notify.finish("error", text); UI.addingAccount = false; }
    else Notify.step(text);
    return;
  }
  Notify.toast(kind === "bad" ? "error" : kind === "good" ? "success" : "info", null, text);
};
window.onLaunchStart = ({ name } = {}) => {
  UI.launching = true;
  $("#btn-stop").disabled = false;
  Notify.progress("▶", t("popup.launching"), name || "");
};
window.onLaunchDone = ({ ok, message, needs_login }) => {
  UI.launching = false;
  $("#btn-stop").disabled = true;
  Notify.finish(ok ? "success" : needs_login ? "warning" : "error", message);
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

/* ----------------------------------------------------- notifications
   All user-facing messages surface inside the app (no OS alert/confirm):
   - a promo-style centered PROGRESS card for ongoing ops (launch / add account),
     where each status line becomes a step — spinner on the current one, green ✓
     on the finished ones;
   - corner TOASTS for transient messages (warnings/errors carry no spinner);
   - centered CONFIRM / ALERT dialogs replacing window.confirm / window.alert. */
const Notify = (() => {
  const overlay = () => $("#overlay");
  const toastBox = () => $("#toasts");

  function openModal(card) {
    const o = overlay();
    o.innerHTML = "";
    const back = el("div", "modal-backdrop");
    back.append(card);
    o.append(back);
    o.classList.add("show");
    requestAnimationFrame(() => card.classList.add("in"));
    return card;
  }
  function closeModal() {
    const o = overlay();
    o.classList.remove("show");
    o.innerHTML = "";
  }

  function head(icon, title, sub, iconKind) {
    const h = el("div", "popup-head");
    h.append(el("div", "popup-ico" + (iconKind ? " " + iconKind : ""), esc(icon)));
    const meta = el("div", "popup-meta");
    meta.append(el("div", "popup-title", esc(title)));
    if (sub) meta.append(el("div", "popup-sub", esc(sub)));
    h.append(meta);
    return h;
  }

  /* ---- progress card (launch / add account) ---- */
  let prog = null;   // { card, list }
  function progress(icon, title, subtitle) {
    const card = el("div", "popup progress");
    const list = el("div", "popup-steps");
    card.append(head(icon, title, subtitle), list);
    prog = { card, list };
    openModal(card);
    return prog;
  }
  function settleActive(state) {            // close off the current spinner row
    if (!prog) return;
    const row = prog.list.querySelector(".step.active");
    if (!row) return;
    row.classList.remove("active");
    row.classList.add(state);
    $(".step-mark", row).innerHTML = state === "done" ? "✓" : state === "warn" ? "!" : "✕";
  }
  function step(text) {
    if (!prog) return;
    settleActive("done");
    const row = el("div", "step active");
    row.append(el("div", "step-mark", '<div class="spinner"></div>'),
               el("div", "step-text", esc(text)));
    prog.list.append(row);
  }
  // level: "success" | "warning" | "error"
  function finish(level, message) {
    if (!prog) { if (level !== "success") toast(level === "warning" ? "warning" : "error", null, message); return; }
    settleActive(level === "success" ? "done" : level === "warning" ? "warn" : "fail");
    const foot = el("div", "popup-foot");
    const resCls = level === "success" ? "good" : level === "warning" ? "warn" : "bad";
    if (message) foot.append(el("div", "popup-result " + resCls, esc(message)));
    const btn = el("button", "popup-btn primary", esc(t("ui.ok")));
    const done = () => { closeModal(); prog = null; };
    btn.addEventListener("click", done);
    foot.append(btn);
    prog.card.append(foot);
    if (level === "success") setTimeout(() => { if (prog && prog.card === foot.parentElement.parentElement) done(); }, 2200);
  }
  function active() { return prog !== null; }

  /* ---- toasts ---- */
  const TOAST_ICON = { info: "i", success: "✓", warning: "⚠", error: "✕" };
  function toast(kind, title, msg, opts = {}) {
    const node = el("div", "toast " + kind);
    node.append(el("div", "toast-ico", TOAST_ICON[kind] || "i"));
    const body = el("div", "toast-body");
    if (title) body.append(el("div", "toast-title", esc(title)));
    if (msg) body.append(el("div", "toast-msg", esc(msg)));
    const x = el("button", "toast-x", "✕");
    node.append(body, x);
    toastBox().append(node);
    requestAnimationFrame(() => node.classList.add("in"));
    let timer = null;
    const dismiss = () => { if (timer) clearTimeout(timer); node.classList.remove("in"); setTimeout(() => node.remove(), 200); };
    x.addEventListener("click", dismiss);
    const sticky = opts.sticky ?? (kind === "warning" || kind === "error");
    if (!sticky) timer = setTimeout(dismiss, 3400);
    return node;
  }

  /* ---- confirm / alert dialogs (replace window.confirm / window.alert) ---- */
  function dialog({ icon, iconKind, title, message, buttons }) {
    return new Promise(resolve => {
      const card = el("div", "popup dialog");
      card.append(head(icon, title, null, iconKind));
      card.append(el("div", "popup-text", esc(message)));
      const foot = el("div", "popup-foot");
      for (const b of buttons) {
        const btn = el("button", "popup-btn " + (b.cls || ""), esc(b.label));
        btn.addEventListener("click", () => { closeModal(); resolve(b.value); });
        foot.append(btn);
      }
      card.append(foot);
      openModal(card);
    });
  }
  function confirm({ title, message, confirmText, cancelText, icon = "?", iconKind }) {
    return dialog({
      icon, iconKind, title, message,
      buttons: [
        { label: cancelText || t("ui.cancel"), value: false, cls: "ghost" },
        { label: confirmText || t("ui.continue"), value: true, cls: "primary" },
      ],
    });
  }
  function alert({ title, message, okText, icon = "⚠", iconKind = "warning" }) {
    return dialog({
      icon, iconKind, title, message,
      buttons: [{ label: okText || t("ui.ok"), value: true, cls: "primary" }],
    });
  }

  return { progress, step, finish, active, toast, confirm, alert, close: closeModal };
})();

/* --------------------------------------------------------- chrome render */
function renderChrome() {
  $("#version").textContent = STATE.version;
  const logo = $("#brand-logo");
  if (logo && STATE.logo && logo.getAttribute("src") !== STATE.logo) logo.src = STATE.logo;
  document.querySelectorAll(".nav-item").forEach(b =>
    b.classList.toggle("active", b.dataset.view === UI.view));
  // account chip
  const cur = STATE.accounts.find(a => a.current);
  const name = STATE.current || (cur && cur.persona_name) || t("chip.noAccount");
  $("#chip-count").textContent = STATE.accounts.length
    ? t("chip.accountsN", { n: STATE.accounts.length }) : t("chip.accounts");
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
  head.append(el("span", "view-title", t("lib.title")),
              el("span", "view-sub", t("lib.count", { shown: list.length, total: STATE.games.length })));
  c.append(head);

  if (!STATE.games.length) {
    c.append(el("div", "empty", !UI.loaded ? t("lib.loading")
      : (STATE.steam_root ? t("lib.none") : t("steam.notFound"))));
    return;
  }

  const grid = el("div", "grid");
  for (const g of list) {
    const card = el("div", "card");
    card.dataset.appid = g.appid;
    card.append(el("div", "placeholder", esc(g.name)));
    const badge = el("div", "badge");
    const sw = el("span", "swatch"); sw.style.background = g.owner_color;
    badge.append(sw, el("span", "nm", esc(g.owner_name || t("badge.unmapped"))));
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
  head.append(el("span", "view-title", t("acc.title")));
  c.append(head);
  c.append(el("div", "view-sub", t("acc.sub")));

  const grid = el("div", "acct-grid");
  grid.style.marginTop = "14px";

  const add = el("div", "acct-add",
    `<div class="plus">＋</div><div class="t">${esc(t("acc.add"))}</div><div class="s">${esc(t("acc.addSub"))}</div>`);
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
    if (a.current) { badgeTxt = t("acc.loggedIn"); badgeColor = "var(--primary-soft)"; }
    else if (a.ready) { badgeTxt = t("acc.ready"); badgeColor = "var(--good)"; }
    else { badgeTxt = t("acc.needsLogin"); badgeColor = "var(--warn)"; }
    const badge = el("div", "acct-badge", badgeTxt); badge.style.color = badgeColor;
    const top = el("div", "acct-top"); top.append(av, meta, badge);
    card.append(top, el("div", "acct-divider"),
      el("div", "acct-count", tn("acc.count", a.game_count)));
    grid.append(card);
  }
  c.append(grid);

  if (!STATE.accounts.length)
    c.append(el("div", "empty", t("acc.empty")));
}

function renderSettings(c) {
  const head = el("div", "view-head");
  head.append(el("span", "view-title", t("set.title")));
  c.append(head);
  c.append(el("div", "view-sub", t("set.sub")));

  const wrap = el("div", "settings");
  const row = (k, v, cls) => `<div class="set-row"><span class="set-key">${esc(k)}</span><span class="set-val ${cls || ""}">${esc(v)}</span></div>`;

  wrap.append(el("div", "set-section", t("set.steam")));
  const p1 = el("div", "panel", [
    row(t("set.installPath"), STATE.steam_root || t("set.notFound"), STATE.steam_root ? "good" : "bad"),
    row(t("set.accountsDetected"), STATE.accounts.length),
    row(t("set.activeAccount"), STATE.current || t("set.none")),
    row(t("set.installedGames"), STATE.games.length),
  ].join(""));
  wrap.append(p1);

  wrap.append(el("div", "set-section", t("set.launch")));
  const p2 = el("div", "panel");
  const lrow = el("div", "set-row");
  lrow.append(el("span", "set-key", t("set.launchOffline")));
  const tog = el("label", "toggle" + (UI.offline ? " on" : ""), `<span class="switch"><span class="knob"></span></span>`);
  tog.addEventListener("click", () => { setOffline(!UI.offline); tog.classList.toggle("on", UI.offline); });
  lrow.append(tog);
  p2.append(lrow);
  wrap.append(p2);

  wrap.append(el("div", "set-section", t("set.language")));
  const p3 = el("div", "panel");
  const grow = el("div", "set-row");
  grow.append(el("span", "set-key", t("set.interfaceLang")));
  const sel = el("select", "lang-select");
  for (const code in LANGS) {
    const opt = new Option(LANGS[code], code);
    opt.selected = (code === LANG);
    sel.append(opt);
  }
  sel.addEventListener("change", () => setLang(sel.value));
  grow.append(sel);
  p3.append(grow);
  wrap.append(p3);

  c.append(wrap);
}

/* ------------------------------------------------------------- actions */
async function play(g) {
  if (UI.launching) { Notify.toast("warning", null, t("status.busyLaunch")); return; }
  if (!g.mapped) {
    Notify.alert({ title: t("play.unmappedTitle"), message: t("play.unmapped", { name: g.name }) });
    return;
  }
  const res = await call("play", g.appid, UI.offline);   // progress arrives via onLaunchStart/onStatus
  if (res && !res.ok && res.message) Notify.toast("warning", null, res.message);
}

function setOffline(on) {
  UI.offline = on;
  $("#toggle-offline").classList.toggle("on", on);
}

async function addAccount() {
  if (UI.launching) { Notify.toast("warning", null, t("status.busyAdd")); return; }
  const ok = await Notify.confirm({
    title: t("addAccount.title"),
    message: t("addAccount.confirm"),
    confirmText: t("ui.continue"),
  });
  if (!ok) return;
  UI.addingAccount = true;
  Notify.progress("＋", t("popup.addingAccount"), t("acc.addSub"));   // onStatus lines fill it in
  await call("add_account");
}

function refresh() {
  Notify.toast("info", null, t("status.refreshing"));
  call("request_state");   // onState re-renders + updates the ambient status bar
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
  m.append(el("div", "menu-label", t("filter.sort")));
  m.append(item(t("filter.az"), UI.sort === "az", () => UI.sort = "az"));
  m.append(item(t("filter.za"), UI.sort === "za", () => UI.sort = "za"));
  m.append(el("div", "menu-sep"));
  m.append(el("div", "menu-label", t("filter.show")));
  m.append(item(t("filter.all"), UI.filter === "all", () => UI.filter = "all"));
  for (const a of STATE.accounts)
    m.append(item(a.persona_name, UI.filter === a.steamid64, () => UI.filter = a.steamid64, a.color));
  m.append(item(t("filter.unmapped"), UI.filter === "unmapped", () => UI.filter = "unmapped"));
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
  applyLang(LANG);         // apply cached language (dir + static strings) up front
  render();                // paint the empty shell immediately
  setStatus(t("status.loading"));
  call("request_state");   // onState fills it in (and re-applies the saved language)
}

/* pywebview injects the api asynchronously; wait for it (or boot now if present). */
if (window.pywebview && window.pywebview.api) boot();
else window.addEventListener("pywebviewready", boot);
