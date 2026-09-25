import QtQuick
import QtQuick.Controls

TextField {
    id: control

    property color appTextColor: "#031528"
    property color appMutedColor: "#687483"
    property color appPanelColor: "#FFFFF8"
    property color appSoftColor: "#FFFFF0"
    property color appLineColor: "#D8D7C7"
    property color appAccentColor: "#315C9B"
    property color appOnAccentColor: "#FFFFFF"
    property int controlHeight: 44

    implicitHeight: controlHeight
    leftPadding: 14
    rightPadding: 14
    color: enabled ? appTextColor : appMutedColor
    placeholderTextColor: appMutedColor
    selectionColor: appAccentColor
    selectedTextColor: appOnAccentColor
    font.pixelSize: 14
    selectByMouse: true

    background: Rectangle {
        radius: 11
        color: control.activeFocus ? appPanelColor : appSoftColor
        opacity: control.enabled ? 1 : 0.62
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? appAccentColor : appLineColor
    }
}
