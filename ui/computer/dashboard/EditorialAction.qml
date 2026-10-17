import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool outlined: false
    property bool arrow: true
    property bool underline: false
    property bool mono: true
    property int textSize: 12
    property color ink: "#031528"
    implicitWidth: label.implicitWidth + (arrow ? 28 : 0) + (outlined ? 32 : 0)
    implicitHeight: outlined ? 40 : 32
    leftPadding: outlined ? 16 : 0
    rightPadding: outlined ? 16 : 0
    topPadding: 0; bottomPadding: 0
    hoverEnabled: true
    font.family: mono ? "Cousine" : "Arimo"
    font.pixelSize: textSize
    font.bold: true
    contentItem: Item {
        EditorialText {
            id: label
            anchors.left: parent.left; anchors.right: arrowItem.left
            anchors.rightMargin: control.arrow ? 4 : 0
            height: parent.height
            text: control.text
            font.family: control.font.family
            font.pixelSize: control.font.pixelSize
            font.bold: control.font.bold
            minimumPixelSize: control.outlined ? 14 : control.textSize
            fontSizeMode: control.outlined ? Text.Fit : Text.FixedSize
            color: control.outlined && control.hovered ? "#f3eddf" : control.hovered ? "#b84932" : control.ink
            font.underline: control.hovered && !control.outlined
        }
        EditorialArrow {
            id: arrowItem
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            width: control.arrow ? (control.outlined ? 16 : 24) : 0
            height: width
            visible: control.arrow
            ink: label.color
        }
    }
    background: Rectangle {
        color: control.outlined && control.hovered ? control.ink : "transparent"
        border.width: control.outlined || control.activeFocus ? 1 : 0
        border.color: control.activeFocus ? "#b84932" : control.ink
        Rectangle { visible: control.underline; anchors.bottom: parent.bottom; width: parent.width; height: 1; color: control.ink }
    }
}
