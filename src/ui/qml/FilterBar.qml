// Filter dropdown: sort (A→Z / Z→A) + filter by account / unmapped. Ported from
// web/app.js toggleFilterMenu + .btn.ghost / .menu styling.

import QtQuick
import QtQuick.Controls
import SteamSwitch

Item {
    id: root
    implicitWidth: btn.width
    implicitHeight: 36

    property var accounts: []
    property string sortOrder: "az"
    property string accountFilter: "all"

    signal sortChanged(string value)
    signal filterChanged(string value)

    Rectangle {
        id: btn
        width: row.width + 24; height: 36; radius: Theme.rMd
        color: bm.containsMouse ? Theme.surfaceHigh : Theme.surface
        border.width: 1; border.color: Theme.outline
        Row {
            id: row; anchors.centerIn: parent; spacing: 7
            Label { text: "⛃"; color: Theme.text; font.pixelSize: 14 }
            Label { text: qsTr("Filter"); color: Theme.text; font.pixelSize: 13; font.bold: true }
        }
        MouseArea { id: bm; anchors.fill: parent; hoverEnabled: true; onClicked: menu.open() }

        Menu {
            id: menu
            y: btn.height + 6
            MenuItem { text: qsTr("Sort A → Z"); checkable: true; checked: root.sortOrder === "az"
                onTriggered: { root.sortOrder = "az"; root.sortChanged("az") } }
            MenuItem { text: qsTr("Sort Z → A"); checkable: true; checked: root.sortOrder === "za"
                onTriggered: { root.sortOrder = "za"; root.sortChanged("za") } }
            MenuSeparator {}
            MenuItem { text: qsTr("All accounts"); checkable: true; checked: root.accountFilter === "all"
                onTriggered: { root.accountFilter = "all"; root.filterChanged("all") } }
            MenuItem { text: qsTr("Unmapped"); checkable: true; checked: root.accountFilter === "unmapped"
                onTriggered: { root.accountFilter = "unmapped"; root.filterChanged("unmapped") } }
            MenuSeparator {}
            Instantiator {
                model: root.accounts
                delegate: MenuItem {
                    text: modelData.personaName
                    checkable: true
                    checked: root.accountFilter === modelData.steamid64
                    onTriggered: { root.accountFilter = modelData.steamid64; root.filterChanged(modelData.steamid64) }
                }
                onObjectAdded: (i, obj) => menu.insertItem(menu.count, obj)
                onObjectRemoved: (i, obj) => menu.removeItem(obj)
            }
        }
    }
}
