import QtQuick
import QtQuick.Effects

// A geometric shadow: no capture or blur of the card's text and contents.
RectangularShadow {
    anchors.fill: parent
    z: -1
    radius: parent.radius
    blur: 8
    spread: -1
    offset: Qt.vector2d(0, 4)
    color: "#18031528"
    cached: false
}
