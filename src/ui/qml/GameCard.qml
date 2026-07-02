// Shelf tile (CINEMA): a 3:4 cover (real art over a per-game gradient
// placeholder) with the owning account pinned to the cover foot, then title +
// a truthful sub line (store · playtime/last-played — only when real data
// exists). Click opens the detail hero; right-click launches directly.
import QtQuick
import QtQuick.Controls
import Orbit

Item {
    id: card
    property string coverUrl: ""
    property bool coverRequested: false

    readonly property var storeMeta: Theme.store(model.store)
    readonly property string placeholderAccent: Theme.accentFor(model.appid)

    implicitWidth: 132
    implicitHeight: width * 4 / 3 + 44

    // Everything AppState.open / play needs, copied off the model row.
    function gameObject() {
        return { appid: model.appid, name: model.name, store: model.store,
                 accountId: model.accountId, accountName: model.accountName,
                 accountColor: model.accountColor, mapped: model.mapped,
                 fullyInstalled: model.fullyInstalled,
                 playtime: model.playtime, lastPlayed: model.lastPlayed }
    }

    Component.onCompleted: if (!coverRequested) { coverRequested = true; backend.requestCover(model.appid) }
    Connections {
        target: backend
        function onCoverReady(appid, dataUrl) {
            if (appid === model.appid && dataUrl.length) card.coverUrl = dataUrl
        }
    }

    Column {
        anchors.fill: parent
        spacing: 7

        // ---- cover ----
        Rectangle {
            id: frame
            width: parent.width
            height: width * 4 / 3
            radius: Theme.rMd
            clip: true
            color: "transparent"
            border.width: 1
            border.color: hover.containsMouse ? Theme.accent : Theme.line
            transform: Translate { y: hover.containsMouse ? -4 : 0
                Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } } }

            // gradient placeholder (also the backdrop for games w/o art)
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.darker(card.placeholderAccent, 1.6) }
                    GradientStop { position: 1.0; color: Theme.coverBottom(model.appid) }
                }
                Label {
                    anchors.centerIn: parent
                    visible: card.coverUrl.length === 0
                    text: model.name.charAt(0)
                    color: Qt.lighter(card.placeholderAccent, 1.5)
                    font.family: Theme.fontDisplay; font.pixelSize: 30; font.weight: Font.Bold
                }
            }

            Image {
                anchors.fill: parent
                source: card.coverUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true; cache: true
                visible: card.coverUrl.length > 0
            }

            // owner foot: gradient scrim + dot + name (the mock's .own strip)
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 34
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0.02, 0.024, 0.035, 0.88) }
                }
                Row {
                    anchors.left: parent.left; anchors.leftMargin: 8
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 6
                    spacing: 5
                    Rectangle { width: 6; height: 6; radius: 3; color: model.accountColor
                        anchors.verticalCenter: parent.verticalCenter }
                    Label { text: model.accountName; color: "#cdd4e2"
                        font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.Bold
                        width: frame.width - 24; elide: Text.ElideRight }
                }
            }

            // dim if not fully installed
            Rectangle { anchors.fill: parent; radius: Theme.rMd; color: "#80000000"
                visible: !model.fullyInstalled }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) ctx.popup()
                    else AppState.open(card.gameObject())
                }
            }
            Menu {
                id: ctx
                MenuItem { text: qsTr("Play"); onTriggered: backend.play(model.appid, AppState.offline) }
                MenuItem { text: qsTr("Play offline"); onTriggered: backend.play(model.appid, true) }
                MenuItem { text: qsTr("Details"); onTriggered: AppState.open(card.gameObject()) }
            }
        }

        // ---- title + truthful sub line ----
        Label {
            width: parent.width
            text: model.name
            color: Theme.text
            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold
            elide: Text.ElideRight
        }
        Label {
            width: parent.width
            text: {
                var bits = [card.storeMeta.name];
                if (model.playtime > 0) bits.push(Theme.hoursLabel(model.playtime));
                else if (model.lastPlayed > 0) bits.push(Theme.relTime(model.lastPlayed));
                return bits.join(" · ");
            }
            color: Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }
}
