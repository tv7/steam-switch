// CINEMA backdrop: the flat near-black canvas with two very soft color glows
// (the mocks' blurred .glow blobs — approximated with radial gradients, no
// GraphicalEffects dependency).
import QtQuick
import Orbit

Item {
    id: root

    Rectangle { anchors.fill: parent; color: Theme.bg }

    // cool glow, top-left (accounts/settings mocks)
    Rectangle {
        width: parent.width * 0.7; height: width
        x: -width * 0.35; y: -width * 0.45
        radius: width / 2
        opacity: 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0.07, 0.145, 0.235, 0.55) }
            GradientStop { position: 0.5; color: Qt.rgba(0.07, 0.145, 0.235, 0.18) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    // warm amber glow, bottom-right (settings mock)
    Rectangle {
        width: parent.width * 0.55; height: width
        x: parent.width - width * 0.55; y: parent.height - width * 0.5
        radius: width / 2
        opacity: 0.4
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0.20, 0.137, 0.055, 0.6) }
            GradientStop { position: 0.5; color: Qt.rgba(0.20, 0.137, 0.055, 0.2) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
