import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: control

    property var model: []
    property int currentIndex: 0
    property color accentColor: "#315EF4"
    property color textPrimary: "#111827"
    property color textSecondary: "#7A8494"
    property color borderColor: "#E5E9EF"
    property color surfaceColor: "#FFFFFF"
    property bool incomeMode: false

    readonly property string currentValue: {
        const item = model && currentIndex >= 0 ? model[currentIndex] : null
        return item && item.value !== undefined ? item.value : ""
    }

    implicitHeight: grid.implicitHeight

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        columns: width >= 470 ? 3 : 2
        rowSpacing: 8
        columnSpacing: 8

        Repeater {
            model: control.model

            delegate: Button {
                id: tile
                required property var modelData
                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.minimumWidth: 120
                hoverEnabled: true
                flat: true

                contentItem: RowLayout {
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: tile.modelData.label
                        color: control.textPrimary
                        font.pixelSize: 12
                        font.weight: tile.index === control.currentIndex ? Font.DemiBold : Font.Medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Rectangle {
                        Layout.preferredWidth: 17
                        Layout.preferredHeight: 17
                        radius: 8.5
                        visible: tile.index === control.currentIndex
                        color: control.incomeMode ? "#168A5B" : control.accentColor

                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            color: "white"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }

                background: Rectangle {
                    radius: 14
                    color: tile.down
                           ? "#F0F3F7"
                           : (tile.hovered ? "#F8FAFC" : control.surfaceColor)
                    border.width: tile.index === control.currentIndex ? 1.5 : 1
                    border.color: tile.index === control.currentIndex
                                  ? (control.incomeMode ? "#87CEAD" : "#9DB1FF")
                                  : control.borderColor
                }

                onClicked: control.currentIndex = tile.index
            }
        }
    }
}
