import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool primary: false
    property bool destructive: false
    // Outlined style: solid dark outline + hard offset shadow (no blur).
    property bool neo: false
    property bool dimWhenDisabled: true
    property int shadowDepth: 4
    property color appTextColor: "#031528"
    property color appMutedColor: "#687483"
    property color appPanelColor: "#FFFFF8"
    property color appSoftColor: "#FFFFF0"
    property color appLineColor: "#D8D7C7"
    property color appAccentColor: "#031528"
    property color appHoverColor: "#E4E8F1"
    property color appOnAccentColor: "#FFFFFF"
    property color appErrorColor: "#B94F48"
    property int cornerRadius: 9
    property int controlHeight: 40

    // Neo buttons sink into their shadow while pressed.
    readonly property int pressShift: neo && down && enabled ? 2 : 0

    activeFocusOnTab: true
    hoverEnabled: true
    implicitHeight: controlHeight
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 14
    font.weight: Font.Medium

    contentItem: Text {
        text: control.text
        color: !control.enabled && control.dimWhenDisabled ? control.appMutedColor
             : control.flat && control.destructive && !control.neo ? control.appErrorColor
             : control.primary || control.destructive ? control.appOnAccentColor
             : control.appTextColor
        opacity: control.enabled || !control.dimWhenDisabled ? 1 : 0.62
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        transform: Translate { y: control.pressShift }
    }

    background: Rectangle {
        transform: Translate { y: control.pressShift }

        SurfaceShadow { visible: !control.flat && !control.neo; blur: 6; spread: -1; offset: Qt.vector2d(0, 4); color: control.enabled ? "#14031528" : "#09031528" }
        HardShadow {
            visible: control.neo
            depth: control.shadowDepth - control.pressShift
            shadowColor: control.appAccentColor
        }

        radius: control.cornerRadius
        opacity: control.enabled || !control.dimWhenDisabled ? 1 : 0.55
        color: control.neo
               ? (control.destructive
                  ? (control.down || control.hovered
                     ? Qt.darker(control.appErrorColor, 1.08) : control.appErrorColor)
                  : control.primary
                    ? (control.down || control.hovered
                       ? Qt.lighter(control.appAccentColor, 1.2) : control.appAccentColor)
                    : control.hovered || control.down ? control.appHoverColor : control.appPanelColor)
               : control.flat
               ? (control.hovered ? control.appHoverColor : "transparent")
               : control.destructive
                 ? (control.down || control.hovered
                    ? Qt.darker(control.appErrorColor, 1.08) : control.appErrorColor)
                 : control.primary
                   ? (control.down || control.hovered
                      ? Qt.lighter(control.appAccentColor, 1.2) : control.appAccentColor)
                   : control.hovered ? control.appHoverColor : control.appSoftColor
        border.width: control.neo ? (control.visualFocus ? 3 : 2)
                    : control.visualFocus ? 2
                    : control.flat ? 0
                    : control.primary || control.destructive ? 0 : 1
        border.color: control.neo
                    // A filled button gets a light ring so its dark shadow stays readable.
                    ? (control.primary ? control.appPanelColor : control.appAccentColor)
                    : control.visualFocus
                      ? (control.primary || control.destructive
                         ? control.appOnAccentColor : control.appAccentColor)
                      : control.appLineColor
    }
}
