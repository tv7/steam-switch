// Settings screen: preferences card. Only real settings are wired — the offline
// launch default and the interface language (persisted + drives RTL). The design's
// startup/sync toggles aren't backed by anything here, so they're omitted.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Flickable {
    id: root
    contentHeight: col.implicitHeight + 78
    clip: true
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: col
        x: 32; width: Math.min(parent.width - 64, 820)
        y: 28
        spacing: 0

        Label { text: qsTr("Settings"); color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 27; font.weight: Font.Bold }
        Label { text: qsTr("PREFERENCES"); color: Theme.faint
            Layout.topMargin: 26; Layout.bottomMargin: 13
            font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.Bold; font.letterSpacing: 1.4 }

        Rectangle {
            Layout.fillWidth: true
            radius: 13; color: Theme.glassSoft
            border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.07)
            implicitHeight: prefs.implicitHeight
            clip: true
            ColumnLayout {
                id: prefs
                anchors.left: parent.left; anchors.right: parent.right
                spacing: 0

                // launch offline by default
                Item {
                    Layout.fillWidth: true; implicitHeight: 68
                    ColumnLayout {
                        anchors.left: parent.left; anchors.leftMargin: 18
                        anchors.verticalCenter: parent.verticalCenter; spacing: 3
                        Label { text: qsTr("Launch offline by default"); color: Theme.text
                            font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Label { text: qsTr("New launches start Steam offline (shared-account friendly)")
                            color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Medium }
                    }
                    Toggle {
                        anchors.right: parent.right; anchors.rightMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        on: AppState.offline; onToggled: AppState.offline = !AppState.offline
                    }
                    MouseArea { anchors.fill: parent; onClicked: AppState.offline = !AppState.offline }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.05) }

                // language
                RowLayout {
                    Layout.fillWidth: true; Layout.margins: 18; spacing: 24
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 3
                        Label { text: qsTr("Language"); color: Theme.text
                            font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Label { text: qsTr("Display language for the interface")
                            color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Medium }
                    }
                    Rectangle {
                        Layout.preferredWidth: 210; Layout.preferredHeight: 42
                        radius: 9; color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.09)
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 4; spacing: 4
                            Repeater {
                                model: [ { code: "en", label: "English" }, { code: "ar", label: "العربية" } ]
                                delegate: Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    radius: 7
                                    property bool active: backend.language === modelData.code
                                    color: active ? AppState.accent : "transparent"
                                    Label { anchors.centerIn: parent; text: modelData.label
                                        color: active ? AppState.accentFg : Theme.muted
                                        font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.setLanguage(modelData.code) }
                                }
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.topMargin: 20; Layout.fillWidth: true
            text: qsTr("Tip: if every account is your own, Steam Families may remove the need to switch at all.")
            color: Theme.ghost; font.family: Theme.fontBody; font.pixelSize: 12; font.italic: true
            wrapMode: Text.WordWrap
        }
    }
}
