import QtQuick
import QtQuick.Controls

ScrollBar {
    id: control

    property color appAccentColor: "#315C9B"
    property color appTrackColor: "#E8E7DA"

    hoverEnabled: true
    implicitWidth: orientation === Qt.Vertical ? 8 : 100
    implicitHeight: orientation === Qt.Horizontal ? 8 : 100

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: control.appAccentColor
        opacity: control.pressed ? 0.85 : control.hovered ? 0.68 : 0.42
    }

    background: Rectangle {
        radius: 3
        color: control.appTrackColor
        opacity: control.active || control.hovered ? 0.42 : 0
    }
}
