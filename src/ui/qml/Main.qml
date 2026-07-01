// ORBIT shell — the multi-store launcher window. Frameless, with a custom title
// bar, an accent-tinted ambient backdrop, a glass sidebar (brand + nav + STORES),
// the content stack (Library / Detail / Accounts / Settings) and the Manage panel.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SteamSwitch

ApplicationWindow {
    id: win
    width: 1180; height: 760
    minimumWidth: 940; minimumHeight: 620
    visible: true
    title: "ORBIT"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // language drives layout mirroring (ar = RTL)
    LayoutMirroring.enabled: backend.rtl
    LayoutMirroring.childrenInherit: true

    // Show the first-run onboarding until it's been completed once.
    Component.onCompleted: if (!backend.onboarded) AppState.onboarding = "welcome"

    // transient status toast state
    property string toastText: ""
    property string toastKind: "info"     // info | good | bad
    Connections {
        target: backend
        function onStatus(message)         { win.showToast(message, "info") }
        function onLaunchDone(ok, message) { win.showToast(message, ok ? "good" : "bad") }
    }
    function showToast(msg, kind) { toastText = msg; toastKind = kind; toastTimer.restart() }
    Timer { id: toastTimer; interval: 6000; onTriggered: win.toastText = "" }

    // rounded window body with a hairline
    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 14
        color: Theme.shell
        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.09)
        clip: true

        AmbientBackground { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ---- title bar ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: Theme.titlebar
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1
                    color: Qt.rgba(1, 1, 1, 0.06) }

                MouseArea {
                    anchors.fill: parent
                    onPressed: win.startSystemMove()
                    onDoubleClicked: win.visibility === Window.Maximized
                        ? win.showNormal() : win.showMaximized()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16; anchors.rightMargin: 6
                    spacing: 10
                    Rectangle { Layout.preferredWidth: 15; Layout.preferredHeight: 15; radius: 7.5
                        gradient: Gradient { GradientStop { position: 0; color: "#5fb8ff" }
                                             GradientStop { position: 1; color: "#2a7fd4" } } }
                    Label { text: "ORBIT"; color: Theme.text
                        font.family: Theme.fontDisplay; font.pixelSize: 12; font.weight: Font.Bold
                        font.letterSpacing: 1 }
                    Label { text: qsTr("All your games, one shelf"); color: Theme.ghost
                        font.family: Theme.fontBody; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }

                    // window controls
                    WinButton { glyph: "min"; onClicked: win.showMinimized() }
                    WinButton { glyph: "max"; onClicked: win.visibility === Window.Maximized
                        ? win.showNormal() : win.showMaximized() }
                    WinButton { glyph: "close"; danger: true; onClicked: win.close() }
                }
            }

            // ---- body: sidebar + content ----
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // sidebar
                Rectangle {
                    Layout.preferredWidth: 212
                    Layout.fillHeight: true
                    color: Theme.sidebar
                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height
                        color: Qt.rgba(1, 1, 1, 0.06) }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 3

                        // brand
                        RowLayout {
                            Layout.leftMargin: 4; Layout.topMargin: 4; Layout.bottomMargin: 14
                            spacing: 9
                            Rectangle { Layout.preferredWidth: 24; Layout.preferredHeight: 24; radius: 7
                                gradient: Gradient { GradientStop { position: 0; color: "#5fb8ff" }
                                                     GradientStop { position: 1; color: "#2a7fd4" } } }
                            Label { text: "ORBIT"; color: Theme.text
                                font.family: Theme.fontDisplay; font.pixelSize: 15; font.weight: Font.Bold
                                font.letterSpacing: 0.8 }
                        }

                        SideNavItem { kind: "library"; label: qsTr("Library")
                            active: AppState.view === "library" || AppState.view === "detail"
                            onClicked: AppState.go("library") }
                        SideNavItem { kind: "accounts"; label: qsTr("Accounts")
                            active: AppState.view === "accounts"; onClicked: AppState.go("accounts") }
                        SideNavItem { kind: "settings"; label: qsTr("Settings")
                            active: AppState.view === "settings"; onClicked: AppState.go("settings") }

                        Item { Layout.fillHeight: true }

                        // STORES panel
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 11
                            color: Theme.glassSoft
                            border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.06)
                            implicitHeight: storesCol.implicitHeight + 26
                            ColumnLayout {
                                id: storesCol
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.top: parent.top; anchors.margins: 13
                                spacing: 9
                                Label { text: qsTr("STORES"); color: Theme.faint
                                    font.family: Theme.fontBody; font.pixelSize: 11
                                    font.weight: Font.Bold; font.letterSpacing: 1.3 }
                                Repeater {
                                    model: backend.stores
                                    delegate: RowLayout {
                                        Layout.fillWidth: true; spacing: 9
                                        Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8
                                            radius: 4; color: modelData.color
                                            opacity: modelData.connected ? 1 : 0.35 }
                                        Label { text: modelData.name
                                            color: modelData.connected ? Qt.rgba(1, 1, 1, 0.75) : Theme.ghost
                                            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                                            Layout.fillWidth: true }
                                        Label { text: modelData.count; color: Theme.faint
                                            font.family: Theme.fontBody; font.pixelSize: 12 }
                                    }
                                }
                            }
                        }
                    }
                }

                // content
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: AppState.view === "detail" ? 1
                        : AppState.view === "accounts" ? 2
                        : AppState.view === "settings" ? 3 : 0
                    LibraryView {}
                    DetailView {}
                    AccountsView {}
                    SettingsView {}
                }
            }
        }

        // ---- Manage overlay ----
        ManagePanel {}

        // ---- first-run onboarding overlay (on top of everything) ----
        OnboardingOverlay {}

        // ---- status toast ----
        Rectangle {
            visible: win.toastText.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 20
            radius: 10
            width: Math.min(toastRow.implicitWidth + 32, parent.width - 60)
            height: 44
            color: Qt.rgba(0.055, 0.070, 0.094, 0.97)
            border.width: 1
            border.color: win.toastKind === "bad" ? Theme.bad
                : (win.toastKind === "good" ? Theme.good : Qt.rgba(1, 1, 1, 0.12))
            RowLayout {
                id: toastRow
                anchors.centerIn: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                spacing: 10
                Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
                    color: win.toastKind === "bad" ? Theme.bad
                        : (win.toastKind === "good" ? Theme.good : AppState.accent) }
                Label { text: win.toastText; color: Theme.text
                    font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Medium
                    elide: Text.ElideRight; Layout.maximumWidth: win.width - 140 }
            }
        }

        // ---- resize edges (frameless) ----
        Repeater {
            model: [
                { e: Qt.TopEdge,                   x: 0,  y: -1, w: 1, h: 0, cur: Qt.SizeVerCursor,  horiz: true,  vert: false },
                { e: Qt.BottomEdge,                x: 0,  y: 0,  w: 1, h: 0, cur: Qt.SizeVerCursor,  horiz: true,  vert: false },
                { e: Qt.LeftEdge,                  x: -1, y: 0,  w: 0, h: 1, cur: Qt.SizeHorCursor,  horiz: false, vert: true },
                { e: Qt.RightEdge,                 x: 0,  y: 0,  w: 0, h: 1, cur: Qt.SizeHorCursor,  horiz: false, vert: true }
            ]
            delegate: MouseArea {
                property var d: modelData
                width: d.horiz ? parent.width : 6
                height: d.vert ? parent.height : 6
                x: d.e === Qt.RightEdge ? parent.width - 6 : 0
                y: d.e === Qt.BottomEdge ? parent.height - 6 : 0
                cursorShape: d.cur
                onPressed: win.startSystemResize(d.e)
            }
        }
    }

    // window control button
    component WinButton: Rectangle {
        property string glyph: ""
        property bool danger: false
        signal clicked()
        implicitWidth: 42; implicitHeight: 30; radius: 6
        color: hov.containsMouse ? (danger ? Theme.bad : Qt.rgba(1, 1, 1, 0.08)) : "transparent"
        Canvas {
            anchors.centerIn: parent; width: 11; height: 11
            property color stroke: (parent.danger && hov.containsMouse) ? "#fff" : Qt.rgba(1, 1, 1, 0.6)
            onStrokeChanged: requestPaint()
            onPaint: { var c = getContext("2d"); c.reset(); c.strokeStyle = stroke;
                c.lineWidth = 1.4; c.lineCap = "round";
                if (glyph === "min") { c.beginPath(); c.moveTo(1, 6); c.lineTo(10, 6); c.stroke(); }
                else if (glyph === "max") { c.strokeRect(1.5, 1.5, 8, 8); }
                else { c.beginPath(); c.moveTo(1.5, 1.5); c.lineTo(9.5, 9.5);
                       c.moveTo(9.5, 1.5); c.lineTo(1.5, 9.5); c.stroke(); } }
        }
        MouseArea { id: hov; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked() }
    }
}
