import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    property var controller
    property color panelColor: "#FFFFF8"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color accentColor: "#031528"
    property color errorColor: "#B94F48"
    property int selectedResult: -1
    property string localError: ""
    property string fixedAccountId: ""
    property string editingPositionId: ""
    property var editingInstrument: null
    property bool showingCurrentInstrument: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 28 : 620, 620)
    height: Math.min(parent ? parent.height - 28 : 690, 690)
    modal: true
    title: editingPositionId.length > 0
         ? qsTr("Редактирование инвестиционной позиции")
         : qsTr("Добавить инвестиционную позицию")
    standardButtons: Dialog.NoButton

    function openForNewPosition(accountId) {
        editingPositionId = "";
        editingInstrument = null;
        showingCurrentInstrument = false;
        fixedAccountId = accountId || "";
        selectedResult = -1;
        localError = "";
        searchField.text = "";
        quantityField.text = "";
        averagePriceField.text = "";
        accountBox.currentIndex = -1;
        if (controller) {
            const accounts = controller.investmentAccounts;
            for (let i = 0; i < accounts.length; ++i) {
                if (!fixedAccountId || accounts[i].id === fixedAccountId) {
                    accountBox.currentIndex = i;
                    break;
                }
            }
        }
        open();
    }

    function openForEdit(position) {
        if (!position)
            return;
        editingPositionId = position.id || "";
        editingInstrument = position;
        showingCurrentInstrument = true;
        fixedAccountId = "";
        selectedResult = 0;
        localError = "";
        searchField.text = position.symbol || position.isin || "";
        quantityField.text = position.quantityText || "";
        averagePriceField.text = position.averagePriceText || "";
        accountBox.currentIndex = -1;
        if (controller) {
            const accounts = controller.investmentAccounts;
            for (let i = 0; i < accounts.length; ++i) {
                if (accounts[i].id === position.accountId) {
                    accountBox.currentIndex = i;
                    break;
                }
            }
        }
        open();
    }

    onClosed: {
        editingPositionId = "";
        editingInstrument = null;
        showingCurrentInstrument = false;
        fixedAccountId = "";
    }

    background: Rectangle {
        color: dialog.panelColor
        radius: 18
        border.color: dialog.lineColor
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("Инвестиционный счёт")
            color: dialog.mutedColor
            font.pixelSize: 12
        }
        ComboBox {
            id: accountBox
            Layout.fillWidth: true
            model: dialog.controller ? dialog.controller.investmentAccounts : []
            textRole: "name"
            valueRole: "id"
            enabled: dialog.fixedAccountId.length === 0
        }

        Label {
            text: qsTr("Тикер или ISIN")
            color: dialog.mutedColor
            font.pixelSize: 12
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Например, SBER или RU0009029540")
                onAccepted: searchButton.clicked()
            }
            Button {
                id: searchButton
                text: dialog.controller && dialog.controller.investmentSearchBusy
                      ? qsTr("Ищем…") : qsTr("Найти")
                enabled: dialog.controller
                         && !dialog.controller.investmentSearchBusy
                         && searchField.text.trim().length >= 2
                onClicked: {
                    if (dialog.editingPositionId.length > 0)
                        averagePriceField.text = "";
                    dialog.showingCurrentInstrument = false;
                    dialog.selectedResult = -1;
                    dialog.localError = "";
                    dialog.controller.searchInvestmentInstruments(searchField.text);
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 180
            color: "transparent"
            border.color: dialog.lineColor
            radius: 10

            ListView {
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                spacing: 4
                model: dialog.showingCurrentInstrument
                     ? [dialog.editingInstrument]
                     : dialog.controller ? dialog.controller.investmentSearchResults : []
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: 68
                    radius: 8
                    color: index === dialog.selectedResult ? "#E4E8F1" : "transparent"

                    Column {
                        anchors.left: parent.left
                        anchors.right: priceLabel.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 3
                        Text {
                            width: parent.width
                            text: modelData.symbol + " · " + modelData.name
                            color: dialog.textColor
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: modelData.typeName
                                  + (modelData.isin ? " · " + modelData.isin : "")
                            color: dialog.mutedColor
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        id: priceLabel
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 110
                        horizontalAlignment: Text.AlignRight
                        text: dialog.showingCurrentInstrument
                              ? qsTr("Текущая позиция")
                              : modelData.hasPrice
                              ? modelData.priceText + " " + modelData.currency
                              : index === dialog.selectedResult
                                && dialog.controller.investmentQuoteBusy
                                ? qsTr("Получаем цену…") : qsTr("Выбрать")
                        color: dialog.textColor
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: !dialog.showingCurrentInstrument
                        onClicked: {
                            dialog.selectedResult = index;
                            dialog.localError = "";
                            dialog.controller.selectInvestmentSearchResult(index);
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                             && dialog.controller
                             && !dialog.controller.investmentSearchBusy
                    text: qsTr("Введите тикер или ISIN для поиска на Мосбирже")
                    color: dialog.mutedColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    width: Math.min(parent.width - 32, 360)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: quantityField
                Layout.fillWidth: true
                placeholderText: qsTr("Количество")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            TextField {
                id: averagePriceField
                Layout.fillWidth: true
                placeholderText: qsTr("Средняя цена (необязательно)")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.localError.length > 0
                  ? dialog.localError
                  : dialog.controller ? dialog.controller.investmentLastError : ""
            color: dialog.errorColor
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Отмена")
                onClicked: dialog.close()
            }
            Button {
                text: dialog.editingPositionId.length > 0
                    ? qsTr("Сохранить") : qsTr("Добавить")
                enabled: dialog.controller
                         && !dialog.controller.investmentQuoteBusy
                         && accountBox.currentIndex >= 0
                         && dialog.selectedResult >= 0
                onClicked: {
                    const result = dialog.editingPositionId.length > 0
                        ? dialog.controller.updateInvestmentPosition(
                              dialog.editingPositionId,
                              accountBox.currentValue,
                              dialog.showingCurrentInstrument
                                  ? -1 : dialog.selectedResult,
                              quantityField.text,
                              averagePriceField.text
                          )
                        : dialog.controller.addInvestmentPosition(
                              accountBox.currentValue,
                              dialog.selectedResult,
                              quantityField.text,
                              averagePriceField.text
                          );
                    if (result.ok)
                        dialog.close();
                    else
                        dialog.localError = result.error
                            || (dialog.editingPositionId.length > 0
                                ? qsTr("Не удалось обновить позицию")
                                : qsTr("Не удалось добавить позицию"));
                }
            }
        }
    }
}
