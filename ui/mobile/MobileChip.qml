import QtQuick
import QtQuick.Controls

Button {
    id: chip

    property bool selected: false

    implicitHeight: 32
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: MobileTheme.sizeCaption
    font.weight: Font.Medium

    contentItem: Text {
        text: chip.text
        font: chip.font
        color: chip.selected ? MobileTheme.accentText : MobileTheme.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 16
        color: chip.selected ? MobileTheme.accent : MobileTheme.panel
        border.width: 1
        border.color: chip.selected ? MobileTheme.accent : MobileTheme.line
    }
}
