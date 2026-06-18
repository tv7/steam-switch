/* SteamSwitch Tauri bridge shim.

   The UI (app.js) was written for pywebview: it calls window.pywebview.api.<m>(...)
   and receives pushes via window.on* globals. This shim provides the same surface
   but over the localhost sidecar (server.py):
     • window.pywebview.api.<m>(...args)  ->  POST /api/<m>  (JSON body = [args])
     • SSE /events {"fn","payload"}       ->  window[fn](payload)

   In dev the page is served by the sidecar (same origin). In the Tauri build the
   page is served from tauri://; the shell injects the sidecar URL as
   window.__SIDECAR__ before this script runs. */
(function () {
  "use strict";

  var BASE = (window.__SIDECAR__ || "").replace(/\/+$/, "");

  function post(method, args) {
    return fetch(BASE + "/api/" + method, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(args || []),
    }).then(function (r) { return r.json(); })
      .catch(function (e) { console.error("api", method, e); return null; });
  }

  // Same method names app.js calls through window.pywebview.api.
  var METHODS = ["request_state", "request_cover", "play", "cancel",
                 "set_language", "add_account"];
  var api = {};
  METHODS.forEach(function (m) {
    api[m] = function () { return post(m, [].slice.call(arguments)); };
  });
  window.pywebview = { api: api };

  // Stream events -> window.on* callbacks. EventSource auto-reconnects, and the
  // sidecar replays the latest onState on (re)connect, so a drop is self-healing.
  function connect() {
    var es;
    try { es = new EventSource(BASE + "/events"); }
    catch (e) { console.error("events", e); return; }
    es.onmessage = function (ev) {
      var msg;
      try { msg = JSON.parse(ev.data); } catch (e) { return; }
      var fn = window[msg.fn];
      if (typeof fn === "function") {
        try { fn(msg.payload); } catch (e) { console.error(msg.fn, e); }
      }
    };
    es.onerror = function () { /* auto-reconnect */ };
  }
  connect();
})();
