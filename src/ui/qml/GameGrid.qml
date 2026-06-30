// Library grid. Bound directly to the C++ proxy model (search/filter/sort happen
// there), so rows reliably appear once a scan finishes. Ported from .content/.grid.

import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root
    property bool offline: false

    // header row (view title + count)
    Column {
        anchors.fill: parent
        anchors.leftMargin: 24; anchors.rightMargin: 24; anchors.bottomMargin: 8
        spacing: 8

        Item {
            width: parent.width; height: 18
            Label { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("MY GAMES"); color: Theme.muted
                font.family: Theme.fontMono; font.pixelSize: 12; font.letterSpacing: 1; font.bold: true }
            Label { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Showing ") + gv.count + qsTr(" of ") + backend.gameCount + qsTr(" games")
                color: Theme.faint; font.pixelSize: 13 }
        }

        // empty state
        Label {
            visible: backend.gameCount === 0 && !backend.scanning
            width: parent.width
            text: qsTr("No games found on this PC. Install a game in Steam (or another store) and it will appear here.")
            color: Theme.faint; font.pixelSize: 14; wrapMode: Text.WordWrap
        }

        GridView {
            id: gv
            width: parent.width
            height: parent.height - 40
            cellWidth: 166; cellHeight: 256
            clip: true
            model: backend.games
            delegate: GameCard {
                width: gv.cellWidth - 16
                height: gv.cellHeight - 16
                offline: root.offline
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
