// CINEMA settings (design/m1-settings.html): Launching (offline default,
// run-on-Windows-startup), Interface (language, hero banner mode), Library
// (rescan + real last-scan time, cover cache size/clear) and the Families tip.
// Every row is wired to a real persisted setting — nothing decorative.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Orbit

Flickable {
    id: root
    contentHeight: col.implicitHeight + 100
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: AppScrollBar {}

    // Cache size is fetched on view load + after clearing (it's a disk walk).
    property real cacheBytes: 0
    function refreshCacheSize() { cacheBytes = backend.coverCacheSize() }
    Component.onCompleted: refreshCacheSize()
    function cacheLabel() {
        if (cacheBytes <= 0) return qsTr("empty");
        if (cacheBytes < 1024 * 1024) return Math.round(cacheBytes / 1024) + " KB";
        return (cacheBytes / (1024 * 1024)).toFixed(1) + " MB";
    }

    component SectLabel: Label {
        color: Theme.faint
        font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.ExtraBold
        font.letterSpacing: 2
    }
    component Card: Rectangle {
        default property alias content: inner.data
        Layout.fillWidth: true
        radius: Theme.rLg
        color: Theme.panel
        border.width: 1; border.color: Theme.line
        implicitHeight: inner.implicitHeight
        clip: true
        ColumnLayout {
            id: inner
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
        }
    }
    component RowSep: Rectangle {
        Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line
    }
    component SettingRow: RowLayout {
        id: sr
        property string title: ""
        property string desc: ""
        default property alias control: slot.data
        Layout.fillWidth: true
        Layout.leftMargin: 20; Layout.rightMargin: 20
        Layout.topMargin: 16; Layout.bottomMargin: 16
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Label { text: sr.title; color: Theme.text
                font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.ExtraBold }
            Label { Layout.fillWidth: true
                text: sr.desc; color: Theme.faint; visible: text.length > 0
                font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold
                wrapMode: Text.WordWrap; lineHeight: 1.35 }
        }
        Item { id: slot
            implicitWidth: childrenRect.width; implicitHeight: childrenRect.height }
    }
    // segmented control ("English | العربية", "Last played | Random pick")
    component Seg: Rectangle {
        id: seg
        property var options: []          // [{value, label}]
        property string value: ""
        signal picked(string value)
        implicitWidth: segRow.implicitWidth + 2; implicitHeight: 36
        radius: 11
        color: "transparent"
        border.width: 1; border.color: Theme.line
        Row {
            id: segRow
            anchors.centerIn: parent
            Repeater {
                model: seg.options
                delegate: Rectangle {
                    required property var modelData
                    width: segLabel.implicitWidth + 32; height: 34
                    radius: 10
                    property bool active: modelData.value === seg.value
                    color: active ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    Label { id: segLabel; anchors.centerIn: parent
                        text: modelData.label
                        color: parent.active ? Theme.text : Theme.faint
                        font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: seg.picked(modelData.value) }
                }
            }
        }
    }
    component ActButton: Rectangle {
        id: ab
        property string label: ""
        signal clicked()
        implicitWidth: abLabel.implicitWidth + 32; implicitHeight: 36
        radius: 11
        color: abHover.containsMouse ? Theme.fillHover : Theme.fill
        border.width: 1; border.color: Theme.line
        Label { id: abLabel; anchors.centerIn: parent; text: ab.label
            color: Theme.text
            font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold }
        MouseArea { id: abHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: ab.clicked() }
    }

    ColumnLayout {
        id: col
        x: 44; y: 92
        width: Math.min(root.width - 88, 860)
        spacing: 0

        Label { text: qsTr("Settings"); color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 30; font.weight: Font.Bold }
        Label { Layout.topMargin: 6
            text: qsTr("Preferences persist in settings.json next to the app.")
            color: Theme.muted; font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }

        // ---- LAUNCHING ----
        SectLabel { Layout.topMargin: 24; Layout.bottomMargin: 12; text: qsTr("LAUNCHING") }
        Card {
            SettingRow {
                title: qsTr("Launch offline by default")
                desc: qsTr("New launches bring Steam up offline after a brief sign-in — shared-account friendly, cloud saves untouched.")
                Toggle {
                    on: backend.offlineDefault
                    onToggled: { backend.setOfflineDefault(!backend.offlineDefault);
                                 AppState.offline = backend.offlineDefault }
                }
            }
            RowSep { visible: backend.autostartSupported }
            SettingRow {
                visible: backend.autostartSupported
                title: qsTr("Run on Windows startup")
                desc: qsTr("Start ORBIT when you sign into Windows (adds a registry Run entry for this exe).")
                Toggle {
                    on: backend.runAtStartup
                    onToggled: backend.setRunAtStartup(!backend.runAtStartup)
                }
            }
        }

        // ---- INTERFACE ----
        SectLabel { Layout.topMargin: 24; Layout.bottomMargin: 12; text: qsTr("INTERFACE") }
        Card {
            SettingRow {
                title: qsTr("Language")
                desc: qsTr("Display language — العربية flips the whole layout right-to-left.")
                Seg {
                    options: [ { value: "en", label: "English" }, { value: "ar", label: "العربية" } ]
                    value: backend.language
                    onPicked: (v) => backend.setLanguage(v)
                }
            }
            RowSep {}
            SettingRow {
                title: qsTr("Hero banner")
                desc: qsTr("What the big banner shows when the library opens.")
                Seg {
                    options: [ { value: "last", label: qsTr("Last played") },
                               { value: "random", label: qsTr("Random pick") } ]
                    value: backend.heroMode
                    onPicked: (v) => backend.setHeroMode(v)
                }
            }
        }

        // ---- LIBRARY ----
        SectLabel { Layout.topMargin: 24; Layout.bottomMargin: 12; text: qsTr("LIBRARY") }
        Card {
            SettingRow {
                title: qsTr("Rescan library")
                desc: backend.lastScanTime > 0
                    ? qsTr("Re-detect installed games and accounts across all stores. Last scan: %1.").arg(Theme.relTime(backend.lastScanTime))
                    : qsTr("Re-detect installed games and accounts across all stores.")
                ActButton {
                    label: backend.scanning ? qsTr("Scanning…") : qsTr("⟳ Rescan now")
                    onClicked: backend.refresh()
                }
            }
            RowSep {}
            SettingRow {
                title: qsTr("Cover art cache")
                desc: qsTr("Downloaded covers are cached on disk so store APIs are hit at most once per game. Currently %1.").arg(root.cacheLabel())
                ActButton {
                    label: qsTr("Clear cache")
                    onClicked: { backend.clearCoverCache(); root.refreshCacheSize() }
                }
            }
        }

        Label {
            Layout.topMargin: 20; Layout.fillWidth: true
            text: qsTr("Tip: if every account is your own, Steam Families may remove the need to switch at all. ORBIT never types passwords — accounts must be signed in once with “Remember me”.")
            color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold
            wrapMode: Text.WordWrap; lineHeight: 1.4
        }
    }
}
