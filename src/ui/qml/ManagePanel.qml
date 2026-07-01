// Slide-in Manage panel for a store (opened from the Accounts screen). Shows the
// store's sync status + a "Rescan" action; for Steam it also lists the accounts on
// this PC (with readiness / logged-in state) and an "Add another Steam account"
// action. Only real capabilities are wired — no fabricated connect/disconnect.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Item {
    id: root
    anchors.fill: parent
    visible: AppState.manageKey !== ""

    // find the store card for the open key
    readonly property var store: {
        var list = backend.stores;
        for (var i = 0; i < list.length; i++) if (list[i].key === AppState.manageKey) return list[i];
        return null;
    }
    readonly property bool isSteam: store && store.isSteam

    // scrim
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.016, 0.027, 0.043, 0.6)
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        MouseArea { anchors.fill: parent; onClicked: AppState.manageKey = "" }
    }

    // panel
    Rectangle {
        id: panel
        width: 392
        anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.right: parent.right
        color: Qt.rgba(0.055, 0.070, 0.094, 0.97)
        transform: Translate { x: root.visible ? 0 : panel.width
            Behavior on x { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } } }

        Rectangle { width: 1; height: parent.height; color: Qt.rgba(1, 1, 1, 0.1) }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // header
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 16
                Layout.topMargin: 22; Layout.bottomMargin: 18
                spacing: 13
                Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 11
                    color: root.store ? root.store.color : Theme.muted
                    Label { anchors.centerIn: parent; text: root.store ? root.store.shortName.charAt(0) : "?"
                        color: root.store ? root.store.textColor : "#fff"
                        font.family: Theme.fontDisplay; font.pixelSize: 18; font.weight: Font.Bold } }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Label { text: root.store ? root.store.name : ""
                        color: Theme.text; font.family: Theme.fontDisplay
                        font.pixelSize: 17; font.weight: Font.Bold }
                    Label { text: qsTr("Manage connection"); color: Theme.faint
                        font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Medium }
                }
                Rectangle {
                    Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 8
                    color: closeHover.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    Label { anchors.centerIn: parent; text: "✕"
                        color: closeHover.containsMouse ? "#fff" : Theme.muted; font.pixelSize: 13 }
                    MouseArea { id: closeHover; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor; onClicked: AppState.manageKey = "" }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.07) }

            Flickable {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentHeight: body.implicitHeight + 44
                clip: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: body
                    x: 22; width: parent.width - 44; y: 20
                    spacing: 0

                    // status rows
                    Rectangle {
                        Layout.fillWidth: true; Layout.bottomMargin: 22
                        implicitHeight: statusCol.implicitHeight
                        radius: 12; color: Qt.rgba(1, 1, 1, 0.04)
                        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.08)
                        ColumnLayout {
                            id: statusCol
                            anchors.left: parent.left; anchors.right: parent.right
                            spacing: 0
                            StatusRow { label: qsTr("Games synced")
                                value: root.store ? String(root.store.count) : "0" }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.06) }
                            StatusRow { label: qsTr("Status")
                                value: root.store && root.store.connected ? qsTr("● Connected") : qsTr("○ No games")
                                valueColor: root.store && root.store.connected ? Theme.good : Theme.faint }
                        }
                    }

                    // rescan action
                    Rectangle {
                        Layout.fillWidth: true; Layout.bottomMargin: 22
                        implicitHeight: 44; radius: 10
                        color: syncHover.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.14)
                        Label { anchors.centerIn: parent
                            text: backend.scanning ? qsTr("Rescanning…") : qsTr("Rescan library")
                            color: Theme.text; font.family: Theme.fontBody
                            font.pixelSize: 13; font.weight: Font.DemiBold }
                        MouseArea { id: syncHover; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor; onClicked: backend.refresh() }
                    }

                    // ---- Steam-only: multiple accounts ----
                    RowLayout {
                        visible: root.isSteam
                        Layout.bottomMargin: 12; spacing: 8
                        Label { text: qsTr("STEAM ACCOUNTS"); color: Theme.faint
                            font.family: Theme.fontBody; font.pixelSize: 10
                            font.weight: Font.Bold; font.letterSpacing: 1.3 }
                        Rectangle { implicitWidth: multiLabel.implicitWidth + 14; implicitHeight: 18
                            radius: 5; color: Qt.rgba(Qt.color(AppState.accent).r, Qt.color(AppState.accent).g,
                                                      Qt.color(AppState.accent).b, 0.12)
                            Label { id: multiLabel; anchors.centerIn: parent; text: qsTr("MULTI-ACCOUNT")
                                color: AppState.accent; font.family: Theme.fontBody
                                font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 0.7 } }
                    }

                    Repeater {
                        model: root.isSteam ? backend.accounts : []
                        delegate: Rectangle {
                            Layout.fillWidth: true; Layout.bottomMargin: 8
                            implicitHeight: 56; radius: 11
                            color: modelData.loggedIn ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                            border.width: 1
                            border.color: modelData.loggedIn ? AppState.accent : Qt.rgba(1, 1, 1, 0.09)
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                                spacing: 12
                                Rectangle { Layout.preferredWidth: 34; Layout.preferredHeight: 34
                                    radius: 9; color: modelData.color
                                    Label { anchors.centerIn: parent
                                        text: (modelData.personaName || "S").charAt(0).toUpperCase()
                                        color: "#0b1326"; font.family: Theme.fontDisplay
                                        font.pixelSize: 13; font.weight: Font.Bold } }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 1
                                    Label { text: modelData.personaName; color: Theme.text
                                        font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold
                                        elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label {
                                        text: modelData.loggedIn ? qsTr("Active now")
                                              : (modelData.ready ? qsTr("Ready to switch") : qsTr("Needs sign-in"))
                                        color: modelData.loggedIn ? AppState.accent
                                               : (modelData.ready ? Theme.faint : Theme.bad)
                                        font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold
                                    }
                                }
                                Label { text: modelData.gameCount + qsTr(" games"); color: Theme.ghost
                                    font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.Medium }
                            }
                        }
                    }

                    // add account
                    Rectangle {
                        visible: root.isSteam
                        Layout.fillWidth: true; Layout.topMargin: 4
                        implicitHeight: 44; radius: 10
                        color: "transparent"
                        border.width: 1; border.color: addHover.containsMouse ? AppState.accent : Qt.rgba(1, 1, 1, 0.2)
                        RowLayout {
                            anchors.centerIn: parent; spacing: 8
                            Label { text: "＋"; color: addHover.containsMouse ? "#fff" : Theme.muted
                                font.pixelSize: 15 }
                            Label { text: qsTr("Add another Steam account")
                                color: addHover.containsMouse ? "#fff" : Qt.rgba(1, 1, 1, 0.7)
                                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
                        }
                        MouseArea { id: addHover; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor; onClicked: backend.addAccount() }
                    }
                }
            }
        }
    }

    component StatusRow: Item {
        property string label: ""
        property string value: ""
        property color valueColor: Theme.text
        Layout.fillWidth: true
        implicitHeight: 44
        Label { anchors.left: parent.left; anchors.leftMargin: 15; anchors.verticalCenter: parent.verticalCenter
            text: label; color: Theme.muted; font.family: Theme.fontBody
            font.pixelSize: 13; font.weight: Font.Medium }
        Label { anchors.right: parent.right; anchors.rightMargin: 15; anchors.verticalCenter: parent.verticalCenter
            text: value; color: valueColor; font.family: Theme.fontBody
            font.pixelSize: 13; font.weight: Font.Bold }
    }
}
