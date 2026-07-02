// CINEMA game detail = hero takeover (design/m1-detail.html): full-bleed hero
// art, back pill, store/account kicker, truthful chips, an explainer of what
// Play will actually do, Play/Offline/Pin actions, a stat card, and a "More on
// <account>" shelf. No fabricated ratings/genres/descriptions — real data only.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Orbit

Item {
    id: root

    readonly property var game: AppState.selected
    readonly property var storeMeta: game ? Theme.store(game.store) : Theme.store("")
    readonly property color storeColor: storeMeta.color
    property string heroUrl: ""

    // The account card matching this game's owner (live readiness badge data).
    readonly property var ownerCard: {
        if (!game || !game.accountId) return null;
        for (var i = 0; i < backend.accounts.length; ++i)
            if (backend.accounts[i].steamid64 === game.accountId) return backend.accounts[i];
        return null;
    }

    onGameChanged: { heroUrl = ""; if (game) backend.requestHero(game.appid) }
    Component.onCompleted: if (game) backend.requestHero(game.appid)
    Connections {
        target: backend
        function onHeroReady(appid, dataUrl) {
            if (root.game && appid === root.game.appid && dataUrl.length) root.heroUrl = dataUrl
        }
    }

    // "More on <account>" (Steam) / "More from <store>" (other stores).
    GameFilter {
        id: moreGames
        sourceModel: backend.allGames
        accountFilter: root.game && root.game.store === "Steam" && root.game.accountId
                       ? root.game.accountId : "all"
        storeFilter: root.game && root.game.store !== "Steam" ? root.game.store : "all"
        sortMode: "recent"
    }

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
                Layout.preferredHeight: Math.min(560, Math.max(380, Math.round(root.height * 0.68)))

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0
                            color: root.game ? Qt.darker(Theme.accentFor(root.game.appid), 1.7) : Theme.bg }
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
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0.039, 0.047, 0.067, 0.30) }
                        GradientStop { position: 0.30; color: Qt.rgba(0.039, 0.047, 0.067, 0.05) }
                        GradientStop { position: 0.86; color: Qt.rgba(0.039, 0.047, 0.067, 0.94) }
                        GradientStop { position: 1.0; color: Theme.bg }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0.039, 0.047, 0.067, 0.88) }
                        GradientStop { position: 0.58; color: "transparent" }
                    }
                }

                // ---- info (bottom-left) ----
                ColumnLayout {
                    anchors.left: parent.left; anchors.leftMargin: 44
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 34
                    width: Math.min(700, parent.width - (statCard.visible ? 380 : 88))
                    spacing: 0

                    // back pill
                    Rectangle {
                        Layout.preferredWidth: backRow.implicitWidth + 28
                        Layout.preferredHeight: 32
                        radius: 16
                        color: backHover.containsMouse ? Theme.fillHover : Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1; border.color: Theme.line
                        Row {
                            id: backRow; anchors.centerIn: parent; spacing: 8
                            Label { text: "‹"; color: Theme.muted; font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter }
                            Label { text: qsTr("Back to library"); color: Theme.muted
                                font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea { id: backHover; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor; onClicked: AppState.back() }
                    }

                    // kicker: STORE · ACCOUNT
                    Row {
                        Layout.topMargin: 16
                        spacing: 9
                        Rectangle { width: 8; height: 8; radius: 4; color: root.storeColor
                            anchors.verticalCenter: parent.verticalCenter }
                        Label { text: root.storeMeta.short
                            color: root.storeColor
                            font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
                            font.letterSpacing: 2.2
                            anchors.verticalCenter: parent.verticalCenter }
                        Rectangle { visible: root.game !== null && root.game.store === "Steam"
                            width: 8; height: 8; radius: 4
                            color: root.game ? root.game.accountColor : "transparent"
                            anchors.verticalCenter: parent.verticalCenter }
                        Label { visible: root.game !== null && root.game.store === "Steam"
                            text: root.game ? root.game.accountName.toUpperCase() : ""
                            color: Theme.muted
                            font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
                            font.letterSpacing: 2.2
                            anchors.verticalCenter: parent.verticalCenter }
                    }

                    Label {
                        Layout.topMargin: 10
                        Layout.fillWidth: true
                        text: root.game ? root.game.name : ""
                        color: Theme.text
                        font.family: Theme.fontDisplay; font.pixelSize: 40; font.weight: Font.Bold
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }

                    Row {
                        Layout.topMargin: 12
                        spacing: 8
                        Chip { text: root.game && root.game.fullyInstalled
                                   ? qsTr("Installed") : qsTr("Not fully installed")
                               fillColor: root.game && root.game.fullyInstalled ? String(root.storeColor) : "" }
                        Chip { visible: root.ownerCard !== null && root.ownerCard.ready === true
                               amber: true
                               text: qsTr("✓ Account ready — no sign-in needed") }
                        Chip { visible: root.ownerCard !== null && root.ownerCard.ready === false
                               text: qsTr("⚠ Needs one manual sign-in") }
                        Chip { visible: root.game !== null && root.game.playtime > 0
                               text: root.game ? Theme.hoursLabel(root.game.playtime) + qsTr(" played") : "" }
                        Chip { visible: root.game !== null && root.game.lastPlayed > 0
                               text: root.game ? qsTr("Last played %1").arg(Theme.relTime(root.game.lastPlayed)) : "" }
                    }

                    // flow explainer: what Play will actually do
                    Rectangle {
                        Layout.topMargin: 14
                        Layout.preferredWidth: Math.min(560, flowLabel.implicitWidth + 30)
                        Layout.maximumWidth: 560
                        implicitHeight: flowLabel.implicitHeight + 24
                        radius: 13
                        color: Qt.rgba(root.storeColor.r, root.storeColor.g, root.storeColor.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(root.storeColor.r, root.storeColor.g, root.storeColor.b, 0.22)
                        Label {
                            id: flowLabel
                            anchors.fill: parent; anchors.margins: 12
                            text: root.game === null ? ""
                                : root.game.store !== "Steam"
                                  ? qsTr("Launches straight through %1 — no account switch involved.").arg(root.storeMeta.name)
                                  : root.game.mapped
                                    ? qsTr("Play switches Steam to %1, waits for sign-in, then launches the game — no manual account swapping. Steam will close and restart.").arg(root.game.accountName)
                                    : qsTr("No owning account found for this game yet. Pin it to an account and Play will switch to it.")
                            color: Qt.lighter(root.storeColor, 1.35)
                            font.family: Theme.fontBody; font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    Row {
                        Layout.topMargin: 18
                        spacing: 12
                        Rectangle {
                            width: playRow.implicitWidth + 56; height: 48; radius: 24
                            color: playHover.containsMouse ? "#e8ecf5" : "#ffffff"
                            opacity: backend.launching ? 0.5 : 1
                            Row {
                                id: playRow
                                anchors.centerIn: parent; spacing: 10
                                Label { text: backend.launching ? "…" : "▶"; color: "#0b0d12"; font.pixelSize: 13
                                    anchors.verticalCenter: parent.verticalCenter }
                                Label { text: backend.launching ? qsTr("Launching…") : qsTr("Play now")
                                    color: "#0b0d12"
                                    font.family: Theme.fontBody; font.pixelSize: 15; font.weight: Font.ExtraBold
                                    anchors.verticalCenter: parent.verticalCenter }
                            }
                            MouseArea { id: playHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: !backend.launching
                                onClicked: backend.play(root.game.appid, AppState.offline) }
                        }
                        Rectangle {
                            visible: root.game !== null && root.game.store === "Steam" && !backend.launching
                            width: offLabel.implicitWidth + 40; height: 48; radius: 24
                            color: offHover.containsMouse ? Theme.fillHover : Theme.fill
                            border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.22)
                            Label { id: offLabel; anchors.centerIn: parent; text: qsTr("Launch offline")
                                color: Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: offHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.play(root.game.appid, true) }
                        }
                        // cancel while launching
                        Rectangle {
                            visible: backend.launching
                            width: cancelLabel.implicitWidth + 40; height: 48; radius: 24
                            color: cancelHover.containsMouse ? Theme.bad : Theme.fill
                            border.width: 1; border.color: Theme.bad
                            Label { id: cancelLabel; anchors.centerIn: parent; text: qsTr("Stop launch")
                                color: cancelHover.containsMouse ? "#fff" : Theme.bad
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: cancelHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor; onClicked: backend.cancel() }
                        }
                        // pin to account (Steam only)
                        Rectangle {
                            visible: root.game !== null && root.game.store === "Steam" && !backend.launching
                            width: pinLabel.implicitWidth + 40; height: 48; radius: 24
                            color: pinHover.containsMouse ? Theme.fillHover : "transparent"
                            border.width: 1; border.color: Theme.line
                            Label { id: pinLabel; anchors.centerIn: parent; text: qsTr("Pin to account…")
                                color: Theme.muted
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: pinHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor; onClicked: pinMenu.popup() }
                            Menu {
                                id: pinMenu
                                Instantiator {
                                    model: backend.accounts
                                    delegate: MenuItem {
                                        required property var modelData
                                        text: modelData.personaName + " (" + modelData.accountName + ")"
                                        onTriggered: backend.pinToAccount(root.game.appid, modelData.steamid64)
                                    }
                                    onObjectAdded: (index, object) => pinMenu.insertItem(index, object)
                                    onObjectRemoved: (index, object) => pinMenu.removeItem(object)
                                }
                            }
                        }
                    }
                }

                // ---- stat card (bottom-right) ----
                Rectangle {
                    id: statCard
                    anchors.right: parent.right; anchors.rightMargin: 44
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 34
                    width: 280
                    height: statsCol.implicitHeight
                    radius: Theme.rLg
                    color: Qt.rgba(0.047, 0.055, 0.078, 0.72)
                    border.width: 1; border.color: Theme.line
                    visible: root.width > 980
                    ColumnLayout {
                        id: statsCol
                        anchors.left: parent.left; anchors.right: parent.right
                        spacing: 0
                        StatRow { k: qsTr("STORE"); v: root.storeMeta.name; vColor: root.storeColor }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                        StatRow { visible: root.game !== null && root.game.store === "Steam"
                            k: qsTr("ACCOUNT")
                            v: root.game ? (root.game.mapped
                                  ? root.game.accountName + (root.ownerCard && root.ownerCard.ready ? qsTr(" · ready") : "")
                                  : qsTr("Unmapped")) : ""
                            vColor: root.game && root.game.mapped ? Theme.warn : Theme.faint }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line
                            visible: root.game !== null && root.game.store === "Steam" }
                        StatRow { k: qsTr("STATUS")
                            v: root.game && root.game.fullyInstalled ? qsTr("Fully installed") : qsTr("Installing / partial") }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line
                            visible: root.game !== null && root.game.store === "Steam" }
                        StatRow { visible: root.game !== null && root.game.store === "Steam"
                            k: qsTr("APP ID"); v: root.game ? String(root.game.appid) : "" }
                    }
                }
            }

            // ================= "More on <account>" shelf =================
            Shelf {
                Layout.fillWidth: true
                Layout.leftMargin: 44; Layout.rightMargin: 44
                Layout.topMargin: 22
                cardWidth: 126
                title: root.game === null ? ""
                     : root.game.store === "Steam" && root.game.mapped
                       ? qsTr("More on %1").arg(root.game.accountName)
                       : qsTr("More from %1").arg(root.storeMeta.name)
                model: moreGames
            }
        }
    }

    component StatRow: RowLayout {
        property string k: ""
        property string v: ""
        property color vColor: Theme.text
        Layout.fillWidth: true
        Layout.leftMargin: 16; Layout.rightMargin: 16
        Layout.topMargin: 11; Layout.bottomMargin: 11
        Label { text: parent.k; color: Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.ExtraBold
            font.letterSpacing: 1 }
        Item { Layout.fillWidth: true }
        Label { text: parent.v; color: parent.vColor
            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold }
    }
}
