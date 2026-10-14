import QtQuick
import QtQuick.Layouts

Rectangle {
    id: bar

    property var items: []
    property string activeKey: ""
    signal selected(string key)

    color: MobileTheme.panel

    Rectangle {
        width: parent.width
        height: 1
        color: MobileTheme.line
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 1
        spacing: 0

        Repeater {
            model: bar.items

            delegate: Item {
                id: tab

                required property var modelData
                readonly property bool current: modelData.key === bar.activeKey

                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 56
                        implicitHeight: 28
                        radius: 14
                        color: tab.current ? MobileTheme.pale : "transparent"

                        MobileSvgIcon {
                            anchors.centerIn: parent
                            visible: !!tab.modelData.svg
                            source: tab.modelData.svg || ""
                            size: 22
                            color: tab.current ? MobileTheme.navSelected : MobileTheme.accentSoft
                        }
                        MobileIcon {
                            anchors.centerIn: parent
                            visible: !tab.modelData.svg
                            name: tab.modelData.icon || ""
                            width: 22
                            height: 22
                            color: tab.current ? MobileTheme.navSelected : MobileTheme.accentSoft
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: tab.modelData.title
                        font.pixelSize: MobileTheme.sizeLabel
                        font.weight: tab.current ? Font.DemiBold : Font.Medium
                        color: tab.current ? MobileTheme.navSelected : MobileTheme.muted
                    }
                }

                TapHandler {
                    onTapped: bar.selected(tab.modelData.key)
                }
            }
        }
    }
}
