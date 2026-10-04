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
    // Outlined style: solid dark outline + hard offset shadow (no blur).
    property bool neo: false

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
        HardShadow {
            visible: control.neo
            depth: 3
            shadowColor: control.activeFocus ? control.appAccentColor : control.appTextColor
        }

        radius: control.neo ? 8 : 11
        color: control.activeFocus ? appPanelColor : appSoftColor
        opacity: control.enabled ? 1 : 0.62
        border.width: control.neo ? 2 : control.activeFocus ? 2 : 1
        border.color: control.neo ? control.appTextColor
                    : control.activeFocus ? appAccentColor : appLineColor
    }
}
