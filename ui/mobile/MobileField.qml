import QtQuick
import QtQuick.Controls

Column {
    id: field

    property string label: ""
    property string placeholder: ""
    property string error: ""
    property alias text: input.text
    property alias validator: input.validator
    property alias inputMethodHints: input.inputMethodHints

    spacing: 6

    Text {
        visible: field.label.length > 0
        text: field.label
        font.pixelSize: MobileTheme.sizeCaption
        font.weight: Font.Medium
        color: MobileTheme.muted
    }

    TextField {
        id: input

        width: field.width
        implicitHeight: MobileTheme.touchHeight
        leftPadding: 12
        rightPadding: 12
        font.pixelSize: MobileTheme.sizeBody
        color: MobileTheme.accent
        placeholderText: field.placeholder
        placeholderTextColor: MobileTheme.muted
        selectByMouse: true

        background: Rectangle {
            radius: MobileTheme.radiusControl
            color: MobileTheme.soft
            border.width: input.activeFocus ? 2 : 1
            border.color: field.error.length > 0 ? MobileTheme.red
                        : input.activeFocus ? MobileTheme.accent : MobileTheme.line
        }
    }

    Text {
        visible: field.error.length > 0
        width: field.width
        text: field.error
        wrapMode: Text.Wrap
        font.pixelSize: MobileTheme.sizeCaption
        color: MobileTheme.red
    }
}
