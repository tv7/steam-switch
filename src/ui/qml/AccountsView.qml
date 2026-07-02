// CINEMA accounts & stores (design/m1-accounts.html): Steam multi-account cards
// (live badge, readiness, game count, Switch now / View library), an Add-account
// card (the validated restart-to-login flow), auto-detected store rows, and a
// rescan line with the real last-scan time.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Orbit

Flickable {
    id: root
    contentHeight: col.implicitHeight + 100
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: AppScrollBar {}

    component Badge: Rectangle {
        property string text: ""
        property color tint: Theme.muted
        implicitWidth: bl.implicitWidth + 20; implicitHeight: 24
        radius: 12
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.14)
        border.width: 1; border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.35)
        Label { id: bl; anchors.centerIn: parent; text: parent.text
            color: Qt.lighter(parent.tint, 1.25)
            font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.ExtraBold }
    }

    ColumnLayout {
        id: col
        x: 44; y: 92
        width: root.width - 88
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: qsTr("Accounts & stores"); color: Theme.text
                    font.family: Theme.fontDisplay; font.pixelSize: 30; font.weight: Font.Bold }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Steam switches between accounts; other stores are detected from what's installed.")
                    color: Theme.muted; font.family: Theme.fontBody; font.pixelSize: 13
                    font.weight: Font.DemiBold; wrapMode: Text.WordWrap
                }
            }
            // Re-run setup (re-opens the first-run onboarding)
            Rectangle {
                Layout.alignment: Qt.AlignTop
                implicitWidth: rerunLabel.implicitWidth + 30; implicitHeight: 36; radius: 12
                color: rerunHover.containsMouse ? Theme.fill : "transparent"
                border.width: 1; border.color: Theme.line
                Label { id: rerunLabel; anchors.centerIn: parent; text: qsTr("Re-run setup")
                    color: rerunHover.containsMouse ? Theme.text : Theme.muted
                    font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold }
                MouseArea { id: rerunHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor; onClicked: AppState.startOnboarding() }
            }
        }

        // ---- STEAM — MULTI-ACCOUNT ----
        Row {
            Layout.topMargin: 26; Layout.bottomMargin: 14
            spacing: 10
            Rectangle { width: 8; height: 8; radius: 4; color: Theme.store("Steam").color
                anchors.verticalCenter: parent.verticalCenter }
            Label { text: qsTr("STEAM — MULTI-ACCOUNT"); color: Theme.faint
                font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
                font.letterSpacing: 2 }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 16

            Repeater {
                model: backend.accounts
                delegate: Rectangle {
                    required property var modelData
                    width: 300
                    height: card.implicitHeight + 40
                    radius: 18
                    color: Theme.panel
                    border.width: 1
                    border.color: modelData.loggedIn ? Qt.rgba(0.29, 0.87, 0.5, 0.4) : Theme.line

                    ColumnLayout {
                        id: card
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 20
                        spacing: 0

                        Rectangle {
                            Layout.preferredWidth: 52; Layout.preferredHeight: 52
                            radius: 26
                            gradient: Gradient {
                                GradientStop { position: 0; color: modelData.color }
                                GradientStop { position: 1; color: Qt.darker(modelData.color, 1.8) }
                            }
                            Label { anchors.centerIn: parent
                                text: modelData.personaName.charAt(0).toUpperCase()
                                color: Theme.fgOn(modelData.color)
                                font.family: Theme.fontDisplay; font.pixelSize: 20; font.weight: Font.ExtraBold }
                        }
                        Label {
                            Layout.topMargin: 14
                            text: modelData.personaName; color: Theme.text
                            font.family: Theme.fontDisplay; font.pixelSize: 17; font.weight: Font.Bold
                        }
                        Label {
                            Layout.topMargin: 2
                            text: modelData.accountName + " · " + qsTr("%n game(s)", "", modelData.gameCount)
                            color: Theme.faint
                            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                        }
                        Row {
                            Layout.topMargin: 12
                            spacing: 8
                            Badge { visible: modelData.loggedIn; text: qsTr("● LOGGED IN"); tint: Theme.good }
                            Badge { visible: modelData.ready; text: qsTr("✓ Ready to switch")
                                tint: Theme.store("Steam").color }
                            Badge { visible: !modelData.ready; text: qsTr("⚠ Needs login"); tint: Theme.accent }
                        }
                        // action: switch now (or just "logged in" state)
                        Rectangle {
                            Layout.topMargin: 16
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 12
                            property bool primary: !modelData.loggedIn && modelData.ready
                            color: primary ? (actHover.containsMouse ? "#e8ecf5" : "#ffffff")
                                           : (actHover.containsMouse ? Theme.fill : "transparent")
                            border.width: primary ? 0 : 1; border.color: Theme.line
                            opacity: backend.launching && primary ? 0.5 : 1
                            Label {
                                anchors.centerIn: parent
                                text: modelData.loggedIn ? qsTr("View library ›")
                                    : modelData.ready ? qsTr("Switch now")
                                    : qsTr("Sign in once via Steam")
                                color: parent.primary ? "#0b0d12" : Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 12
                                font.weight: parent.primary ? Font.ExtraBold : Font.Bold
                            }
                            MouseArea {
                                id: actHover
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: !backend.launching
                                onClicked: {
                                    if (modelData.loggedIn) AppState.go("library");
                                    else if (modelData.ready) backend.switchTo(modelData.steamid64);
                                    else backend.addAccount();
                                }
                            }
                        }
                    }
                }
            }

            // add-account card
            Rectangle {
                width: 300
                height: 220
                radius: 18
                color: "transparent"
                border.width: 1
                border.color: addHover.containsMouse ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.16)
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10
                    width: parent.width - 40
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 44; Layout.preferredHeight: 44
                        radius: 22; color: Theme.fill
                        Label { anchors.centerIn: parent; text: "＋"; color: Theme.muted; font.pixelSize: 20 }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Add another Steam account")
                        color: Theme.muted
                        font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Steam restarts to its sign-in screen. Log in once with “Remember me” and ORBIT can switch to it from then on.")
                        color: Theme.faint
                        font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
                MouseArea { id: addHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor; onClicked: backend.addAccount() }
            }
        }

        // ---- OTHER STORES — AUTO-DETECTED ----
        Label {
            Layout.topMargin: 30; Layout.bottomMargin: 14
            text: qsTr("OTHER STORES — AUTO-DETECTED"); color: Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
            font.letterSpacing: 2
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Repeater {
                model: backend.stores
                delegate: Rectangle {
                    required property var modelData
                    visible: !modelData.isSteam
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: Theme.rLg
                    color: Theme.panel
                    border.width: 1; border.color: Theme.line
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 16; spacing: 14
                        Rectangle {
                            Layout.preferredWidth: 38; Layout.preferredHeight: 38
                            radius: 12
                            property color bc: Theme.store(modelData.storeName).color
                            color: Qt.rgba(bc.r, bc.g, bc.b, 0.14)
                            Label { anchors.centerIn: parent
                                text: modelData.name.charAt(0)
                                color: parent.bc
                                font.family: Theme.fontDisplay; font.pixelSize: 15; font.weight: Font.ExtraBold }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Label { text: modelData.name; color: Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.ExtraBold }
                            Label { text: modelData.connected ? qsTr("Detected on this PC") : qsTr("Nothing installed found")
                                color: Theme.faint
                                font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold }
                        }
                        ColumnLayout {
                            spacing: 2
                            Label { text: modelData.connected ? qsTr("● CONNECTED") : qsTr("○ NOT FOUND")
                                color: modelData.connected ? Theme.good : Theme.faint
                                Layout.alignment: Qt.AlignRight
                                font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.ExtraBold }
                            Label { text: qsTr("%n game(s)", "", modelData.count)
                                color: Theme.faint
                                Layout.alignment: Qt.AlignRight
                                font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold }
                        }
                    }
                }
            }
        }

        // ---- rescan line ----
        Rectangle {
            Layout.topMargin: 26
            implicitWidth: rescanRow.implicitWidth + 30; implicitHeight: 36
            radius: 12
            color: rescanHover.containsMouse ? Theme.fill : "transparent"
            border.width: 1; border.color: Theme.line
            Row {
                id: rescanRow
                anchors.centerIn: parent
                spacing: 8
                Label { text: "⟳"; color: Theme.muted; font.pixelSize: 14
                    anchors.verticalCenter: parent.verticalCenter
                    RotationAnimation on rotation {
                        running: backend.scanning; loops: Animation.Infinite
                        from: 0; to: 360; duration: 900 } }
                Label {
                    text: backend.scanning ? qsTr("Scanning…")
                        : backend.lastScanTime > 0
                          ? qsTr("Rescan — last scan %1").arg(Theme.relTime(backend.lastScanTime))
                          : qsTr("Rescan")
                    color: Theme.muted
                    font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea { id: rescanHover; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor; onClicked: backend.refresh() }
        }
    }
}
