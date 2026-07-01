// The moody, accent-tinted backdrop behind the whole shell. The design blurs the
// active game's cover; we approximate that dependency-free with a dark vertical
// wash plus an accent glow from the top that shifts with the current accent.
import QtQuick
import SteamSwitch

Item {
    id: root
    property string accent: AppState.accent

    // base canvas: top slightly lifted toward the accent, fading to near-black
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.canvasTop }
            GradientStop { position: 0.55; color: Qt.darker(Theme.canvasTop, 1.9) }
            GradientStop { position: 1.0; color: Theme.canvasBottom }
        }
    }

    // accent glow washing down from the top ~half of the window
    Rectangle {
        id: glow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.62
        property color acc: Qt.color(root.accent)
        Behavior on acc { ColorAnimation { duration: 700; easing.type: Easing.OutCubic } }
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(glow.acc.r, glow.acc.g, glow.acc.b, 0.30) }
            GradientStop { position: 0.5; color: Qt.rgba(glow.acc.r, glow.acc.g, glow.acc.b, 0.10) }
            GradientStop { position: 1.0; color: Qt.rgba(glow.acc.r, glow.acc.g, glow.acc.b, 0.0) }
        }
    }

    // subtle darkening scrim for legibility over the glow
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0.031, 0.043, 0.063, 0.28) }
            GradientStop { position: 0.72; color: Qt.rgba(0.031, 0.043, 0.063, 0.60) }
            GradientStop { position: 1.0; color: Qt.rgba(0.031, 0.043, 0.063, 0.82) }
        }
    }
}
