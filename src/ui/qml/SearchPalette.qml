// Ctrl-K search palette (design/m1-search.html): dimmed backdrop, a centered
// glass panel with a query field and live results. ↑↓ navigate, Enter = play,
// Shift+Enter = play offline, Tab = details, Esc = close.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Orbit

Item {
    id: pal
    anchors.fill: parent
    visible: AppState.paletteOpen
    z: 60

    component Hint: Row {
        property string keys: ""
        property string what: ""
        spacing: 5
        Label { text: parent.keys; color: Theme.muted
            font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.ExtraBold }
        Label { text: parent.what; color: Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.Bold }
    }

    onVisibleChanged: {
        if (visible) { query.text = ""; results.currentIndex = 0; query.forceActiveFocus() }
    }

    GameFilter {
        id: matches
        sourceModel: backend.allGames
        searchText: query.text
        sortMode: "recent"
    }

    function act(kind) {
        if (results.currentIndex < 0 || results.currentIndex >= matches.count) return;
        var g = matches.gameAt(results.currentIndex);
        AppState.paletteOpen = false;
        if (kind === "details") AppState.open(g);
        else backend.play(g.appid, kind === "offline" ? true : AppState.offline);
    }

    // dim + click-away
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.02, 0.024, 0.039, 0.55)
        MouseArea { anchors.fill: parent; onClicked: AppState.paletteOpen = false }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(40, Math.round(parent.height * 0.13))
        width: Math.min(640, parent.width - 80)
        height: Math.min(col.implicitHeight, parent.height - y - 40)
        radius: 20
        color: Qt.rgba(0.063, 0.075, 0.106, 0.95)
        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.14)
        clip: true

        ColumnLayout {
            id: col
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0

            // query line
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Layout.topMargin: 8; Layout.bottomMargin: 8
                spacing: 12
                Label { text: "⌕"; color: Theme.faint; font.pixelSize: 18 }
                TextField {
                    id: query
                    Layout.fillWidth: true
                    placeholderText: qsTr("Jump to a game…")
                    color: Theme.text
                    placeholderTextColor: Theme.faint
                    font.family: Theme.fontBody; font.pixelSize: 16; font.weight: Font.Bold
                    background: null
                    onTextChanged: results.currentIndex = 0
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Down) { results.incrementCurrentIndex(); event.accepted = true }
                        else if (event.key === Qt.Key_Up) { results.decrementCurrentIndex(); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            pal.act(event.modifiers & Qt.ShiftModifier ? "offline" : "play");
                            event.accepted = true
                        } else if (event.key === Qt.Key_Tab) { pal.act("details"); event.accepted = true }
                        else if (event.key === Qt.Key_Escape) { AppState.paletteOpen = false; event.accepted = true }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

            // results
            ListView {
                id: results
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 5 * 72)
                Layout.margins: 8
                clip: true
                model: matches
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 80
                delegate: Rectangle {
                    width: results.width
                    height: 72
                    radius: 13
                    color: ListView.isCurrentItem || rowHover.containsMouse
                           ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    required property int index
                    required property var model
                    property string coverUrl: ""
                    Component.onCompleted: backend.requestCover(model.appid)
                    Connections {
                        target: backend
                        function onCoverReady(appid, dataUrl) {
                            if (appid === model.appid && dataUrl.length) coverUrl = dataUrl
                        }
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 14
                        spacing: 14
                        Rectangle {
                            Layout.preferredWidth: 38; Layout.preferredHeight: 52
                            radius: 7; clip: true
                            color: Qt.darker(Theme.accentFor(model.appid), 1.8)
                            Image { anchors.fill: parent; source: coverUrl
                                fillMode: Image.PreserveAspectCrop; asynchronous: true
                                visible: coverUrl.length > 0 }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label { text: model.name; color: Theme.text
                                font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.ExtraBold
                                elide: Text.ElideRight; Layout.fillWidth: true }
                            Row {
                                spacing: 6
                                Rectangle { width: 5; height: 5; radius: 2.5
                                    color: Theme.store(model.store).color
                                    anchors.verticalCenter: parent.verticalCenter }
                                Label {
                                    text: {
                                        var bits = [Theme.store(model.store).name];
                                        if (model.store === "Steam") bits.push(model.accountName);
                                        if (model.playtime > 0) bits.push(Theme.hoursLabel(model.playtime));
                                        return bits.join(" · ");
                                    }
                                    color: Theme.faint
                                    font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold
                                }
                            }
                        }
                        Rectangle {
                            visible: ListView.isCurrentItem
                            implicitWidth: enterLabel.implicitWidth + 16; implicitHeight: 20
                            radius: 6
                            color: Qt.rgba(1, 1, 1, 0.12)
                            border.width: 1; border.color: Theme.line
                            Label { id: enterLabel; anchors.centerIn: parent
                                text: qsTr("↵ Play"); color: Theme.muted
                                font.family: Theme.fontBody; font.pixelSize: 10; font.weight: Font.Bold }
                        }
                    }
                    MouseArea {
                        id: rowHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { results.currentIndex = index; pal.act("play") }
                    }
                }
                // no matches
                Label {
                    anchors.centerIn: parent
                    visible: matches.count === 0
                    text: query.text.length ? qsTr("No games match “%1”").arg(query.text)
                                            : qsTr("Type to search your library")
                    color: Theme.faint
                    font.family: Theme.fontBody; font.pixelSize: 13
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

            // key hints footer
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22; Layout.rightMargin: 22
                Layout.topMargin: 10; Layout.bottomMargin: 12
                spacing: 18
                Hint { keys: "↑↓"; what: qsTr("navigate") }
                Hint { keys: "↵"; what: qsTr("play") }
                Hint { keys: qsTr("Shift ↵"); what: qsTr("play offline") }
                Hint { keys: qsTr("Tab"); what: qsTr("details") }
                Item { Layout.fillWidth: true }
                Hint { keys: qsTr("Esc"); what: qsTr("close") }
            }
        }
    }
}
