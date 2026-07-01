// Game detail screen: big cover with Play now (+ offline) and install status on the
// left, store/title/owner and truthful stat cards on the right. Fields are real
// (no fabricated ratings/descriptions): store, owning account, install status, id.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Item {
    id: root
    property var game: AppState.selected
    readonly property var storeMeta: game ? Theme.store(game.store) : Theme.store("Steam")
    property string coverUrl: ""

    onGameChanged: { coverUrl = ""; if (game) backend.requestCover(game.appid) }
    Component.onCompleted: if (game) backend.requestCover(game.appid)
    Connections {
        target: backend
        function onCoverReady(appid, dataUrl) {
            if (root.game && appid === root.game.appid && dataUrl.length) root.coverUrl = dataUrl
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + 74
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: content
            x: 32; width: parent.width - 64
            y: 24
            spacing: 0

            // back
            Rectangle {
                Layout.bottomMargin: 22
                implicitWidth: backRow.implicitWidth + 22; implicitHeight: 34
                radius: 8
                color: backHover.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.1)
                RowLayout {
                    id: backRow
                    anchors.centerIn: parent; spacing: 8
                    Canvas { Layout.preferredWidth: 15; Layout.preferredHeight: 15
                        property color stroke: backHover.containsMouse ? "#fff" : Theme.muted
                        onStrokeChanged: requestPaint()
                        onPaint: { var c = getContext("2d"); c.reset(); c.strokeStyle = stroke;
                            c.lineWidth = 2; c.lineCap = "round"; c.lineJoin = "round";
                            c.beginPath(); c.moveTo(9,3); c.lineTo(5,7.5); c.lineTo(9,12); c.stroke(); } }
                    Label { text: qsTr("Back"); color: backHover.containsMouse ? "#fff" : Theme.muted
                        font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
                }
                MouseArea { id: backHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor; onClicked: AppState.back() }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 36

                // ---- left: cover + actions ----
                ColumnLayout {
                    Layout.preferredWidth: 380
                    Layout.alignment: Qt.AlignTop
                    spacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 4 / 3
                        radius: 14; clip: true
                        color: "transparent"
                        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.08)
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: Qt.lighter(AppState.accent, 1.1) }
                                GradientStop { position: 0.6; color: Qt.darker(AppState.accent, 2.2) }
                                GradientStop { position: 1.0; color: Theme.coverBottom(root.game ? root.game.appid : 0) }
                            }
                        }
                        Image { anchors.fill: parent; source: root.coverUrl
                            fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                            visible: root.coverUrl.length > 0 }
                        Rectangle {   // store badge
                            x: 12; y: 12; radius: 6
                            width: dBadge.implicitWidth + 16; height: dBadge.implicitHeight + 6
                            color: root.storeMeta.color
                            Label { id: dBadge; anchors.centerIn: parent; text: root.storeMeta.short
                                color: root.storeMeta.fg; font.family: Theme.fontBody
                                font.pixelSize: 9; font.weight: Font.Bold; font.letterSpacing: 0.7 }
                        }
                    }

                    // Play now (+ offline)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 46; radius: 10
                            color: backend.launching ? Theme.bad : AppState.accent
                            transform: Translate { y: playHover.containsMouse ? -2 : 0
                                Behavior on y { NumberAnimation { duration: 120 } } }
                            RowLayout {
                                anchors.centerIn: parent; spacing: 8
                                Canvas { Layout.preferredWidth: 15; Layout.preferredHeight: 15
                                    property color fg: backend.launching ? "#fff" : AppState.accentFg
                                    onFgChanged: requestPaint()
                                    onPaint: { var c = getContext("2d"); c.reset(); c.fillStyle = fg;
                                        if (backend.launching) { c.fillRect(3,3,9,9); }
                                        else { c.beginPath(); c.moveTo(4,3); c.lineTo(13,7.5); c.lineTo(4,12);
                                               c.closePath(); c.fill(); } } }
                                Label { text: backend.launching ? qsTr("Stop") : qsTr("Play now")
                                    color: backend.launching ? "#fff" : AppState.accentFg
                                    font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.Bold }
                            }
                            MouseArea { id: playHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend.launching) backend.cancel();
                                    else if (root.game) backend.play(root.game.appid, AppState.offline);
                                } }
                        }
                        // offline quick-toggle
                        Rectangle {
                            Layout.preferredWidth: 54; Layout.preferredHeight: 46
                            radius: 10
                            property color acc: Qt.color(AppState.accent)
                            color: AppState.offline ? Qt.rgba(acc.r, acc.g, acc.b, 0.18)
                                                     : Qt.rgba(1, 1, 1, 0.07)
                            border.width: 1
                            border.color: AppState.offline ? AppState.accent : Qt.rgba(1, 1, 1, 0.12)
                            Label { anchors.centerIn: parent; text: qsTr("OFF")
                                color: AppState.offline ? AppState.accent : Theme.muted
                                font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.Bold
                                font.letterSpacing: 0.6 }
                            MouseArea { anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: AppState.offline = !AppState.offline
                                ToolTip.visible: containsMouse; ToolTip.text: qsTr("Launch offline (Steam)") }
                        }
                    }

                    // install status
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 60; radius: 12
                        color: Theme.glassSoft
                        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.07)
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("Install status"); color: Theme.muted
                                    font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Label { text: root.game && root.game.fullyInstalled
                                            ? qsTr("Installed") : qsTr("Partial")
                                    color: Theme.text; font.family: Theme.fontBody
                                    font.pixelSize: 12; font.weight: Font.Bold }
                            }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: 6; radius: 3
                                color: Qt.rgba(1, 1, 1, 0.1)
                                Rectangle {
                                    height: parent.height; radius: 3
                                    width: parent.width * (root.game && root.game.fullyInstalled ? 1.0 : 0.5)
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: AppState.accent }
                                        GradientStop { position: 1.0; color: root.storeMeta.color }
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- right: metadata ----
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 0

                    Rectangle {
                        Layout.preferredWidth: sName.implicitWidth + 24; implicitHeight: 26
                        radius: 7; color: root.storeMeta.color
                        Label { id: sName; anchors.centerIn: parent; text: root.storeMeta.name
                            color: root.storeMeta.fg; font.family: Theme.fontBody
                            font.pixelSize: 11; font.weight: Font.Bold; font.letterSpacing: 0.5 }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 14; Layout.bottomMargin: 10
                        text: root.game ? root.game.name : ""
                        color: Theme.text
                        font.family: Theme.fontDisplay; font.pixelSize: 40; font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.bottomMargin: 26
                        spacing: 8
                        Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
                            Layout.alignment: Qt.AlignVCenter
                            color: root.game ? root.game.accountColor : Theme.muted }
                        Label { text: root.game ? root.game.accountName : ""
                            color: Theme.muted; font.family: Theme.fontBody
                            font.pixelSize: 13; font.weight: Font.DemiBold }
                    }

                    Label { text: qsTr("How it launches"); color: Qt.rgba(1, 1, 1, 0.85)
                        font.family: Theme.fontDisplay; font.pixelSize: 13; font.weight: Font.Bold
                        Layout.bottomMargin: 9 }
                    Label {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 28
                        text: root.launchExplainer()
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.family: Theme.fontBody; font.pixelSize: 15; lineHeight: 1.5
                        wrapMode: Text.WordWrap
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2; rowSpacing: 12; columnSpacing: 12
                        StatCard { label: qsTr("STORE"); value: root.storeMeta.name }
                        StatCard { label: qsTr("ACCOUNT")
                            value: root.game ? root.game.accountName : "—" }
                        StatCard { label: qsTr("STATUS")
                            value: root.game && root.game.fullyInstalled ? qsTr("Installed") : qsTr("Partial") }
                        StatCard { label: qsTr("APP ID")
                            value: root.game ? String(root.game.appid) : "—" }
                    }
                }
            }
        }
    }

    function launchExplainer() {
        if (!game) return "";
        if (game.store === "Steam") {
            if (!game.mapped)
                return qsTr("This game isn't mapped to a Steam account yet, so it can't be launched by switching.");
            return AppState.offline
                ? qsTr("Launching switches Steam to “%1”, brings it up offline, then starts the game — so a shared account's cloud saves aren't touched.").arg(game.accountName)
                : qsTr("Launching switches Steam to “%1”, waits for sign-in, then starts the game — no manual account swapping.").arg(game.accountName);
        }
        return qsTr("Launches straight through %1 — no account switching needed.").arg(storeMeta.name);
    }

    // small stat tile
    component StatCard: Rectangle {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        implicitHeight: 62; radius: 11
        color: Theme.glassSoft
        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.07)
        Column {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 5
            Label { text: label; color: Theme.faint
                font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.Medium }
            Label { text: value; color: Theme.text; width: parent.width; elide: Text.ElideRight
                font.family: Theme.fontDisplay; font.pixelSize: 18; font.weight: Font.Bold }
        }
    }
}
