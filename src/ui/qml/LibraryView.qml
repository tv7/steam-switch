// The home / library screen: search + refresh + offline toggle, the "Your library"
// heading with a live count, store & account filter chip rows, and the cover grid.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28; anchors.rightMargin: 28
        anchors.topMargin: 24; anchors.bottomMargin: 0
        spacing: 0

        // ---- top bar: search + refresh + offline ----
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 22
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 44
                radius: 10
                color: Theme.input
                border.width: 1
                border.color: search.activeFocus ? AppState.accent : Qt.rgba(1, 1, 1, 0.1)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14; anchors.rightMargin: 14
                    spacing: 10
                    Canvas {
                        Layout.preferredWidth: 16; Layout.preferredHeight: 16
                        onPaint: { var c = getContext("2d"); c.reset();
                            c.strokeStyle = Qt.rgba(1,1,1,0.42); c.lineWidth = 2; c.lineCap = "round";
                            c.beginPath(); c.arc(6.8, 6.8, 4.6, 0, Math.PI*2); c.stroke();
                            c.beginPath(); c.moveTo(10.6, 10.6); c.lineTo(14.5, 14.5); c.stroke(); }
                    }
                    TextField {
                        id: search
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search your library")
                        color: Theme.text
                        placeholderTextColor: Theme.faint
                        font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.Medium
                        background: null
                        onTextChanged: backend.setSearch(text)
                    }
                }
            }

            // refresh
            Rectangle {
                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                radius: 10
                color: refreshHover.containsMouse ? Qt.rgba(0.11, 0.13, 0.17, 0.9) : Theme.input
                border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.1)
                Canvas {
                    id: refreshIcon
                    anchors.centerIn: parent
                    width: 18; height: 18
                    property color stroke: refreshHover.containsMouse ? "#fff" : Theme.muted
                    onStrokeChanged: requestPaint()
                    onPaint: { var c = getContext("2d"); c.reset();
                        c.strokeStyle = stroke; c.lineWidth = 1.9; c.lineCap = "round";
                        c.beginPath(); c.arc(9, 9, 6.4, Math.PI*1.15, Math.PI*0.15); c.stroke();
                        c.beginPath(); c.arc(9, 9, 6.4, Math.PI*0.15+Math.PI, Math.PI*1.15+Math.PI); c.stroke();
                        c.beginPath(); c.moveTo(15, 3.4); c.lineTo(15.4, 7.2); c.lineTo(11.7, 6.6); c.stroke();
                        c.beginPath(); c.moveTo(3, 14.6); c.lineTo(2.6, 10.8); c.lineTo(6.3, 11.4); c.stroke(); }
                    transformOrigin: Item.Center
                    RotationAnimation on rotation {
                        running: backend.scanning; loops: Animation.Infinite
                        from: 0; to: 360; duration: 900
                        onRunningChanged: if (!running) refreshIcon.rotation = 0
                    }
                }
                MouseArea { id: refreshHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor; onClicked: backend.refresh() }
            }

            // offline toggle
            Rectangle {
                Layout.preferredHeight: 44
                implicitWidth: offRow.implicitWidth + 28
                radius: 10
                color: offHover.containsMouse ? Qt.rgba(0.11, 0.13, 0.17, 0.8) : Theme.input
                border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.1)
                RowLayout {
                    id: offRow
                    anchors.centerIn: parent
                    spacing: 11
                    ColumnLayout {
                        spacing: 1
                        Label { text: qsTr("Launch offline"); color: Theme.text
                            font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
                        Label { text: qsTr("Steam only"); color: Theme.faint
                            font.family: Theme.fontBody; font.pixelSize: 10.5; font.weight: Font.Medium }
                    }
                    Toggle { on: AppState.offline; onToggled: AppState.offline = !AppState.offline }
                }
                MouseArea { id: offHover; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: AppState.offline = !AppState.offline }
            }
        }

        // ---- heading + count ----
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            Label { text: qsTr("Your library"); color: Theme.text
                font.family: Theme.fontDisplay; font.pixelSize: 20; font.weight: Font.Bold }
            Item { Layout.fillWidth: true }
            Label { text: gv.count + qsTr(" of ") + backend.gameCount + qsTr(" games")
                color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 12.5; font.weight: Font.Medium }
        }

        // ---- store filter chips ----
        Flow {
            Layout.fillWidth: true
            Layout.bottomMargin: 9
            spacing: 7
            StoreChip {
                label: qsTr("All stores")
                active: AppState.storeFilter === "all"
                onClicked: { AppState.storeFilter = "all"; backend.setStoreFilter("all") }
            }
            Repeater {
                model: backend.stores
                delegate: StoreChip {
                    label: modelData.name
                    activeColor: modelData.color
                    active: AppState.storeFilter === modelData.storeName
                    onClicked: { AppState.storeFilter = modelData.storeName;
                                 backend.setStoreFilter(modelData.storeName) }
                }
            }
        }

        // ---- account filter chips ----
        Flow {
            Layout.fillWidth: true
            Layout.bottomMargin: 22
            spacing: 7
            StoreChip {
                label: qsTr("All accounts")
                active: AppState.accountFilter === "all"
                onClicked: { AppState.accountFilter = "all"; backend.setAccountFilter("all") }
            }
            Repeater {
                model: backend.accounts
                delegate: StoreChip {
                    label: modelData.personaName
                    active: AppState.accountFilter === modelData.steamid64
                    onClicked: { AppState.accountFilter = modelData.steamid64;
                                 backend.setAccountFilter(modelData.steamid64) }
                }
            }
            StoreChip {
                label: qsTr("Unmapped")
                active: AppState.accountFilter === "unmapped"
                onClicked: { AppState.accountFilter = "unmapped"; backend.setAccountFilter("unmapped") }
            }
        }

        // ---- cover grid ----
        GridView {
            id: gv
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.games

            readonly property int cols: 5
            readonly property int gap: 18
            cellWidth: Math.floor((width) / cols)
            property real tileW: cellWidth - gap
            cellHeight: Math.round(tileW * 4 / 3) + 58 + gap

            delegate: Item {
                width: gv.cellWidth
                height: gv.cellHeight
                GameCard {
                    width: gv.tileW
                    height: parent.height - gv.gap
                }
            }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            // empty / no-results state
            Item {
                anchors.centerIn: parent
                width: 320; height: 120
                visible: gv.count === 0 && !backend.scanning
                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Label { anchors.horizontalCenter: parent.horizontalCenter
                        text: backend.gameCount === 0 ? qsTr("No games installed") : qsTr("No games found")
                        color: Theme.text; font.family: Theme.fontDisplay; font.pixelSize: 17; font.weight: Font.Bold }
                    Label { anchors.horizontalCenter: parent.horizontalCenter
                        width: 320; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                        text: backend.gameCount === 0
                            ? qsTr("Install a game in Steam, Epic, GOG or Game Pass and it will show up here.")
                            : qsTr("Nothing matches the current search and filters.")
                        color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 13 }
                }
            }
        }
    }
}
