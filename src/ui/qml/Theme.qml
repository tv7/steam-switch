// CINEMA design tokens + helpers (design/m1base.css): console-style dark UI,
// a single amber accent, per-store brand colors. Singleton so every view shares
// the same palette + helper functions.
pragma Singleton
import QtQuick
import Orbit

QtObject {
    id: theme

    // ---- surfaces / text (m1base.css custom properties) ----
    readonly property color bg:      "#0a0c11"                    // --bg
    readonly property color panel:   Qt.rgba(1, 1, 1, 0.045)      // --panel
    readonly property color line:    Qt.rgba(1, 1, 1, 0.09)       // --line
    readonly property color text:    "#eef1f7"                    // --txt
    readonly property color muted:   "#98a0b3"                    // --mut
    readonly property color faint:   "#5c657a"                    // --faint
    readonly property color ghost:   Qt.rgba(1, 1, 1, 0.30)
    readonly property color hairline: Qt.rgba(1, 1, 1, 0.07)
    readonly property color fill:     Qt.rgba(1, 1, 1, 0.06)
    readonly property color fillHover: Qt.rgba(1, 1, 1, 0.12)

    readonly property color good:  "#4ade80"
    readonly property color warn:  "#ffd48a"
    readonly property color bad:   "#e23b3b"

    // ---- accent: CINEMA amber (static — no more per-game dynamic accent) ----
    readonly property color accent:   "#f59e0b"                   // --acc1
    readonly property color accent2:  "#f472b6"                   // --acc2
    readonly property string accentFg: "#1a1205"

    // radii
    readonly property int rSm: 7
    readonly property int rMd: 12
    readonly property int rLg: 16

    // type — Space Grotesk headings, Manrope body. Cairo carries the Arabic
    // glyphs (the Latin faces have none), so the whole face switches with the
    // language; Cairo's own Latin set keeps mixed lines even.
    readonly property string fontArabic:  "Cairo"
    readonly property string fontDisplay: backend.rtl ? fontArabic : "Space Grotesk"
    readonly property string fontBody:    backend.rtl ? fontArabic : "Manrope"

    // Per-game placeholder palette. Real covers don't carry a color, so a stable
    // hue is derived from the game id for the no-art gradient tiles.
    readonly property var glowPalette: [
        "#7c5cff", "#ff7a3d", "#ff3d9a", "#3fd27a", "#3da5ff", "#8aa0c0",
        "#c0a0ff", "#2fd0d0", "#4d6dff", "#b06dff", "#ff9a3d", "#5db0ff"
    ]
    function glowIndex(id) {
        var n = Number(id);
        if (!isFinite(n)) n = 0;
        var h = Math.abs(Math.floor(n % 1000000)) ^ Math.abs(Math.floor(n / 1000000) % 1000000);
        return h % glowPalette.length;
    }
    function accentFor(id)  { return glowPalette[glowIndex(id)]; }
    function coverBottom(id) { return Qt.rgba(0.03, 0.04, 0.07, 1); }

    // Readable foreground for a solid color (luminance threshold).
    function fgOn(hex) {
        var c = Qt.color(hex);
        var lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;   // 0..1
        return lum > 0.588 ? "#0a0a0c" : "#ffffff";
    }

    // Per-store brand identity, keyed by the model's store name ("Steam"/"Epic"/…).
    // Colors from m1base.css (--steam/--epic/--gog/--xbox).
    readonly property var storeMeta: ({
        "Steam": { key: "steam", name: "Steam",      short: "STEAM",     color: "#66c0f4", fg: "#062032" },
        "Epic":  { key: "epic",  name: "Epic Games", short: "EPIC",      color: "#a78bfa", fg: "#160e2e" },
        "GOG":   { key: "gog",   name: "GOG",        short: "GOG",       color: "#c084fc", fg: "#22093a" },
        "Xbox":  { key: "xbox",  name: "Game Pass",  short: "GAME PASS", color: "#4ade80", fg: "#0c2913" }
    })
    function store(name) {
        return storeMeta[name] || { key: "?", name: name, short: name, color: "#9aa0a6", fg: "#ffffff" }
    }

    // ---- truthful time/usage labels (only called when real data exists) ----
    // "214 h" / "45 min" from localconfig playtime minutes.
    function hoursLabel(minutes) {
        if (minutes >= 90) return qsTr("%1 h").arg(Math.round(minutes / 60));
        return qsTr("%1 min").arg(Math.max(1, Math.round(minutes)));
    }
    // "just now" / "3 h ago" / "yesterday" / "5 days ago" / a date.
    function relTime(unixSecs) {
        if (unixSecs <= 0) return "";
        var s = Date.now() / 1000 - unixSecs;
        if (s < 5400) return qsTr("just now");
        if (s < 86400) return qsTr("%1 h ago").arg(Math.round(s / 3600));
        if (s < 172800) return qsTr("yesterday");
        if (s < 14 * 86400) return qsTr("%1 days ago").arg(Math.round(s / 86400));
        return new Date(unixSecs * 1000).toLocaleDateString(Qt.locale(), Locale.ShortFormat);
    }
}
