import QtQuick
import QtQuick.Layouts

Item {
    id: row

    property string title: ""
    property string subtitle: ""
    property string amount: ""
    property color amountColor: MobileTheme.accent
    property string glyph: ""
    property string icon: ""
    property string svg: ""
    property bool chevron: false
    property bool marked: false
    property bool first: false

    signal clicked()

    implicitHeight: Math.max(60, content.implicitHeight + 20)

    Rectangle {
        visible: !row.first
        width: parent.width
        height: 1
        color: MobileTheme.line
    }

    Rectangle {
        anchors.fill: parent
        visible: tap.pressed
        color: MobileTheme.hover
    }

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 12

        Rectangle {
            visible: row.glyph.length > 0 || row.icon.length > 0 || row.svg.length > 0
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter
            radius: 10
            color: MobileTheme.pale

            Text {
                anchors.centerIn: parent
                visible: row.glyph.length > 0
                text: row.glyph
                font.pixelSize: MobileTheme.sizeBody
                font.weight: Font.DemiBold
                color: MobileTheme.navSelected
            }

            MobileIcon {
                anchors.centerIn: parent
                visible: row.glyph.length === 0 && row.svg.length === 0 && row.icon.length > 0
                name: row.icon
                width: 18
                height: 18
                color: MobileTheme.navSelected
            }
            MobileSvgIcon {
                anchors.centerIn: parent
                visible: row.glyph.length === 0 && row.svg.length > 0
                source: row.svg
                size: 18
                color: MobileTheme.navSelected
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: row.title
                elide: Text.ElideRight
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeBody
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                visible: row.subtitle.length > 0
                text: row.subtitle
                elide: Text.ElideRight
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }
        }

        Text {
            visible: row.amount.length > 0
            Layout.alignment: Qt.AlignVCenter
            text: row.amount
            color: row.amountColor
            font.pixelSize: MobileTheme.sizeBody
            font.weight: Font.DemiBold
        }

        MobileIcon {
            visible: row.marked
            Layout.alignment: Qt.AlignVCenter
            name: "check"
            width: 18
            height: 18
            color: MobileTheme.accent
        }

        MobileIcon {
            visible: row.chevron
            Layout.alignment: Qt.AlignVCenter
            name: "chevronRight"
            width: 18
            height: 18
            color: MobileTheme.muted
        }
    }

    TapHandler {
        id: tap
        onTapped: row.clicked()
    }
}
