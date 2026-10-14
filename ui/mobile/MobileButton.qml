import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string variant: "secondary"

    implicitHeight: MobileTheme.touchHeight
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: MobileTheme.sizeBody
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.variant === "primary" ? MobileTheme.accentText
             : control.variant === "danger" ? MobileTheme.red : MobileTheme.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: MobileTheme.radiusControl
        color: control.variant === "primary" ? MobileTheme.accent
             : control.down ? MobileTheme.hover : MobileTheme.panel
        border.width: 1
        border.color: control.variant === "primary" ? MobileTheme.accent
                    : control.variant === "danger" ? MobileTheme.red : MobileTheme.line
        opacity: control.enabled ? 1 : 0.4
    }
}
