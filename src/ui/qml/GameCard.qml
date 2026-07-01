// Library tile: a 3:4 cover (real art over a per-game gradient placeholder), a
// store badge, then title + owner line beneath. Hover lifts + accent-outlines the
// cover. Click opens the detail screen; right-click launches directly.
import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: card
    property string coverUrl: ""
    property bool coverRequested: false

    readonly property var storeMeta: Theme.store(model.store)
    readonly property string accent: Theme.accentFor(model.appid)

    Component.onCompleted: if (!coverRequested) { coverRequested = true; backend.requestCover(model.appid) }
    Connections {
        target: backend
        function onCoverReady(appid, dataUrl) {
            if (appid === model.appid && dataUrl.length) card.coverUrl = dataUrl
        }
    }

    Column {
        anchors.fill: parent
        spacing: 10

        // ---- cover ----
        Item {
            width: parent.width
            height: width * 4 / 3

            Rectangle {
                id: frame
                anchors.fill: parent
                radius: 11
                clip: true
                color: "transparent"
                border.width: 1
                border.color: hover.containsMouse ? card.accent : Qt.rgba(1, 1, 1, 0.07)
                transform: Translate { y: hover.containsMouse ? -5 : 0
                    Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } } }

                // gradient placeholder (also the backdrop for non-Steam games w/o art)
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.lighter(card.accent, 1.15) }
                        GradientStop { position: 0.55; color: Qt.darker(card.accent, 2.2) }
                        GradientStop { position: 1.0; color: Theme.coverBottom(model.appid) }
                    }
                }

                Image {
                    anchors.fill: parent
                    source: card.coverUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true; cache: true
                    visible: card.coverUrl.length > 0
                }

                // bottom scrim
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: parent.height * 0.45
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
                    }
                }

                // store badge (top-left)
                Rectangle {
                    x: 11; y: 11
                    radius: 6
                    width: badgeLabel.implicitWidth + 16; height: badgeLabel.implicitHeight + 6
                    color: card.storeMeta.color
                    Label {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: card.storeMeta.short
                        color: card.storeMeta.fg
                        font.family: Theme.fontBody; font.pixelSize: 9; font.weight: Font.Bold
                        font.letterSpacing: 0.7
                    }
                }

                // dim if not fully installed
                Rectangle { anchors.fill: parent; radius: 11; color: "#80000000"
                    visible: !model.fullyInstalled }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) ctx.popup()
                        else AppState.open({ appid: model.appid, name: model.name, store: model.store,
                            accountName: model.accountName, accountColor: model.accountColor,
                            mapped: model.mapped, fullyInstalled: model.fullyInstalled })
                    }
                }
                Menu {
                    id: ctx
                    MenuItem { text: qsTr("Play"); onTriggered: backend.play(model.appid, AppState.offline) }
                    MenuItem { text: qsTr("Play offline"); onTriggered: backend.play(model.appid, true) }
                    MenuItem { text: qsTr("Details"); onTriggered: AppState.open({ appid: model.appid,
                        name: model.name, store: model.store, accountName: model.accountName,
                        accountColor: model.accountColor, mapped: model.mapped,
                        fullyInstalled: model.fullyInstalled }) }
                }
            }
        }

        // ---- title + owner ----
        Label {
            width: parent.width
            text: model.name
            color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 14; font.weight: Font.Bold
            elide: Text.ElideRight
        }
        Row {
            width: parent.width; spacing: 6
            Rectangle { width: 7; height: 7; radius: 3.5; color: model.accountColor
                anchors.verticalCenter: parent.verticalCenter }
            Label {
                text: model.accountName
                color: Theme.faint
                font.family: Theme.fontBody; font.pixelSize: 11.5; font.weight: Font.Medium
                elide: Text.ElideRight; width: parent.width - 13
            }
        }
    }
}
