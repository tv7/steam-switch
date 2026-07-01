// First-run onboarding overlay (Welcome → Connect → Done), ported from the ORBIT
// design. Adapted to this app's reality: stores are *detected* from what's installed
// (no OAuth), and Steam is the one store with a real setup action — signing in an
// account (with "Remember me") so silent switching works. So the "Connect" step
// shows real detection status per store + a Steam "Add account" action + Rescan,
// instead of the mock's simulated connect toggles.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Item {
    id: root
    anchors.fill: parent
    visible: AppState.onboarding !== ""
    z: 1000

    readonly property var descByKey: ({
        "steam": qsTr("Your biggest library — and the only store that switches accounts"),
        "epic":  qsTr("Free weekly games & exclusives"),
        "gog":   qsTr("DRM-free classics & GOG Galaxy"),
        "xbox":  qsTr("Hundreds of Game Pass titles")
    })

    function connectedCount() {
        var n = 0, list = backend.stores;
        for (var i = 0; i < list.length; i++) if (list[i].connected) n++;
        return n;
    }
    function finish() {
        backend.completeOnboarding();
        AppState.onboarding = "";
        AppState.go("library");
    }

    // opaque backdrop (hides the app behind) — approximates the design's radial
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.canvasTop }
            GradientStop { position: 0.62; color: Theme.canvasBottom }
            GradientStop { position: 1.0; color: Theme.canvasBottom }
        }
        MouseArea { anchors.fill: parent }   // swallow clicks to the app behind
    }

    // ============================ WELCOME ============================
    ColumnLayout {
        anchors.centerIn: parent
        width: 460
        visible: AppState.onboarding === "welcome"
        spacing: 0

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 66; Layout.preferredHeight: 66; radius: 20
            Layout.bottomMargin: 26
            gradient: Gradient { GradientStop { position: 0; color: "#5fb8ff" }
                                 GradientStop { position: 1; color: "#2a7fd4" } }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Welcome to ORBIT"); color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 40; font.weight: Font.Bold
        }
        Label {
            Layout.fillWidth: true; Layout.topMargin: 14; Layout.bottomMargin: 34
            text: qsTr("One library for every game you own. ORBIT gathers your Steam, Epic, GOG and Xbox Game Pass games onto a single shelf — and switches Steam accounts for you.")
            color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            font.family: Theme.fontBody; font.pixelSize: 16; lineHeight: 1.5
        }
        PrimaryButton {
            Layout.alignment: Qt.AlignHCenter
            label: qsTr("Get started")
            onClicked: AppState.obGo("connect")
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 30; spacing: 10
            Repeater {
                model: backend.stores
                delegate: Rectangle { Layout.preferredWidth: 9; Layout.preferredHeight: 9; radius: 3
                    color: modelData.color }
            }
        }
    }

    // ============================ CONNECT ============================
    ColumnLayout {
        anchors.centerIn: parent
        width: 600
        visible: AppState.onboarding === "connect"
        spacing: 0

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Your stores"); color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 30; font.weight: Font.Bold
        }
        Label {
            Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 9; Layout.bottomMargin: 26
            text: qsTr("ORBIT finds installed games automatically. Sign in to a Steam account (with “Remember me”) to switch between accounts.")
            color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.family: Theme.fontBody; font.pixelSize: 14
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2; rowSpacing: 12; columnSpacing: 12

            Repeater {
                model: backend.stores
                delegate: Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 76; radius: 14
                    color: Qt.rgba(1, 1, 1, 0.04)
                    border.width: 1
                    border.color: modelData.connected ? Qt.rgba(0.247, 0.682, 0.31, 0.3)
                                                       : Qt.rgba(1, 1, 1, 0.09)
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                        spacing: 14
                        Rectangle { Layout.preferredWidth: 44; Layout.preferredHeight: 44; radius: 11
                            color: modelData.color
                            Label { anchors.centerIn: parent; text: modelData.shortName.charAt(0)
                                color: modelData.textColor; font.family: Theme.fontDisplay
                                font.pixelSize: 18; font.weight: Font.ExtraBold } }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            RowLayout {
                                spacing: 7
                                Label { text: modelData.name; color: Theme.text
                                    font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.Bold }
                                Rectangle { visible: modelData.isSteam
                                    implicitWidth: multi.implicitWidth + 12; implicitHeight: 16; radius: 4
                                    color: Qt.rgba(0.373, 0.722, 1, 0.14)
                                    Label { id: multi; anchors.centerIn: parent; text: qsTr("MULTI")
                                        color: "#5fb8ff"; font.family: Theme.fontBody
                                        font.pixelSize: 8; font.weight: Font.Bold; font.letterSpacing: 0.6 } }
                            }
                            Label { text: root.descByKey[modelData.key] || ""
                                color: Theme.faint; font.family: Theme.fontBody; font.pixelSize: 12
                                elide: Text.ElideRight; Layout.fillWidth: true }
                        }

                        // right slot: Steam has a real action; others show detection
                        Loader {
                            Layout.preferredWidth: 120
                            sourceComponent: modelData.isSteam ? steamAction : detectPill
                            property var m: modelData
                        }
                    }
                }
            }
        }

        // footer
        Rectangle { Layout.fillWidth: true; Layout.topMargin: 20; Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.09) }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 20
            Label { text: root.connectedCount() + qsTr(" of 4 stores detected")
                color: Theme.muted; font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            TextButton { label: backend.scanning ? qsTr("Rescanning…") : qsTr("Rescan")
                onClicked: backend.refresh() }
            TextButton { label: qsTr("Skip for now"); onClicked: root.finish() }
            PrimaryButton { label: qsTr("Continue"); compact: true; withArrow: true
                onClicked: AppState.obGo("done") }
        }
    }

    // ============================= DONE =============================
    ColumnLayout {
        anchors.centerIn: parent
        width: 440
        visible: AppState.onboarding === "done"
        spacing: 0

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 72; Layout.preferredHeight: 72; radius: 36
            Layout.bottomMargin: 26
            color: Qt.rgba(0.247, 0.682, 0.31, 0.15)
            border.width: 1; border.color: Qt.rgba(0.247, 0.682, 0.31, 0.4)
            Canvas {
                anchors.centerIn: parent; width: 34; height: 34
                onPaint: { var c = getContext("2d"); c.reset(); c.strokeStyle = "#4ed36a";
                    c.lineWidth = 2.6; c.lineCap = "round"; c.lineJoin = "round";
                    c.beginPath(); c.moveTo(7, 18); c.lineTo(14, 25); c.lineTo(27, 10); c.stroke(); }
            }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("You're all set"); color: Theme.text
            font.family: Theme.fontDisplay; font.pixelSize: 34; font.weight: Font.Bold
        }
        Label {
            Layout.fillWidth: true; Layout.topMargin: 12; Layout.bottomMargin: 32
            text: qsTr("%1 games found across %2 stores. Your library is ready.")
                .arg(backend.gameCount).arg(root.connectedCount())
            color: Theme.muted; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            font.family: Theme.fontBody; font.pixelSize: 16; lineHeight: 1.5
        }
        PrimaryButton {
            Layout.alignment: Qt.AlignHCenter
            label: qsTr("Enter ORBIT"); withArrow: true
            onClicked: root.finish()
        }
    }

    // ---- inline components ----

    // Steam right-slot: a real "Add account" action.
    Component {
        id: steamAction
        Rectangle {
            implicitWidth: 120; implicitHeight: 38; radius: 9
            color: addHover.containsMouse ? "#5fb8ff" : Qt.rgba(0.373, 0.722, 1, 0.14)
            border.width: 1; border.color: addHover.containsMouse ? "transparent" : Qt.rgba(0.373, 0.722, 1, 0.4)
            RowLayout {
                anchors.centerIn: parent; spacing: 6
                Label { text: "＋"; color: addHover.containsMouse ? "#04141f" : "#5fb8ff"; font.pixelSize: 13 }
                Label { text: qsTr("Add account"); color: addHover.containsMouse ? "#04141f" : "#5fb8ff"
                    font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.Bold }
            }
            MouseArea { id: addHover; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor; onClicked: backend.addAccount() }
        }
    }

    // Non-Steam right-slot: detection status pill.
    Component {
        id: detectPill
        Rectangle {
            id: pill
            property bool on: parent && parent.m ? parent.m.connected : false
            implicitWidth: 120; implicitHeight: 38; radius: 9
            color: on ? Qt.rgba(0.247, 0.682, 0.31, 0.14) : Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
            border.color: on ? Qt.rgba(0.247, 0.682, 0.31, 0.35) : Qt.rgba(1, 1, 1, 0.1)
            RowLayout {
                anchors.centerIn: parent; spacing: 6
                Label { text: pill.on ? "✓" : "○"; color: pill.on ? "#4ed36a" : Theme.faint; font.pixelSize: 12 }
                Label { text: pill.on ? qsTr("Detected") : qsTr("Not found")
                    color: pill.on ? "#4ed36a" : Theme.faint
                    font.family: Theme.fontBody; font.pixelSize: 12; font.weight: Font.DemiBold }
            }
        }
    }

    // ---- shared button styles ----
    component PrimaryButton: Rectangle {
        property string label: ""
        property bool compact: false
        property bool withArrow: false
        signal clicked()
        implicitWidth: prow.implicitWidth + (compact ? 44 : 80)
        implicitHeight: compact ? 44 : 50
        radius: compact ? 10 : 11
        color: "#5fb8ff"
        transform: Translate { y: pbHover.containsMouse ? -2 : 0
            Behavior on y { NumberAnimation { duration: 120 } } }
        RowLayout {
            id: prow; anchors.centerIn: parent; spacing: 9
            Label { text: label; color: "#04141f"
                font.family: Theme.fontBody; font.pixelSize: compact ? 14 : 15; font.weight: Font.Bold }
            Canvas { visible: withArrow; Layout.preferredWidth: 16; Layout.preferredHeight: 16
                onPaint: { var c = getContext("2d"); c.reset(); c.strokeStyle = "#04141f";
                    c.lineWidth = 2.4; c.lineCap = "round"; c.lineJoin = "round";
                    c.beginPath(); c.moveTo(3, 8); c.lineTo(13, 8);
                    c.moveTo(9, 4); c.lineTo(13, 8); c.lineTo(9, 12); c.stroke(); } }
        }
        MouseArea { id: pbHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked() }
    }

    component TextButton: Item {
        property string label: ""
        signal clicked()
        implicitWidth: tb.implicitWidth + 16; implicitHeight: 34
        Label { id: tb; anchors.centerIn: parent; text: label
            color: tbHover.containsMouse ? "#fff" : Theme.muted
            font.family: Theme.fontBody; font.pixelSize: 13; font.weight: Font.DemiBold }
        MouseArea { id: tbHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked() }
    }
}
