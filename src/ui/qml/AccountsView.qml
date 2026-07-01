// Accounts screen: a card per store showing its brand mark, name, sync status and
// game count, with a Manage button that opens the side panel. Mirrors the design's
// "connect your stores" grid — here "connected" means the store has installed games.
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

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Label { text: qsTr("Accounts"); color: Theme.text
                    font.family: Theme.fontDisplay; font.pixelSize: 27; font.weight: Font.Bold }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Your stores are detected from what's installed on this PC. Steam also supports switching between multiple accounts.")
                    color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 13; wrapMode: Text.WordWrap
                }
            }
            // Re-run setup (re-opens the first-run onboarding)
            Rectangle {
                Layout.alignment: Qt.AlignTop
                implicitWidth: rerunRow.implicitWidth + 30; implicitHeight: 36; radius: 9
                color: rerunHover.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.14)
                RowLayout {
                    id: rerunRow; anchors.centerIn: parent; spacing: 8
                    Canvas { Layout.preferredWidth: 14; Layout.preferredHeight: 14
                        property color stroke: rerunHover.containsMouse ? "#fff" : Theme.muted
                        onStrokeChanged: requestPaint()
                        onPaint: { var c = getContext("2d"); c.reset(); c.strokeStyle = stroke;
                            c.lineWidth = 1.8; c.lineCap = "round";
                            c.beginPath(); c.arc(7, 7, 5, Math.PI*1.15, Math.PI*0.15); c.stroke();
                            c.beginPath(); c.arc(7, 7, 5, Math.PI*0.15+Math.PI, Math.PI*1.15+Math.PI); c.stroke();
                            c.beginPath(); c.moveTo(11.6, 2.6); c.lineTo(12, 5.6); c.lineTo(9, 5.2); c.stroke(); } }
                    Label { text: qsTr("Re-run setup"); color: rerunHover.containsMouse ? "#fff" : Theme.muted
                        font.family: Theme.fontBody; font.pixelSize: 12.5; font.weight: Font.DemiBold }
                }
                MouseArea { id: rerunHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor; onClicked: AppState.startOnboarding() }
            }
        }
        Item { Layout.preferredHeight: 26 }

        GridLayout {
            Layout.fillWidth: true
            columns: 2; rowSpacing: 12; columnSpacing: 12

            Repeater {
                model: backend.stores
                delegate: Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 74; radius: 13
                    color: Theme.glassSoft
                    border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.07)
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 17; spacing: 13
                        Rectangle { Layout.preferredWidth: 40; Layout.preferredHeight: 40
                            radius: 10; color: modelData.color
                            Label { anchors.centerIn: parent; text: modelData.shortName.charAt(0)
                                color: modelData.textColor; font.family: Theme.fontDisplay
                                font.pixelSize: 18; font.weight: Font.Bold } }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Label { text: modelData.name; color: Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.Bold }
                            Label {
                                text: modelData.connected
                                    ? "● " + modelData.count + qsTr(" games synced")
                                    : qsTr("○ Not connected")
                                color: modelData.connected ? Theme.good : Theme.faint
                                font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Medium
                            }
                        }
                        Rectangle {
                            implicitWidth: manageLabel.implicitWidth + 28; implicitHeight: 32
                            radius: 8
                            color: manageHover.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                            border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.12)
                            Label { id: manageLabel; anchors.centerIn: parent; text: qsTr("Manage")
                                color: Theme.muted; font.family: Theme.fontBody
                                font.pixelSize: 12; font.weight: Font.DemiBold }
                            MouseArea { id: manageHover; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: AppState.manageKey = modelData.key }
                        }
                    }
                }
            }
        }
    }
}
