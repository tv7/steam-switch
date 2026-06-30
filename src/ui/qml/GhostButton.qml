// Ghost button (icon + label) matching .btn.ghost: surface fill, 1px outline,
// hover lighten, dimmed when disabled.
import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root
    property string glyph: ""
    property string text: ""
    property bool enabled: true
    signal clicked()

    implicitWidth: box.width
    implicitHeight: 36

    Rectangle {
        id: box
        width: row.width + 28; height: 36; radius: Theme.rMd
        color: !root.enabled ? Theme.dim : (m.containsMouse ? Theme.surfaceHigh : Theme.surface)
        border.width: 1; border.color: root.enabled ? Theme.outline : "transparent"
        Row {
            id: row; anchors.centerIn: parent; spacing: 7
            Label { text: root.glyph; color: root.enabled ? Theme.text : Theme.faint; font.pixelSize: 14 }
            Label { text: root.text; color: root.enabled ? Theme.text : Theme.faint
                font.pixelSize: 13; font.bold: true }
        }
        MouseArea { id: m; anchors.fill: parent; hoverEnabled: true
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (root.enabled) root.clicked() }
    }
}
