// Accounts hub: a card per account (avatar, name, login, readiness badge) + an
// "Add New Account" dashed card. Ported from .acct-grid / .acct-card / .acct-add.

import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root

    Column {
        anchors.fill: parent
        anchors.leftMargin: 24; anchors.rightMargin: 24; anchors.bottomMargin: 8
        spacing: 12

        Label { text: qsTr("ACCOUNTS"); color: Theme.muted
            font.family: Theme.fontMono; font.pixelSize: 12; font.letterSpacing: 1; font.bold: true }

        GridView {
            width: parent.width
            height: parent.height - 36
            cellWidth: 320; cellHeight: 124
            clip: true
            model: backend.accounts

            delegate: Rectangle {
                width: 304; height: 108; radius: Theme.rLg
                color: Theme.surface
                border.color: modelData.loggedIn ? Theme.primarySoft : Theme.outline
                border.width: modelData.loggedIn ? 2 : 1

                Row {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Rectangle {
                        width: 46; height: 46; radius: Theme.rMd; color: modelData.color
                        Label { anchors.centerIn: parent; text: modelData.personaName.charAt(0).toUpperCase()
                            color: "#0b1326"; font.bold: true; font.pixelSize: 18 }
                    }
                    Column {
                        width: parent.width - 58; spacing: 3
                        Label { text: modelData.personaName; color: "#fff"; font.bold: true; font.pixelSize: 15
                            elide: Text.ElideRight; width: parent.width }
                        Label { text: modelData.accountName; color: Theme.faint; font.pixelSize: 12
                            elide: Text.ElideRight; width: parent.width }
                        Label {
                            text: modelData.loggedIn ? qsTr("● LOGGED IN")
                                  : (modelData.ready ? qsTr("✓ ready") : qsTr("⚠ needs login"))
                            color: modelData.loggedIn ? Theme.good
                                   : (modelData.ready ? Theme.muted : Theme.warn)
                            font.pixelSize: 12; font.bold: true
                        }
                    }
                }
            }

            footer: Rectangle {
                width: 304; height: 108; radius: Theme.rLg
                color: addHover.containsMouse ? Theme.surface : "transparent"
                border.color: addHover.containsMouse ? Theme.primary : Theme.outline
                border.width: 1
                Column {
                    anchors.centerIn: parent; spacing: 4
                    Label { text: "＋"; color: Theme.primarySoft; font.pixelSize: 26
                        anchors.horizontalCenter: parent.horizontalCenter }
                    Label { text: qsTr("Add New Account"); color: Theme.text; font.bold: true; font.pixelSize: 15
                        anchors.horizontalCenter: parent.horizontalCenter }
                }
                MouseArea { id: addHover; anchors.fill: parent; hoverEnabled: true
                    onClicked: backend.addAccount() }
            }
        }
    }
}
