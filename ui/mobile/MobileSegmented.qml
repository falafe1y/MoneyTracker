import QtQuick
import QtQuick.Layouts

Rectangle {
    id: control

    property var options: []
    property var value
    signal activated(var chosen)

    implicitHeight: 40
    radius: MobileTheme.radiusControl
    color: MobileTheme.tableHeader
    border.width: 1
    border.color: MobileTheme.line

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3

        Repeater {
            model: control.options

            delegate: Rectangle {
                id: segment

                required property var modelData
                readonly property bool current: modelData.value === control.value

                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 7
                color: segment.current ? MobileTheme.panel : "transparent"
                border.width: segment.current ? 1 : 0
                border.color: MobileTheme.line

                Text {
                    anchors.centerIn: parent
                    text: segment.modelData.label
                    color: segment.current ? MobileTheme.accent : MobileTheme.muted
                    font.pixelSize: 13
                    font.weight: segment.current ? Font.DemiBold : Font.Medium
                }

                TapHandler {
                    onTapped: control.activated(segment.modelData.value)
                }
            }
        }
    }
}
