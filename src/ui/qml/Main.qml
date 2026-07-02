// ORBIT shell (CINEMA): frameless window; content fills the whole body (the
// library hero bleeds to the top edge) with a gradient top bar overlaid — logo,
// tab nav, search (Ctrl-K palette), Steam status pill, offline pill, avatar and
// the window controls. Plus the status toast, resize edges, onboarding overlay.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Orbit

ApplicationWindow {
    id: win
    width: 1280; height: 800
    minimumWidth: 960; minimumHeight: 640
    visible: true
    title: "ORBIT"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // language drives layout mirroring (ar = RTL)
    LayoutMirroring.enabled: backend.rtl
    LayoutMirroring.childrenInherit: true

    Component.onCompleted: {
        // Seed the session offline toggle from the persisted default.
        AppState.offline = backend.offlineDefault;
        // Show the first-run onboarding until it's been completed once.
        if (!backend.onboarded) AppState.onboarding = "welcome";
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: AppState.paletteOpen = !AppState.paletteOpen }
    Shortcut { sequence: "Escape"; enabled: AppState.paletteOpen
        onActivated: AppState.paletteOpen = false }

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

    // "Steam online · x_05" pill data: the account Steam is logged in as now.
    readonly property var liveAccount: {
        for (var i = 0; i < backend.accounts.length; ++i)
            if (backend.accounts[i].loggedIn) return backend.accounts[i];
        return null;
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 12
        color: Theme.bg
        border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.09)
        clip: true

        AmbientBackground { anchors.fill: parent }

        // ---- content (fills the whole shell; hero bleeds under the top bar) ----
        StackLayout {
            anchors.fill: parent
            currentIndex: AppState.view === "detail" ? 1
                : AppState.view === "accounts" ? 2
                : AppState.view === "settings" ? 3 : 0
            LibraryView {}
            DetailView {}
            AccountsView {}
            SettingsView {}
        }

        // ---- top bar overlay ----
        Rectangle {
            id: topBar
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 64
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.039, 0.047, 0.067, 0.94) }
                GradientStop { position: 0.7; color: Qt.rgba(0.039, 0.047, 0.067, 0.55) }
                GradientStop { position: 1.0; color: "transparent" }
            }

            // drag + double-click maximize (under the interactive children)
            MouseArea {
                anchors.fill: parent
                onPressed: win.startSystemMove()
                onDoubleClicked: win.visibility === Window.Maximized
                    ? win.showNormal() : win.showMaximized()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24; anchors.rightMargin: 10
                spacing: 14

                // brand
                Row {
                    spacing: 9
                    Image { width: 22; height: 22
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/icons/orbit-48.png"; sourceSize: Qt.size(48, 48)
                        smooth: true; mipmap: true }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.StyledText
                        text: "OR<font color=\"" + Theme.accent + "\">B</font>IT"
                        color: Theme.text
                        font.family: Theme.fontDisplay; font.pixelSize: 18; font.weight: Font.Bold
                        font.letterSpacing: 2.4
                    }
                }

                // tabs
                Row {
                    Layout.leftMargin: 10
                    spacing: 4
                    NavTab { label: qsTr("Library")
                        active: AppState.view === "library" || AppState.view === "detail"
                        onClicked: AppState.go("library") }
                    NavTab { label: qsTr("Accounts")
                        active: AppState.view === "accounts"; onClicked: AppState.go("accounts") }
                    NavTab { label: qsTr("Settings")
                        active: AppState.view === "settings"; onClicked: AppState.go("settings") }
                }

                // search pill -> palette
                Rectangle {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 340
                    Layout.leftMargin: 6
                    Layout.preferredHeight: 38
                    radius: 19
                    color: searchHover.containsMouse ? Qt.rgba(1, 1, 1, 0.09) : Theme.fill
                    border.width: 1; border.color: Theme.line
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16; anchors.rightMargin: 10
                        spacing: 10
                        Label { text: "⌕"; color: Theme.faint; font.pixelSize: 14 }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Search %n game(s)…", "", backend.gameCount)
                            color: Theme.faint
                            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: kbd.implicitWidth + 12; implicitHeight: 20
                            radius: 5; color: "transparent"
                            border.width: 1; border.color: Theme.line
                            Label { id: kbd; anchors.centerIn: parent; text: "Ctrl K"
                                color: Theme.faint
                                font.family: Theme.fontBody; font.pixelSize: 9; font.weight: Font.Bold }
                        }
                    }
                    MouseArea { id: searchHover; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: AppState.paletteOpen = true }
                }

                Item { Layout.fillWidth: true }

                // Steam status pill
                Rectangle {
                    Layout.preferredHeight: 34
                    radius: 17
                    implicitWidth: statusRow.implicitWidth + 28
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1; border.color: Theme.line
                    Row {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: win.liveAccount ? Theme.good : Theme.faint }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: win.liveAccount
                                ? qsTr("Steam online · %1").arg(win.liveAccount.personaName)
                                : qsTr("Steam signed out")
                            color: Theme.muted
                            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                        }
                    }
                }

                // offline mode pill (session toggle; default set in Settings)
                Rectangle {
                    Layout.preferredHeight: 34
                    radius: 17
                    implicitWidth: offRow.implicitWidth + 28
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.width: 1; border.color: AppState.offline ? Theme.accent : Theme.line
                    Row {
                        id: offRow
                        anchors.centerIn: parent
                        spacing: 8
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Offline mode")
                            color: AppState.offline ? Theme.warn : Theme.muted
                            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold
                        }
                        Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            scale: 0.75
                            on: AppState.offline
                            onToggled: AppState.offline = !AppState.offline
                        }
                    }
                }

                // avatar -> accounts
                Rectangle {
                    Layout.preferredWidth: 34; Layout.preferredHeight: 34
                    radius: 17
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.accent }
                        GradientStop { position: 1; color: "#ef4444" }
                    }
                    Label { anchors.centerIn: parent
                        text: backend.currentAccount.length ? backend.currentAccount.charAt(0).toUpperCase() : "?"
                        color: Theme.accentFg
                        font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.ExtraBold }
                    MouseArea { anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor; onClicked: AppState.go("accounts") }
                }

                // window controls
                WinButton { glyph: "min"; onClicked: win.showMinimized() }
                WinButton { glyph: "max"; onClicked: win.visibility === Window.Maximized
                    ? win.showNormal() : win.showMaximized() }
                WinButton { glyph: "close"; danger: true; onClicked: win.close() }
            }
        }

        // ---- Ctrl-K palette ----
        SearchPalette {}

        // ---- first-run onboarding overlay (on top of everything) ----
        OnboardingOverlay {}

        // ---- status toast ----
        Rectangle {
            visible: win.toastText.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 20
            radius: 12
            width: Math.min(toastRow.implicitWidth + 32, parent.width - 60)
            height: 44
            z: 70
            color: Qt.rgba(0.055, 0.070, 0.094, 0.97)
            border.width: 1
            border.color: win.toastKind === "bad" ? Theme.bad
                : (win.toastKind === "good" ? Theme.good : Qt.rgba(1, 1, 1, 0.12))
            RowLayout {
                id: toastRow
                anchors.centerIn: parent
                spacing: 10
                Rectangle { Layout.preferredWidth: 8; Layout.preferredHeight: 8; radius: 4
                    color: win.toastKind === "bad" ? Theme.bad
                        : (win.toastKind === "good" ? Theme.good : Theme.accent) }
                Label { text: win.toastText; color: Theme.text
                    font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Medium
                    elide: Text.ElideRight; Layout.maximumWidth: win.width - 140 }
            }
        }

        // ---- resize edges (frameless) ----
        Repeater {
            model: [
                { e: Qt.TopEdge,    x: 0,  y: -1, w: 1, h: 0, cur: Qt.SizeVerCursor,  horiz: true,  vert: false },
                { e: Qt.BottomEdge, x: 0,  y: 0,  w: 1, h: 0, cur: Qt.SizeVerCursor,  horiz: true,  vert: false },
                { e: Qt.LeftEdge,   x: -1, y: 0,  w: 0, h: 1, cur: Qt.SizeHorCursor,  horiz: false, vert: true },
                { e: Qt.RightEdge,  x: 0,  y: 0,  w: 0, h: 1, cur: Qt.SizeHorCursor,  horiz: false, vert: true }
            ]
            delegate: MouseArea {
                property var d: modelData
                width: d.horiz ? parent.width : 6
                height: d.vert ? parent.height : 6
                x: d.e === Qt.RightEdge ? parent.width - 6 : 0
                y: d.e === Qt.BottomEdge ? parent.height - 6 : 0
                z: 80
                cursorShape: d.cur
                onPressed: win.startSystemResize(d.e)
            }
        }
    }

    // top-bar nav tab
    component NavTab: Rectangle {
        id: tab
        property string label: ""
        property bool active: false
        signal clicked()
        width: tabLabel.implicitWidth + 32; height: 36
        radius: 18
        color: active ? Qt.rgba(1, 1, 1, 0.08)
             : tabHover.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
        Label { id: tabLabel; anchors.centerIn: parent; text: tab.label
            color: tab.active ? Theme.text : Theme.faint
            font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.Bold }
        MouseArea { id: tabHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: tab.clicked() }
    }

    // window control button
    component WinButton: Rectangle {
        property string glyph: ""
        property bool danger: false
        signal clicked()
        implicitWidth: 40; implicitHeight: 30; radius: 8
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
