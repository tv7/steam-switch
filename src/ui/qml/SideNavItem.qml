// Sidebar nav row (drawn icon + label) with hover/active states, ORBIT styling.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SteamSwitch

Rectangle {
    id: root
    property string label: ""
    property string kind: ""          // "library" | "accounts" | "settings"
    property bool active: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 42
    radius: 9
    color: active ? Qt.rgba(1, 1, 1, 0.07)
                  : (hover.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
    Behavior on color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12; anchors.rightMargin: 12
        spacing: 13

        Canvas {
            id: icon
            Layout.preferredWidth: 19; Layout.preferredHeight: 19
            property color stroke: root.active ? "#ffffff" : Theme.muted
            onStrokeChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = stroke;
                ctx.lineWidth = 1.7;
                ctx.lineJoin = "round"; ctx.lineCap = "round";
                if (root.kind === "library") {
                    var pads = [[4,4],[12,4],[4,12],[12,12]];
                    for (var i = 0; i < pads.length; i++) {
                        ctx.beginPath();
                        ctx.roundedRect(pads[i][0], pads[i][1], 5.5, 5.5, 1.4, 1.4);
                        ctx.stroke();
                    }
                } else if (root.kind === "accounts") {
                    ctx.beginPath(); ctx.arc(9.5, 6.5, 2.9, 0, Math.PI * 2); ctx.stroke();
                    ctx.beginPath(); ctx.arc(9.5, 16, 5.6, Math.PI, 0); ctx.stroke();
                } else if (root.kind === "settings") {
                    var ys = [5.5, 9.5, 13.5];
                    var cx = [7, 12, 6];
                    for (var j = 0; j < 3; j++) {
                        ctx.beginPath(); ctx.moveTo(2.5, ys[j]); ctx.lineTo(16.5, ys[j]); ctx.stroke();
                        ctx.beginPath(); ctx.arc(cx[j], ys[j], 1.9, 0, Math.PI * 2);
                        ctx.fillStyle = Theme.shell; ctx.fill(); ctx.stroke();
                    }
                }
            }
        }

        Label {
            text: root.label
            color: root.active ? "#fff" : Theme.muted
            font.family: Theme.fontBody; font.pixelSize: 14; font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
    }
    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true
        cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
