// A CINEMA shelf: heading (optional brand dot + title + live count) over a
// horizontal row of GameCards. The caller provides the filter model (its own
// GameFilter over Backend.allGames); the shelf hides itself when empty.
import QtQuick
import QtQuick.Controls
import Orbit

Column {
    id: shelf
    property alias model: row.model
    property string title: ""
    property string dotColor: ""       // brand dot ("" = none)
    property int cardWidth: 132

    visible: row.count > 0
    spacing: 12

    Row {
        spacing: 10
        Rectangle {
            visible: shelf.dotColor.length > 0
            width: 8; height: 8; radius: 4; color: shelf.dotColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: shelf.title
            color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 15; font.weight: Font.DemiBold
        }
        Label {
            visible: row.count > 0
            text: qsTr("%n game(s)", "", row.count)
            color: Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    ListView {
        id: row
        width: shelf.width
        height: shelf.cardWidth * 4 / 3 + 48
        orientation: ListView.Horizontal
        spacing: 14
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        delegate: GameCard { width: shelf.cardWidth }
        ScrollBar.horizontal: AppScrollBar { }
    }
}
