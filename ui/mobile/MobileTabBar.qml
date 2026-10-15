import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

// Bottom navigation of the design: four tabs in five equal columns, the centre column holds the big round button.
// Icon offsets are frame pixels: x from the centre of a column, y from the top of the bar (frame y 960).
Rectangle {
    id: bar

    property var items: []
    property string activeKey: ""
    property string centerKey: "add"
    signal selected(string key)

    color: MobileTheme.designCard

    // Tab icons at the frame offsets (grid 7 px squares, wallet 20x15 with a 2 px stroke, bars 4 px, dots 4 px)
    component NavIcon: Item {
        id: icon

        property string kind: ""
        property color color: MobileTheme.designInk

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: 0
        height: 40

        Rectangle { visible: icon.kind === "grid"; x: -8; y: 16; width: 7; height: 7; color: icon.color }
        Rectangle { visible: icon.kind === "grid"; x: 1; y: 16; width: 7; height: 7; color: icon.color }
        Rectangle { visible: icon.kind === "grid"; x: -8; y: 25; width: 7; height: 7; color: icon.color }
        Rectangle { visible: icon.kind === "grid"; x: 1; y: 25; width: 7; height: 7; color: icon.color }

        Rectangle {
            visible: icon.kind === "wallet"
            x: -10
            y: 14
            width: 20
            height: 15
            radius: 3
            color: "transparent"
            border.width: 2
            border.color: icon.color
        }
        Rectangle { visible: icon.kind === "wallet"; x: -9; y: 19; width: 18; height: 2; color: icon.color }

        Rectangle { visible: icon.kind === "bars"; x: -8; y: 23; width: 4; height: 10; color: icon.color }
        Rectangle { visible: icon.kind === "bars"; x: -2; y: 16; width: 4; height: 17; color: icon.color }
        Rectangle { visible: icon.kind === "bars"; x: 4; y: 20; width: 4; height: 13; color: icon.color }

        Rectangle { visible: icon.kind === "dots"; x: -8; y: 20; width: 4; height: 4; radius: 2; color: icon.color }
        Rectangle { visible: icon.kind === "dots"; x: -2; y: 20; width: 4; height: 4; radius: 2; color: icon.color }
        Rectangle { visible: icon.kind === "dots"; x: 4; y: 20; width: 4; height: 4; radius: 2; color: icon.color }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: bar.items

            delegate: Item {
                id: tab

                required property var modelData
                readonly property bool current: modelData.key === bar.activeKey
                readonly property bool center: modelData.key === bar.centerKey
                readonly property bool available: !modelData.disabled

                Layout.fillWidth: true
                Layout.fillHeight: true

                // The big round button: accent when available, pale when the active asset has no operations
                Rectangle {
                    visible: tab.center
                    x: (tab.width - width) / 2
                    y: 24 - 28
                    width: 56
                    height: 56
                    radius: 28
                    color: tab.available ? MobileTheme.accent : MobileTheme.pale

                    RectangularShadow {
                        anchors.fill: parent
                        z: -1
                        radius: parent.radius
                        blur: 2.3
                        spread: 0
                        offset: Qt.vector2d(0, 2)
                        color: MobileTheme.designShadow
                        cached: false
                    }

                    MobileIcon {
                        anchors.centerIn: parent
                        name: "plus"
                        width: 24
                        height: 24
                        color: tab.available ? MobileTheme.designCream : MobileTheme.muted
                    }

                    TapHandler {
                        enabled: tab.available
                        onTapped: bar.selected(tab.modelData.key)
                    }
                }

                NavIcon {
                    visible: !tab.center
                    kind: tab.modelData.icon || ""
                    color: tab.current ? MobileTheme.designNavy : MobileTheme.designInk
                }

                Text {
                    id: label

                    visible: !tab.center
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 53.3 - metrics.ascent
                    text: tab.modelData.title
                    color: tab.current ? MobileTheme.designNavy : MobileTheme.designMuted
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: tab.current ? 0 : (tab.modelData.spacing || 0)

                    FontMetrics {
                        id: metrics
                        font: label.font
                    }
                }

                TapHandler {
                    enabled: !tab.center
                    onTapped: bar.selected(tab.modelData.key)
                }
            }
        }
    }
}
