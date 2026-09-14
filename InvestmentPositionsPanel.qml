import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    signal contextMenuRequested(var position, var sourceItem, real x, real y)
    signal deleteRequested(var position)

    property var controller
    property bool confirmDeletion: false
    property color panelColor: "#FFFFF8"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color errorColor: "#B94F48"

    color: panelColor
    radius: 14
    border.color: lineColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Позиции")
                color: panel.textColor
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Button {
                text: panel.controller && panel.controller.investmentRefreshing
                      ? qsTr("Обновляем…") : qsTr("Обновить цены")
                enabled: panel.controller && !panel.controller.investmentRefreshing
                onClicked: panel.controller.refreshInvestmentQuotes()
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: panel.controller ? panel.controller.investmentPositions : []
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 72
                color: index % 2 ? "#FAF9EC" : "transparent"
                radius: 8

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: modelData.symbol + " · " + modelData.name
                            color: panel.textColor
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.accountName + " · "
                                  + qsTr("Количество: %1").arg(modelData.quantityText)
                            color: panel.mutedColor
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 150
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: modelData.hasQuote
                                  ? modelData.marketValueText + " " + modelData.currency
                                  : qsTr("Цена недоступна")
                            color: panel.textColor
                            horizontalAlignment: Text.AlignRight
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.hasQuote
                                  ? qsTr("%1 %2 за единицу")
                                        .arg(modelData.priceText).arg(modelData.currency)
                                  : ""
                            color: panel.mutedColor
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 11
                        }
                    }
                    Button {
                        text: "×"
                        flat: true
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Удалить позицию")
                        onClicked: {
                            if (panel.confirmDeletion)
                                panel.deleteRequested(modelData);
                            else if (panel.controller)
                                panel.controller.deleteInvestmentPosition(modelData.id);
                        }
                    }
                }

                MouseArea {
                    id: positionMenuArea
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onPressed: function (mouse) {
                        if (mouse.button === Qt.RightButton)
                            panel.contextMenuRequested(
                                modelData,
                                positionMenuArea,
                                mouse.x,
                                mouse.y
                            );
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: qsTr("Позиций пока нет")
                color: panel.mutedColor
            }
        }
    }
}
