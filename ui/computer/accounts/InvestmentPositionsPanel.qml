import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    SurfaceShadow { }

    id: panel
    signal contextMenuRequested(var position, var sourceItem, real x, real y)
    signal deleteRequested(var position)
    signal operationRequested(var position)
    signal editRequested(var position)
    property bool showEditButton: false

    property var controller
    property bool confirmDeletion: false
    property color panelColor: "#FFFFF8"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color errorColor: "#B94F48"

    component PanelButton: StyledButton {
        appTextColor: panel.textColor
        appMutedColor: panel.mutedColor
        appPanelColor: panel.panelColor
        appSoftColor: "#FFFFF0"
        appLineColor: panel.lineColor
        appAccentColor: panel.textColor
        appHoverColor: "#E4E8F1"
        appOnAccentColor: panel.panelColor
        appErrorColor: panel.errorColor
    }

    SurfaceShadow { }

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
            PanelButton {
                text: panel.controller && panel.controller.investmentRefreshing
                      ? qsTr("Обновляем…") : qsTr("Обновить цены")
                enabled: panel.controller && !panel.controller.investmentRefreshing
                onClicked: panel.controller.refreshInvestmentQuotes()
            }
        }

        Label {
            Layout.fillWidth: true; visible: panel.controller && panel.controller.investmentValuationIncomplete
            text: qsTr("Оценка капитала неполная: для части позиций нужна котировка, курс или ручная стоимость.")
            color: panel.errorColor; wrapMode: Text.WordWrap; font.pixelSize: 14
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: panel.controller ? panel.controller.investmentPositions : []
            delegate: Rectangle {
                SurfaceShadow { }

                required property var modelData
                required property int index
                width: ListView.view.width
                height: panel.width < 620 ? 144 : 104
                color: index % 2 ? "#FAF9EC" : "transparent"
                radius: 8

                GridLayout {
                    columns: panel.width < 620 ? 2 : 3
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    columnSpacing: 10
                    rowSpacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
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
                                  + modelData.quantityText + " " + modelData.quantityUnit
                                  + (modelData.direction === -1 ? " · " + qsTr("Продано") : "")
                            color: panel.mutedColor
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: panel.width < 620 ? 165 : 190
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: panel.width < 620 ? 165 : 250
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
                            text: modelData.valuationLabel
                            elide: Text.ElideRight
                            color: panel.mutedColor; horizontalAlignment: Text.AlignRight; font.pixelSize: 14
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.hasMarketQuote && !modelData.manualValuation
                                  ? modelData.priceText + (modelData.type === 2 ? "%" : modelData.type >= 9 ? " " + qsTr("пункт.") : " " + modelData.quoteCurrency)
                                    + " · " + modelData.quoteSourceLabel
                                  : modelData.valuationError
                            color: panel.mutedColor; horizontalAlignment: Text.AlignRight; font.pixelSize: 14
                            elide: Text.ElideRight
                            ToolTip.visible: hover.hovered && text.length > 0
                            ToolTip.text: text
                            HoverHandler { id: hover }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.manualValuation ? modelData.valuedAtText : modelData.quotedAtText
                            color: panel.mutedColor; horizontalAlignment: Text.AlignRight; font.pixelSize: 14
                        }
                    }
                    RowLayout {
                        Layout.columnSpan: panel.width < 620 ? 2 : 1
                        Layout.fillWidth: panel.width < 620
                        PanelButton { text: qsTr("Операция"); onClicked: panel.operationRequested(modelData) }
                        PanelButton { visible: panel.showEditButton; text: qsTr("Изменить"); onClicked: panel.editRequested(modelData) }
                        Item { visible: panel.width < 620; Layout.fillWidth: true }
                        PanelButton {
                        text: "×"
                        flat: true
                        destructive: true
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
