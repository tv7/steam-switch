// Shared, mutable UI state for the ORBIT shell. A singleton so any view can read
// or drive navigation without threading properties through every parent. The
// backend (games/accounts/launch) stays the source of truth; this only holds the
// view-layer state the design needs (which screen, which game, overlays).
pragma Singleton
import QtQuick
import SteamSwitch

QtObject {
    id: app

    // "library" | "detail" | "accounts" | "settings"
    property string view: "library"
    property string previousView: "library"

    // The game shown on the detail screen (a plain JS object copied from the model
    // row: appid, name, store, accountName, accountColor, mapped, fullyInstalled).
    property var selected: null

    // Library launch-offline toggle (Steam only; other stores ignore it).
    property bool offline: false

    // Which store's Manage panel is open ("" = closed).
    property string manageKey: ""

    // Current filters (drive the backend proxy model).
    property string storeFilter: "all"     // "all" | store name ("Steam"/…)
    property string accountFilter: "all"    // "all" | steamid64 | "unmapped"

    // The active game id drives the dynamic accent (detail = selected game,
    // elsewhere = the design's default violet).
    readonly property string accent: (view === "detail" && selected)
        ? Theme.accentFor(selected.appid) : Theme.accentDefault
    readonly property string accentFg: Theme.fgOn(accent)

    function open(game) {
        selected = game;
        previousView = view;
        view = "detail";
    }
    function back() {
        view = (previousView === "detail" ? "library" : previousView);
    }
    function go(v) {
        if (view !== v) { previousView = view; view = v; }
        manageKey = "";
    }
}
