// Settings view — status panel (mirrors the web Settings tab's read-only state).
// Minimal for now; API-key entry / language select land in a later polish pass.

import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root

    Column {
        anchors.left: parent.left; anchors.top: parent.top; anchors.right: parent.right
        anchors.leftMargin: 24; anchors.rightMargin: 24; anchors.topMargin: 8
        width: Math.min(parent.width - 48, 640)
        spacing: 10

        Label { text: qsTr("STATUS"); color: Theme.primarySoft
            font.family: Theme.fontMono; font.pixelSize: 11; font.letterSpacing: 1; font.bold: true }

        Rectangle {
            width: parent.width; radius: Theme.rLg; color: Theme.surface
            border.width: 1; border.color: Theme.outline
            height: col.height + 28
            Column {
                id: col
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 14
                spacing: 0
                SettingsRow { label: qsTr("Current account"); value: backend.currentAccount.length ? backend.currentAccount : qsTr("None") }
                SettingsRow { label: qsTr("Installed games"); value: backend.gameCount + "" }
                SettingsRow { label: qsTr("Accounts on this PC"); value: backend.accounts.length + "" }
                SettingsRow { label: qsTr("Engine"); value: "C++ / Qt 6" }
            }
        }

        Label {
            width: parent.width
            text: qsTr("Tip: if every account is your own, Steam Families may remove the need to switch at all.")
            color: Theme.faint; font.pixelSize: 12; font.italic: true; wrapMode: Text.WordWrap
        }
    }

    component SettingsRow: Item {
        property string label: ""
        property string value: ""
        width: col.width; height: 34
        Label { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            text: label; color: Theme.muted; font.pixelSize: 14 }
        Label { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            text: value; color: Theme.text; font.pixelSize: 14 }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1
            color: Theme.outline; opacity: 0.4 }
    }
}
