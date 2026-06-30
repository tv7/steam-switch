// Proton-Dark shell, matched to the shipped web/Tauri layout: left sidebar (brand,
// Library/Settings nav, ACCOUNTS chip), main column with topbar (search + Refresh
// + Filter ... offline toggle + Stop launch), the content view, and a status bar.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

ApplicationWindow {
    id: win
    width: 1180; height: 780
    visible: true
    title: "SteamSwitch"
    color: Theme.bg

    property bool rtl: false
    LayoutMirroring.enabled: rtl
    LayoutMirroring.childrenInherit: true

    property string view: "library"        // "library" | "settings" | "accounts"
    property bool offline: false
    property string statusText: ""
    property string statusKind: "info"     // info | good | bad

    Connections {
        target: backend
        function onStatus(message) { win.statusText = message; win.statusKind = "info" }
        function onLaunchDone(ok, message) { win.statusText = message; win.statusKind = ok ? "good" : "bad" }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ----------------------------------------------------------- sidebar
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 4

                // brand: swap-arrows mark + wordmark
                RowLayout {
                    Layout.bottomMargin: 14
                    Layout.leftMargin: 6
                    spacing: 10
                    Rectangle { width: 34; height: 34; radius: Theme.rMd; color: Theme.primary
                        Label { anchors.centerIn: parent; text: "⇄"; color: "#fff"; font.bold: true; font.pixelSize: 18 } }
                    Label { text: "SteamSwitch"; color: "#fff"; font.pixelSize: 17; font.bold: true }
                }

                SideNavItem { text: qsTr("Library");  glyph: "▦"; active: win.view === "library"
                    onClicked: win.view = "library" }
                SideNavItem { text: qsTr("Settings"); glyph: "⚙"; active: win.view === "settings"
                    onClicked: win.view = "settings" }

                Item { Layout.fillHeight: true }

                // ACCOUNTS chip -> opens the accounts view
                Rectangle {
                    Layout.fillWidth: true
                    height: 58; radius: Theme.rLg
                    color: chipMouse.containsMouse ? Theme.primaryHover : Theme.primary
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                        Rectangle { width: 38; height: 38; radius: Theme.rMd; color: "#2a3a6a"
                            Label { anchors.centerIn: parent
                                text: backend.currentAccount.length ? backend.currentAccount.charAt(0).toUpperCase() : "—"
                                color: "#fff"; font.bold: true } }
                        ColumnLayout {
                            spacing: 0; Layout.fillWidth: true
                            Label { text: qsTr("ACCOUNTS"); color: "#d8e2ff"
                                font.pixelSize: 9; font.family: Theme.fontMono; font.letterSpacing: 1; font.bold: true }
                            Label { text: backend.currentAccount.length ? backend.currentAccount : qsTr("No account")
                                color: "#fff"; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight
                                Layout.fillWidth: true }
                        }
                    }
                    MouseArea { id: chipMouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: win.view = "accounts" }
                }
            }
        }

        // -------------------------------------------------------------- main
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // topbar
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 10

                // search
                Rectangle {
                    Layout.preferredWidth: 320; height: 36; radius: Theme.rMd
                    color: Theme.base; border.width: 1
                    border.color: searchInput.activeFocus ? Theme.primary : Theme.outline
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
                        Label { text: "⌕"; color: Theme.faint }
                        TextField {
                            id: searchInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search games in library…")
                            color: Theme.text
                            placeholderTextColor: Theme.faint
                            background: null
                            onTextChanged: backend.setSearch(text)
                        }
                    }
                }

                GhostButton { glyph: "⟳"; text: qsTr("Refresh"); onClicked: backend.refresh() }
                FilterBar {
                    accounts: backend.accounts
                    onSortChanged: (v) => backend.setSortOrder(v)
                    onFilterChanged: (v) => backend.setAccountFilter(v)
                }

                Item { Layout.fillWidth: true }

                // offline toggle
                RowLayout {
                    spacing: 8
                    Label { text: qsTr("LAUNCH OFFLINE"); color: Theme.muted
                        font.family: Theme.fontMono; font.pixelSize: 11; font.letterSpacing: 0.5 }
                    Switch { checked: win.offline; onToggled: win.offline = checked }
                }

                GhostButton {
                    glyph: "⊘"; text: qsTr("Stop launch")
                    enabled: backend.launching
                    onClicked: backend.cancel()
                }
            }

            // content
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: win.view === "accounts" ? 2 : (win.view === "settings" ? 1 : 0)

                GameGrid { offline: win.offline }
                SettingsView {}
                AccountsView {}
            }

            // status bar
            Rectangle {
                Layout.fillWidth: true
                height: 32; color: Theme.panel
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24; spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4
                        color: win.statusKind === "bad" ? Theme.bad
                               : (win.statusKind === "good" ? Theme.good : Theme.primary) }
                    Label { text: win.statusText.length ? win.statusText
                            : (backend.scanning ? qsTr("Scanning library…") : qsTr("Ready"))
                        color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: "C++ / Qt"; color: Theme.faint; font.pixelSize: 12; font.italic: true }
                }
            }
        }
    }
}
