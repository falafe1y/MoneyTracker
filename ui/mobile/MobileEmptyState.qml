import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: empty

    property string icon: ""
    property string svg: ""
    property string title: ""
    property string text: ""
    property string actionText: ""
    signal action()

    spacing: 8

    MobileIcon {
        Layout.alignment: Qt.AlignHCenter
        visible: empty.svg.length === 0
        name: empty.icon
        width: 32
        height: 32
        color: MobileTheme.accentSoft
    }

    MobileSvgIcon {
        Layout.alignment: Qt.AlignHCenter
        visible: empty.svg.length > 0
        source: empty.svg
        size: 32
        color: MobileTheme.accentSoft
    }

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: empty.title
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeHeading
        font.weight: Font.DemiBold
    }

    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: empty.text
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    MobileButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 4
        visible: empty.actionText.length > 0
        variant: "primary"
        text: empty.actionText
        onClicked: empty.action()
    }
}
