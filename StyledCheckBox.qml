import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    property color appTextColor: "#031528"
    property color appMutedColor: "#687483"
    property color appSoftColor: "#FFFFF0"
    property color appLineColor: "#D8D7C7"
    property color appAccentColor: "#031528"
    property color appHoverColor: "#E4E8F1"
    property color appOnAccentColor: "#FFFFFF"

    hoverEnabled: true
    spacing: 10
    implicitHeight: 32

    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 6
        color: control.checked
               ? (control.hovered ? Qt.lighter(control.appAccentColor, 1.2)
                                  : control.appAccentColor)
               : (control.hovered ? control.appHoverColor : control.appSoftColor)
        opacity: control.enabled ? 1 : 0.55
        border.width: control.checked ? 0 : 1
        border.color: control.appLineColor

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: control.appOnAccentColor
            font.pixelSize: 15
            font.weight: Font.Bold
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? control.appTextColor : control.appMutedColor
        font.pixelSize: 14
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
