// Proton-Dark palette + metrics, ported 1:1 from web/style.css :root. Singleton so
// every view shares the same tokens.
pragma Singleton
import QtQuick

QtObject {
    // colors
    readonly property color bg:          "#0b1326"   // background canvas
    readonly property color panel:       "#131b2e"   // sidebar / topbar / footer
    readonly property color surface:     "#171f33"   // cards
    readonly property color surfaceHigh: "#222a3d"   // hover / active
    readonly property color base:        "#060e20"   // inputs
    readonly property color primary:     "#4d8eff"
    readonly property color primarySoft: "#adc6ff"
    readonly property color primaryHover:"#6ba0ff"
    readonly property color text:        "#dae2fd"
    readonly property color muted:       "#c2c6d6"
    readonly property color faint:       "#8c909f"
    readonly property color outline:     "#424754"
    readonly property color dim:         "#2d3449"
    readonly property color good:        "#10b981"
    readonly property color warn:        "#d9a441"
    readonly property color bad:         "#ef4444"

    // radii
    readonly property int rSm: 4
    readonly property int rMd: 8
    readonly property int rLg: 12

    // type
    readonly property string fontUi:   "Hanken Grotesk"   // falls back to Segoe UI if absent
    readonly property string fontMono: "Geist"
}
