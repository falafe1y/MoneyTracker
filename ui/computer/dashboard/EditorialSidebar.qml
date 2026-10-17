import QtQuick
import QtQuick.Controls

Rectangle {
    id: sidebar
    objectName: "editorialSidebar"
    required property var host
    readonly property bool rail: host.width <= 850
    readonly property bool shortWindow: host.height <= 800 && !rail
    readonly property int pad: rail ? 12 : host.width <= 1500 ? 16 : 24
    readonly property int topPad: rail ? 20 : shortWindow ? 16 : 24
    readonly property int brandHeight: rail ? 80 : shortWindow ? 96 : 104
    readonly property int navHeight: shortWindow ? 44 : 48
    color: "#f9f6ea"
    clip: true
    Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: "#031528" }
    Item {
        id: brand
        x: sidebar.pad; y: sidebar.topPad
        width: parent.width - sidebar.pad * 2; height: sidebar.brandHeight
        EditorialText {
            width: parent.width; height: sidebar.rail ? 40 : 64
            text: "Vexa."; font.family: "Gelasio"; font.italic: true; font.bold: true
            font.pixelSize: sidebar.rail ? 20 : 48; font.letterSpacing: sidebar.rail ? -1 : -2
            fontSizeMode: Text.Fit; minimumPixelSize: sidebar.rail ? 16 : 48
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: sidebar.host.page = "overview" }
        Rectangle { y: sidebar.rail ? 64 : 80; width: parent.width; height: 1; color: "#031528" }
    }
    Column {
        id: navigation
        x: sidebar.pad; y: brand.y + brand.height
        width: parent.width - sidebar.pad * 2
        Repeater {
            model: [ ["overview", qsTr("Обзор")], ["categories", qsTr("Категории")],
                ["goals", qsTr("Цели")], ["notes", qsTr("Заметки")],
                ["trajectory", qsTr("Траектория")], ["budgets", qsTr("Бюджеты")],
                ["projects", qsTr("Проекты")], ["analytics", qsTr("Аналитика")] ]
            delegate: Button {
                id: nav
                required property var modelData
                required property int index
                readonly property bool selected: sidebar.host.page === modelData[0]
                objectName: "editorialNav_" + modelData[0]
                width: navigation.width; height: sidebar.navHeight
                padding: 0; hoverEnabled: true
                Accessible.name: modelData[1]
                contentItem: Item {
                    EditorialText {
                        x: sidebar.rail ? 0 : 0; width: sidebar.rail ? parent.width : 24; height: parent.height
                        text: String(nav.index + 1).padStart(2, "0")
                        font.family: "Cousine"; font.pixelSize: sidebar.rail ? 14 : 12
                        horizontalAlignment: sidebar.rail ? Text.AlignHCenter : Text.AlignLeft
                        color: nav.selected ? "#b84932" : sidebar.rail ? "#031528" : "#59616a"
                    }
                    EditorialText {
                        visible: !sidebar.rail
                        x: 32; width: parent.width - 64; height: parent.height
                        text: nav.modelData[1]; font.bold: nav.selected
                        color: nav.hovered ? "#b84932" : "#031528"
                    }
                    EditorialArrow { visible: nav.selected && !sidebar.rail; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter }
                }
                background: Rectangle {
                    color: "transparent"
                    border.width: nav.activeFocus ? 1 : 0; border.color: "#b84932"
                    Rectangle { visible: nav.selected; anchors.bottom: parent.bottom; height: 1; width: parent.width; color: "#031528" }
                }
                onClicked: sidebar.host.page = modelData[0]
            }
        }
    }
    Column {
        x: sidebar.pad; width: parent.width - sidebar.pad * 2
        anchors.bottom: parent.bottom; anchors.bottomMargin: sidebar.topPad
        Rectangle {
            visible: !sidebar.rail && !sidebar.shortWindow && sidebar.height >= 860
            width: parent.width; height: 128; color: "transparent"; border.color: "#031528"
            EditorialText {
                x: sidebar.host.width <= 1500 ? 12 : 16; y: 16; width: parent.width - x * 2; height: 40
                text: "Keep count."; font.family: "Gelasio"; font.italic: true
                font.pixelSize: sidebar.host.width <= 1500 ? 26 : 28
            }
            EditorialText {
                x: 16; y: 64; width: parent.width - 32; height: 32
                font.family: "Cousine"; font.pixelSize: 12
                text: qsTr("Для тех, кто уважает\nсвои деньги")
            }
        }
        Item { width: 1; height: 16 }
        Button {
            id: settings
            objectName: "editorialNav_settings"
            width: parent.width; height: 40; padding: 0; hoverEnabled: true
            Accessible.name: qsTr("Настройки")
            contentItem: Item {
                EditorialText {
                    width: 16; height: 24; anchors.verticalCenter: parent.verticalCenter
                    x: sidebar.rail ? (parent.width - width) / 2 : 0
                    text: "⚙"; font.pixelSize: 14; color: "#59616a"
                }
                EditorialText { visible: !sidebar.rail; x: 32; width: parent.width - 32; height: parent.height; text: qsTr("Настройки"); font.pixelSize: 14; color: settings.hovered ? "#b84932" : "#031528" }
            }
            background: Rectangle { color: "transparent"; border.width: settings.activeFocus ? 1 : 0; border.color: "#b84932" }
            onClicked: sidebar.host.page = "settings"
        }
        Item {
            width: parent.width; height: sidebar.rail ? 0 : 48; visible: !sidebar.rail
            Rectangle { width: parent.width; height: 1; color: "#031528" }
            EditorialText { height: parent.height; text: "VEXA"; font.family: "Cousine"; font.pixelSize: 12 }
            EditorialText { anchors.right: parent.right; height: parent.height; text: "© 2026"; font.family: "Cousine"; font.pixelSize: 12 }
        }
    }
}
