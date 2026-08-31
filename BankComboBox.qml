import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    property color surfaceColor: "#FFFFFF"
    property color surfaceHoverColor: "#F7F8FA"
    property color borderColor: "#E4E8EE"
    property color focusColor: "#315EF4"
    property color textPrimary: "#111827"
    property color textSecondary: "#7A8494"
    property string secondaryRole: ""
    property bool compact: false

    implicitHeight: compact ? 42 : 50
    leftPadding: 14
    rightPadding: 38
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true

    function itemAt(index) {
        if (index < 0 || !model)
            return null
        return model[index]
    }

    function roleText(item, roleName) {
        if (item === null || item === undefined)
            return ""
        if (!roleName || roleName.length === 0)
            return typeof item === "string" ? item : String(item)
        return item[roleName] !== undefined ? String(item[roleName]) : ""
    }

    function primaryText(index) {
        return roleText(itemAt(index), textRole)
    }

    function secondaryText(index) {
        return roleText(itemAt(index), secondaryRole)
    }

    contentItem: Row {
        spacing: 8
        Text {
            width: Math.max(0, parent.width - (secondaryLabel.visible ? secondaryLabel.width + 8 : 0))
            height: parent.height
            text: control.primaryText(control.currentIndex)
            color: control.textPrimary
            font.pixelSize: control.compact ? 13 : 14
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            id: secondaryLabel
            height: parent.height
            visible: text.length > 0 && control.compact
            text: control.secondaryText(control.currentIndex)
            color: control.textSecondary
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
    }

    indicator: Item {
        width: 18
        height: 18
        x: control.width - width - 13
        y: Math.round((control.height - height) / 2)

        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = control.textSecondary
                ctx.lineWidth = 1.6
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(5, 7)
                ctx.lineTo(9, 11)
                ctx.lineTo(13, 7)
                ctx.stroke()
            }
        }
    }

    background: Rectangle {
        radius: 13
        color: control.down ? "#F1F3F6" : (control.hovered ? control.surfaceHoverColor : control.surfaceColor)
        border.width: control.activeFocus || control.popup.visible ? 1.5 : 1
        border.color: control.activeFocus || control.popup.visible ? control.focusColor : control.borderColor
    }

    delegate: ItemDelegate {
        id: optionDelegate
        required property var modelData
        required property int index

        width: ListView.view ? ListView.view.width : control.width
        height: 54
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Row {
            spacing: 10

            Rectangle {
                width: 32
                height: 32
                radius: 10
                anchors.verticalCenter: parent.verticalCenter
                color: optionDelegate.index === control.currentIndex ? "#EEF2FF" : "#F4F6F8"

                Text {
                    anchors.centerIn: parent
                    text: control.primaryText(optionDelegate.index).slice(0, 1)
                    color: optionDelegate.index === control.currentIndex ? control.focusColor : control.textSecondary
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }
            }

            Column {
                width: Math.max(0, parent.width - 78)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    text: control.primaryText(optionDelegate.index)
                    color: control.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: control.secondaryText(optionDelegate.index)
                    color: control.textSecondary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            Text {
                width: 18
                height: parent.height
                visible: optionDelegate.index === control.currentIndex
                text: "✓"
                color: control.focusColor
                font.pixelSize: 14
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
        }

        background: Rectangle {
            radius: 11
            color: optionDelegate.hovered || optionDelegate.highlighted ? "#F6F8FB" : "transparent"
        }

    }

    popup: Popup {
        id: comboPopup
        y: control.height + 7
        width: Math.max(control.width, 254)
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 310)
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.leftMargin: 3
                radius: 15
                color: "#140D1726"
            }

            Rectangle {
                anchors.fill: parent
                anchors.bottomMargin: 3
                anchors.rightMargin: 3
                radius: 15
                color: control.surfaceColor
                border.width: 1
                border.color: control.borderColor
            }
        }
    }
}
