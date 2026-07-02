// CINEMA library: a full-bleed hero of the last-played (or random) game with
// instant Play, then horizontal shelves — Continue playing + one per store.
// Everything shown is real: hero art via requestHero, chips only where data
// exists, shelves are live filters over the scanned model.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Orbit

Item {
    id: root

    // ---- hero pick ------------------------------------------------------------
    // "last": the most recently played game (Steam localconfig / ORBIT history);
    // "random": any game, re-rolled per scan. Falls back A->Z when nothing has
    // been played yet; null when the library is empty.
    GameFilter { id: recentGames; sourceModel: backend.allGames; sortMode: "recent"; playedOnly: true }
    GameFilter { id: anyGames; sourceModel: backend.allGames; sortMode: "az" }

    property var heroGame: null
    property string heroUrl: ""

    function recastHero() {
        var g = null;
        if (backend.heroMode === "random" && anyGames.count > 0)
            g = anyGames.gameAt(Math.floor(Math.random() * anyGames.count));
        else if (recentGames.count > 0)
            g = recentGames.gameAt(0);
        else if (anyGames.count > 0)
            g = anyGames.gameAt(0);
        if (g && heroGame && g.appid === heroGame.appid) return;
        heroGame = g;
        heroUrl = "";
        if (g) backend.requestHero(g.appid);
    }
    Connections {
        target: backend
        function onStateChanged() { root.recastHero() }
        function onHeroModeChanged() { root.heroGame = null; root.recastHero() }
        function onHeroReady(appid, dataUrl) {
            if (root.heroGame && appid === root.heroGame.appid && dataUrl.length)
                root.heroUrl = dataUrl
        }
    }
    Component.onCompleted: recastHero()

    readonly property var heroStore: heroGame ? Theme.store(heroGame.store) : null

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 30
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: content
            width: root.width
            spacing: 0

            // ================= hero =================
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(480, Math.max(330, Math.round(root.height * 0.55)))
                visible: root.heroGame !== null

                // art (or the per-game gradient while/if there's none)
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0
                            color: root.heroGame ? Qt.darker(Theme.accentFor(root.heroGame.appid), 1.7) : Theme.bg }
                        GradientStop { position: 1.0; color: Theme.bg }
                    }
                }
                Image {
                    anchors.fill: parent
                    source: root.heroUrl
                    fillMode: Image.PreserveAspectCrop
                    verticalAlignment: Image.AlignTop
                    asynchronous: true
                    visible: root.heroUrl.length > 0
                }
                // fades: bottom into the page, left for legibility (mock .fade)
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0.039, 0.047, 0.067, 0.25) }
                        GradientStop { position: 0.35; color: Qt.rgba(0.039, 0.047, 0.067, 0.05) }
                        GradientStop { position: 0.88; color: Qt.rgba(0.039, 0.047, 0.067, 0.92) }
                        GradientStop { position: 1.0; color: Theme.bg }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0.039, 0.047, 0.067, 0.85) }
                        GradientStop { position: 0.55; color: "transparent" }
                    }
                }

                // info block
                ColumnLayout {
                    anchors.left: parent.left; anchors.leftMargin: 44
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 30
                    width: Math.min(640, parent.width - 88)
                    spacing: 0

                    Label {
                        text: root.heroGame
                            ? (root.heroStore.short + (root.heroGame.store === "Steam" && root.heroGame.mapped
                                   ? " · " + root.heroGame.accountName.toUpperCase()
                                   : " · " + qsTr("READY TO PLAY")))
                            : ""
                        color: root.heroStore ? root.heroStore.color : Theme.muted
                        font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
                        font.letterSpacing: 2.2
                    }
                    Label {
                        Layout.topMargin: 10
                        Layout.fillWidth: true
                        text: root.heroGame ? root.heroGame.name : ""
                        color: Theme.text
                        font.family: Theme.fontDisplay; font.pixelSize: 42; font.weight: Font.Bold
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                    Row {
                        Layout.topMargin: 12
                        spacing: 8
                        Chip { text: root.heroGame && root.heroGame.fullyInstalled
                                   ? qsTr("Installed") : qsTr("Not fully installed")
                               fillColor: root.heroGame && root.heroGame.fullyInstalled ? Theme.store("Xbox").color : "" }
                        Chip { visible: root.heroGame !== null && root.heroGame.store !== "Steam"
                               text: root.heroGame ? qsTr("Launches via %1").arg(root.heroStore.name) : "" }
                        Chip { visible: root.heroGame !== null && root.heroGame.playtime > 0
                               text: root.heroGame ? Theme.hoursLabel(root.heroGame.playtime) + qsTr(" played") : "" }
                        Chip { visible: root.heroGame !== null && root.heroGame.lastPlayed > 0
                               text: root.heroGame ? qsTr("Last played %1").arg(Theme.relTime(root.heroGame.lastPlayed)) : "" }
                    }
                    Row {
                        Layout.topMargin: 20
                        spacing: 12
                        // Play now
                        Rectangle {
                            width: playRow.implicitWidth + 52; height: 46; radius: 23
                            color: playHover.containsMouse ? "#e8ecf5" : "#ffffff"
                            opacity: backend.launching ? 0.5 : 1
                            Row {
                                id: playRow
                                anchors.centerIn: parent; spacing: 10
                                Label { text: "▶"; color: "#0b0d12"; font.pixelSize: 13
                                    anchors.verticalCenter: parent.verticalCenter }
                                Label { text: qsTr("Play now"); color: "#0b0d12"
                                    font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.ExtraBold
                                    anchors.verticalCenter: parent.verticalCenter }
                            }
                            MouseArea { id: playHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: !backend.launching
                                onClicked: backend.play(root.heroGame.appid, AppState.offline) }
                        }
                        // Play offline (Steam only)
                        Rectangle {
                            visible: root.heroGame !== null && root.heroGame.store === "Steam"
                            width: offLabel.implicitWidth + 40; height: 46; radius: 23
                            color: offHover.containsMouse ? Theme.fillHover : Theme.fill
                            border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.22)
                            opacity: backend.launching ? 0.5 : 1
                            Label { id: offLabel; anchors.centerIn: parent; text: qsTr("Play offline")
                                color: Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: offHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: !backend.launching
                                onClicked: backend.play(root.heroGame.appid, true) }
                        }
                        // Details ghost
                        Rectangle {
                            width: detLabel.implicitWidth + 40; height: 46; radius: 23
                            color: detHover.containsMouse ? Theme.fillHover : "transparent"
                            border.width: 1; border.color: Theme.line
                            Label { id: detLabel; anchors.centerIn: parent; text: qsTr("Details")
                                color: Theme.muted
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: detHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: AppState.open(root.heroGame) }
                        }
                    }
                    Label {
                        Layout.topMargin: 12
                        text: root.heroGame === null ? ""
                            : root.heroGame.store !== "Steam"
                              ? qsTr("No account switch needed — launches straight through %1.").arg(root.heroStore.name)
                              : root.heroGame.mapped
                                ? qsTr("Play switches Steam to %1 and signs in — no manual account swapping.").arg(root.heroGame.accountName)
                                : qsTr("This game isn't mapped to an account yet — open Details to pin one.")
                        color: Theme.faint
                        font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                    }
                }
            }

            // spacing under the top bar when there's no hero (empty library)
            Item { Layout.preferredHeight: 90; visible: root.heroGame === null }

            // ================= shelves =================
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 44; Layout.rightMargin: 44
                Layout.topMargin: 6
                spacing: 22

                Shelf {
                    Layout.fillWidth: true
                    title: qsTr("Continue playing")
                    model: recentGames
                }

                Repeater {
                    model: backend.stores
                    delegate: Shelf {
                        Layout.fillWidth: true
                        title: modelData.name
                        dotColor: Theme.store(modelData.storeName).color
                        model: storeFilter
                        property var storeFilter: GameFilter {
                            sourceModel: backend.allGames
                            storeFilter: modelData.storeName
                            sortMode: "az"
                        }
                    }
                }
            }

            // ================= empty state =================
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 120
                spacing: 8
                visible: anyGames.count === 0 && !backend.scanning
                Label { Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No games installed")
                    color: Theme.text; font.family: Theme.fontDisplay; font.pixelSize: 18; font.weight: Font.Bold }
                Label { Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 360
                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                    text: qsTr("Install a game in Steam, Epic, GOG or Game Pass and it will show up here.")
                    color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 13 }
            }
        }
    }
}
