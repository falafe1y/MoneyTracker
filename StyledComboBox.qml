import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    property color appTextColor: "#031528"
    property color appMutedColor: "#687483"
    property color appPanelColor: "#FFFFF8"
    property color appSoftColor: "#FFFFF0"
    property color appLineColor: "#D8D7C7"
    property color appAccentColor: "#315C9B"
    property color appHoverColor: "#E4E8F1"
    property bool neo: false
    readonly property int neoPressShift: neo && (pressed || popup.visible) ? 2 : 0
    property int controlHeight: 44
    property string emptyText: ""

    hoverEnabled: true
    implicitHeight: controlHeight
    leftPadding: 14
    rightPadding: 42
    font.pixelSize: 14

    contentItem: Text {
        text: control.count > 0 ? control.displayText : control.emptyText
        color: control.enabled ? control.appTextColor : control.appMutedColor
        transform: Translate { y: control.neoPressShift }
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 15
        y: (control.height - height) / 2 - 1
        transform: Translate { y: control.neoPressShift }
        text: control.popup.visible ? "⌃" : "⌄"
        color: control.enabled ? control.appAccentColor : control.appMutedColor
        font.pixelSize: 18
        font.weight: Font.DemiBold
    }

    background: Rectangle {
        transform: Translate { y: control.neoPressShift }
        HardShadow {
            visible: control.neo
            depth: 4 - control.neoPressShift
            shadowColor: control.appAccentColor
        }
        radius: control.neo ? 8 : 11
        opacity: control.enabled ? 1 : 0.62
        color: control.neo
               ? (control.hovered || control.pressed || control.popup.visible
                  ? control.appHoverColor : control.appPanelColor)
               : control.pressed || control.popup.visible
                 ? control.appPanelColor
                 : control.hovered ? control.appHoverColor : control.appSoftColor
        border.width: control.neo ? (control.visualFocus ? 3 : 2)
                    : control.visualFocus || control.popup.visible ? 2 : 1
        border.color: control.neo || control.visualFocus || control.popup.visible
                      ? control.appAccentColor : control.appLineColor
    }

    delegate: ItemDelegate {
        id: optionDelegate
        required property int index
        width: control.popup.width - 12
        height: 40
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: control.textAt(optionDelegate.index)
            color: control.neo && (optionDelegate.highlighted || optionDelegate.hovered)
                   ? control.appPanelColor : control.appTextColor
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: optionDelegate.highlighted || optionDelegate.hovered
                   ? (control.neo ? control.appAccentColor : control.appHoverColor) : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 260)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2
            ScrollBar.vertical: StyledScrollBar {
                appAccentColor: control.appAccentColor
                appTrackColor: control.appLineColor
            }
        }

        background: Rectangle {
            HardShadow { visible: control.neo; depth: 4; shadowColor: control.appAccentColor }
            radius: 12
            color: control.appPanelColor
            border.width: control.neo ? 2 : 1
            border.color: control.neo ? control.appAccentColor : control.appLineColor
        }
    }
}
