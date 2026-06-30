// Sidebar nav row (icon + label), with hover/active states. Ported from .nav-item.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Rectangle {
    id: root
    property string text: ""
    property string glyph: ""
    property bool active: false
    signal clicked()

    Layout.fillWidth: true
    height: 40; radius: Theme.rMd
    color: active ? Theme.surfaceHigh : (hover.containsMouse ? Theme.surfaceHigh : "transparent")

    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 12
        Label { text: root.glyph; color: active ? "#fff" : Theme.muted; font.pixelSize: 16 }
        Label { text: root.text; color: active ? "#fff" : Theme.muted; font.pixelSize: 15; Layout.fillWidth: true }
    }
    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; onClicked: root.clicked() }
}
