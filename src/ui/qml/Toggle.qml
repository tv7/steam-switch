// Pill toggle switch, styled to the ORBIT design. `on` is the state; emits
// toggled() on click (the parent owns the value).
import QtQuick
import Orbit

Item {
    id: root
    property bool on: false
    property color accent: Theme.accent
    property color accentFg: Theme.accentFg
    signal toggled()

    implicitWidth: 42
    implicitHeight: 24

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.on ? root.accent : Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        border.color: root.on ? "transparent" : Qt.rgba(1, 1, 1, 0.12)
        Behavior on color { ColorAnimation { duration: 160 } }

        Rectangle {
            id: knob
            width: 18; height: 18; radius: 9
            y: 2
            x: root.on ? parent.width - width - 2 : 2
            color: root.on ? root.accentFg : "#e9e7f0"
            Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled()
    }
}
