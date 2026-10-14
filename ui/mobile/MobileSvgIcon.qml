import QtQuick
import QtQuick.Effects

Item {
    id: icon

    property url source
    property color color: MobileTheme.accent
    property int size: 22

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        source: icon.source
        sourceSize: Qt.size(icon.size, icon.size)
        fillMode: Image.PreserveAspectFit
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: icon.color
        }
    }
}
