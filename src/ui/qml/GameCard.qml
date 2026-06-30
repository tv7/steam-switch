// Game tile: 2:3 cover with hover-lift + blue outline, gradient account badge at
// the bottom (swatch + name), dimmed if not fully installed. Ported from .card.

import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: card
    property bool offline: false
    property string coverUrl: ""
    property bool coverRequested: false

    Component.onCompleted: if (!coverRequested) { coverRequested = true; backend.requestCover(model.appid) }

    Connections {
        target: backend
        function onCoverReady(appid, dataUrl) {
            if (appid === model.appid && dataUrl.length) card.coverUrl = dataUrl
        }
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.rLg
        color: Theme.surface
        border.width: hover.containsMouse ? 2 : 1
        border.color: hover.containsMouse ? Theme.primary : Theme.outline
        clip: true
        // hover-lift (transform avoids fighting anchors.fill)
        transform: Translate { y: hover.containsMouse ? -3 : 0
            Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } } }

        // placeholder (name) until cover loads
        Label {
            anchors.centerIn: parent; width: parent.width - 24
            visible: card.coverUrl.length === 0
            text: model.name; color: Theme.muted; font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        }

        Image {
            anchors.fill: parent
            source: card.coverUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: card.coverUrl.length > 0
        }

        // bottom gradient + account badge
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 52
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "#ec080c12" }
            }
            Row {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.bottomMargin: 9
                spacing: 7
                Rectangle { width: 8; height: 8; radius: 4; color: model.accountColor
                    anchors.verticalCenter: parent.verticalCenter }
                Label { text: model.accountName; color: "#fff"; font.pixelSize: 12; font.bold: true
                    elide: Text.ElideRight; width: parent.width - 22 }
            }
        }

        Rectangle { anchors.fill: parent; radius: Theme.rLg; color: "#80000000"; visible: !model.fullyInstalled }

        MouseArea {
            id: hover
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) ctx.popup()
                else backend.play(model.appid, card.offline)
            }
        }

        Menu {
            id: ctx
            MenuItem { text: qsTr("Launch"); onTriggered: backend.play(model.appid, false) }
            MenuItem { text: qsTr("Launch offline"); onTriggered: backend.play(model.appid, true) }
        }
    }
}
