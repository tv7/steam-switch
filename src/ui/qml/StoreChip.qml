// Rounded filter pill (store / account filters on the library screen). When
// active it fills with either the store's brand color or the app accent.
import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root
    property string label: ""
    property bool active: false
    property string activeColor: ""     // brand color for store chips; "" = use accent
    signal clicked()

    implicitWidth: row.implicitWidth + 30
    implicitHeight: 33

    readonly property string fillColor: active
        ? (activeColor.length ? activeColor : AppState.accent)
        : ""
    readonly property string fgColor: active
        ? (activeColor.length ? Theme.fgOn(activeColor) : AppState.accentFg)
        : Qt.rgba(1, 1, 1, 0.66)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.active ? root.fillColor : Qt.rgba(1, 1, 1, 0.045)
        border.width: 1
        border.color: root.active ? "transparent" : Qt.rgba(1, 1, 1, 0.09)
        Behavior on color { ColorAnimation { duration: 120 } }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 7
            Label {
                text: root.label
                color: root.fgColor
                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold
            }
        }
        MouseArea { anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
    }
}
