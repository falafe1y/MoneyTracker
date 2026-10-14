import QtQuick

Rectangle {
    id: track

    property real value: 0
    property color fill: MobileTheme.accent

    implicitWidth: 120
    implicitHeight: 8
    radius: height / 2
    color: MobileTheme.tableHeader
    border.width: 1
    border.color: MobileTheme.line
    clip: true

    Rectangle {
        height: track.height
        width: track.width * Math.max(0, Math.min(1, track.value))
        radius: height / 2
        color: track.value > 1 ? MobileTheme.red : track.fill
    }
}
