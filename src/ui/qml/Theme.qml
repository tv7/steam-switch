// ORBIT design tokens + helpers. Ported from the "Games Launcher.dc.html" design:
// dark glass surfaces, a dynamic per-game accent, and per-store brand colors.
// Singleton so every view shares the same palette + helper functions.
pragma Singleton
import QtQuick

QtObject {
    id: theme

    // ---- surfaces / text (rgba to fake the design's glass over the ambient) ----
    readonly property color canvasTop:   "#12161c"   // ambient wash top
    readonly property color canvasBottom: "#06080b"   // ambient wash bottom
    readonly property color shell:        "#080b10"   // window body under the glass
    readonly property color titlebar:     Qt.rgba(0.066, 0.082, 0.105, 0.72)
    readonly property color sidebar:      Qt.rgba(0.051, 0.066, 0.090, 0.55)
    readonly property color glass:        Qt.rgba(0.070, 0.090, 0.121, 0.72)
    readonly property color glassSoft:    Qt.rgba(0.070, 0.090, 0.121, 0.55)
    readonly property color input:        Qt.rgba(0.070, 0.090, 0.121, 0.80)

    readonly property color text:   "#e6ebf2"
    readonly property color muted:  Qt.rgba(1, 1, 1, 0.55)
    readonly property color faint:  Qt.rgba(1, 1, 1, 0.42)
    readonly property color ghost:  Qt.rgba(1, 1, 1, 0.30)
    readonly property color hairline:    Qt.rgba(1, 1, 1, 0.07)
    readonly property color hairlineSoft: Qt.rgba(1, 1, 1, 0.05)
    readonly property color fill:   Qt.rgba(1, 1, 1, 0.06)
    readonly property color fillHover: Qt.rgba(1, 1, 1, 0.12)

    readonly property color good:  "#3fae4f"
    readonly property color bad:   "#e23b3b"
    readonly property color badSoft: "#ff6b6b"

    // ---- accent (dynamic; driven per active game) ----
    readonly property string accentDefault:   "#7c5cff"
    readonly property string accentDefaultFg:  "#ffffff"

    // radii
    readonly property int rSm: 7
    readonly property int rMd: 10
    readonly property int rLg: 13

    // type — matches the design (Space Grotesk headings, Manrope body).
    readonly property string fontDisplay: "Space Grotesk"
    readonly property string fontBody:    "Manrope"
    readonly property string fontArabic:  "Cairo"

    // Per-game glow palette. Real covers don't carry a color, so we derive a stable
    // accent from the game id (hash) — same vivid, moody range as the design.
    readonly property var glowPalette: [
        "#7c5cff", "#ff7a3d", "#ff3d9a", "#3fd27a", "#3da5ff", "#8aa0c0",
        "#c0a0ff", "#2fd0d0", "#4d6dff", "#b06dff", "#ff9a3d", "#5db0ff"
    ]

    // Stable index from a game id (works for real Steam appids and synthetic ids).
    function glowIndex(id) {
        var n = Number(id);
        if (!isFinite(n)) n = 0;
        // fold the (possibly huge) id into the palette range
        var h = Math.abs(Math.floor(n % 1000000)) ^ Math.abs(Math.floor(n / 1000000) % 1000000);
        return h % glowPalette.length;
    }
    function accentFor(id)  { return glowPalette[glowIndex(id)]; }

    // Two cover-placeholder colors + glow, derived from the accent — gives each
    // tile the design's radial gradient look before/without real art.
    function coverTop(id)    { return Qt.darker(accentFor(id), 2.6); }
    function coverBottom(id) { return Qt.rgba(0.03, 0.04, 0.07, 1); }

    // Readable foreground for a solid color (design's _fg: luminance threshold).
    function fgOn(hex) {
        var c = Qt.color(hex);
        var lum = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;   // 0..1
        return lum > 0.588 ? "#0a0a0c" : "#ffffff";
    }

    // Per-store brand identity, keyed by the model's store name ("Steam"/"Epic"/…).
    readonly property var storeMeta: ({
        "Steam": { key: "steam", name: "Steam",      short: "STEAM",     color: "#2a7fd4", fg: "#ffffff" },
        "Epic":  { key: "epic",  name: "Epic Games", short: "EPIC",      color: "#d9d9e0", fg: "#0a0a0c" },
        "GOG":   { key: "gog",   name: "GOG",        short: "GOG",       color: "#9b4dde", fg: "#ffffff" },
        "Xbox":  { key: "xbox",  name: "Game Pass",  short: "GAME PASS", color: "#3fae4f", fg: "#ffffff" }
    })
    function store(name) {
        return storeMeta[name] || { key: "?", name: name, short: name, color: "#9aa0a6", fg: "#ffffff" }
    }
}
