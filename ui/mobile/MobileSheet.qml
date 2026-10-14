import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: sheet

    property string title: ""
    default property alias content: body.data

    parent: Overlay.overlay
    modal: true
    focus: true
    dim: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: parent ? parent.width : 360
    height: parent ? Math.min(parent.height * 0.92, body.implicitHeight + 32 + MobileTheme.navInset) : 480
    property real revealFactor: 0

    x: 0
    y: parent ? parent.height - height * sheet.revealFactor : 0

    enter: Transition {
        NumberAnimation {
            property: "revealFactor"
            from: 0
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "revealFactor"
            from: 1
            to: 0
            duration: 160
            easing.type: Easing.InCubic
        }
    }

    background: Rectangle {
        width: sheet.width
        height: sheet.height + MobileTheme.radiusSheet
        radius: MobileTheme.radiusSheet
        color: MobileTheme.panel
        border.width: 2
        border.color: MobileTheme.accent
    }

    contentItem: Flickable {
        clip: true
        focus: true
        Keys.onBackPressed: function(event) {
            sheet.close();
            event.accepted = true;
        }
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: body.implicitHeight + 32 + MobileTheme.navInset

        ColumnLayout {
            id: body

            x: 16
            y: 16
            width: parent.width - 32
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                visible: sheet.title.length > 0
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: sheet.title
                    wrapMode: Text.Wrap
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeTitle
                    font.weight: Font.DemiBold
                }

                MobileIconButton {
                    icon: "close"
                    onClicked: sheet.close()
                }
            }
        }
    }
}
