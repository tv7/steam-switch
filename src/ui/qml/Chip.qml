// Small meta chip (the mock's .chip): ghost by default, or filled with a brand
// color, or amber-outlined for account-readiness notes.
import QtQuick
import QtQuick.Controls
import Orbit

Rectangle {
    id: chip
    property string text: ""
    property string fillColor: ""      // "" = ghost
    property bool amber: false         // account-ready style (.chip.am)

    implicitWidth: label.implicitWidth + 22
    implicitHeight: 26
    radius: height / 2
    color: fillColor.length ? fillColor
         : amber ? Qt.rgba(0.96, 0.62, 0.043, 0.10) : Qt.rgba(1, 1, 1, 0.07)
    border.width: 1
    border.color: fillColor.length ? "transparent"
                : amber ? Qt.rgba(0.96, 0.62, 0.043, 0.40) : Theme.line

    Label {
        id: label
        anchors.centerIn: parent
        text: chip.text
        color: chip.fillColor.length ? Theme.fgOn(chip.fillColor)
             : chip.amber ? Theme.warn : Theme.muted
        font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.Bold
    }
}
