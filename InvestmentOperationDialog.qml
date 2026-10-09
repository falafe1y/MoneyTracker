import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    property var controller
    property var position: null
    property color panelColor: "#FFFFF8"
    property color softColor: "#FFFFF0"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color accentColor: "#031528"
    property color hoverColor: "#E4E8F1"
    property color errorColor: "#B94F48"
    property string localError: ""
    readonly property string kind: operationBox.currentValue || "buy"
    readonly property bool trade: kind === "buy" || kind === "sell"
    parent: Overlay.overlay; anchors.centerIn: parent; modal: true
    width: Math.min(parent ? parent.width - 32 : 640, 640)
    height: Math.min(parent ? parent.height - 32 : 760, 760)
    title: qsTr("Операция по инвестиции")
    standardButtons: Dialog.NoButton
    header: Label { text: dialog.title; color: dialog.textColor; font.pixelSize: 21; font.weight: Font.Bold; leftPadding: dialog.leftPadding; topPadding: dialog.topPadding }
    background: Rectangle { color: dialog.panelColor; radius: 16; border.width: 1; border.color: dialog.accentColor; HardShadow { depth: 6; shadowColor: dialog.accentColor } }
    function openFor(value) {
        position = value; localError = ""; operationBox.currentIndex = 0;
        quantityField.text = ""; priceField.text = ""; cashField.text = ""; remainingValueField.text = "";
        accruedField.text = "0"; faceField.text = value.faceValueText || "";
        dateField.text = Qt.formatDateTime(new Date(), "dd.MM.yyyy HH:mm:ss"); descriptionField.text = ""; open();
    }
    function operationKinds() {
        const sold = position && position.direction === -1;
        const kinds = [{name: sold ? qsTr("Продажа / увеличение позиции") : qsTr("Покупка / увеличение позиции"), value: "buy"},
            {name: sold ? qsTr("Покупка / уменьшение позиции") : qsTr("Продажа / уменьшение позиции"), value: "sell"}];
        if (position && position.type === 2) { kinds.push({name: qsTr("Купон"), value: "coupon"}); kinds.push({name: qsTr("Частичное погашение номинала"), value: "amortization"}); }
        if (position && [0,1,3,5,6].indexOf(position.type) >= 0) kinds.push({name: qsTr("Дивиденды / выплата фонда"), value: "dividend"});
        if (position && (position.type === 9 || position.pricing === "margined_option")) kinds.push({name: qsTr("Расчёт вариационной маржи"), value: "margin"});
        kinds.push({name: qsTr("Комиссия"), value: "fee"});
        if (position && [2,9,10].indexOf(position.type) >= 0) kinds.push({name: qsTr("Погашение / исполнение"), value: "expiry"});
        return kinds;
    }
    function submit() {
        const result = controller.recordInvestmentOperation({positionId: position.id, kind: kind,
            quantity: quantityField.text, price: priceField.text, cashAmount: cashField.text,
            remainingValue: remainingValueField.text, accruedAfter: accruedField.text, faceAfter: faceField.text,
            date: dateField.text, description: descriptionField.text});
        if (result.ok) close(); else localError = result.error || qsTr("Не удалось записать операцию");
    }
    component FormField: StyledTextField {
        Layout.minimumWidth: 0
        neo: true
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appOnAccentColor: dialog.panelColor
    }

    component FormCombo: StyledComboBox {
        Layout.minimumWidth: 0
        neo: true
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: dialog.hoverColor
    }

    component FormButton: StyledButton {
        neo: true
        cornerRadius: 8
        font.weight: Font.DemiBold
        appTextColor: dialog.textColor
        appMutedColor: dialog.mutedColor
        appPanelColor: dialog.panelColor
        appSoftColor: dialog.softColor
        appLineColor: dialog.lineColor
        appAccentColor: dialog.accentColor
        appHoverColor: dialog.hoverColor
        appOnAccentColor: dialog.panelColor
        appErrorColor: dialog.errorColor
    }


    contentItem: ColumnLayout {
        spacing: 16
        ScrollView {
            id: operationScroll
            contentWidth: availableWidth
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: operationScroll.availableWidth; spacing: 8
                Label { Layout.fillWidth: true; text: dialog.position ? dialog.position.symbol + " · " + dialog.position.accountName : ""; color: dialog.textColor; wrapMode: Text.WordWrap; font.pixelSize: 16 }
                FormCombo { id: operationBox; Layout.fillWidth: true; model: dialog.operationKinds(); textRole: "name"; valueRole: "value" }
                Label { text: qsTr("Дата и время фактической операции"); color: dialog.mutedColor; font.pixelSize: 14 }
                FormField { id: dateField; Layout.fillWidth: true; placeholderText: qsTr("дд.мм.гггг чч:мм:сс") }
                Label { text: qsTr("Изменение денег на счёте, %1").arg(dialog.position ? dialog.position.currency : ""); color: dialog.mutedColor; font.pixelSize: 14 }
                FormField { id: cashField; Layout.fillWidth: true; placeholderText: qsTr("Со знаком: например, −1000 или 500") }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Вводите фактическое движение денег с учётом комиссий и удержаний. Не добавляйте сюда стоимость всей позиции или обеспечение."); color: dialog.mutedColor; font.pixelSize: 14 }
                FormField { id: quantityField; Layout.fillWidth: true; visible: dialog.trade; placeholderText: qsTr("Количество операции") }
                FormField { id: priceField; Layout.fillWidth: true; visible: dialog.trade || dialog.kind === "margin"; placeholderText: dialog.kind === "margin" ? qsTr("Цена проведённого расчёта") : qsTr("Цена сделки в единицах котировки") }
                FormField { id: remainingValueField; Layout.fillWidth: true; visible: !!dialog.position && dialog.position.manualValuation && (dialog.trade || dialog.kind === "coupon" || dialog.kind === "amortization"); placeholderText: qsTr("Новая стоимость оставшейся позиции, %1").arg(dialog.position ? dialog.position.currency : "") }
                FormField { id: accruedField; Layout.fillWidth: true; visible: dialog.kind === "coupon" || dialog.kind === "amortization"; placeholderText: qsTr("НКД одной облигации после операции") }
                FormField { id: faceField; Layout.fillWidth: true; visible: dialog.kind === "amortization"; placeholderText: qsTr("Оставшийся номинал одной облигации") }
                Label { Layout.fillWidth: true; visible: dialog.kind === "coupon" || dialog.kind === "amortization"; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14; text: qsTr("Укажите НКД и оставшийся номинал после выплаты, чтобы выплаченная сумма не осталась в оценке облигации.") }
                Label { Layout.fillWidth: true; visible: dialog.kind === "margin"; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14; text: qsTr("Деньги изменятся на указанную сумму. Дальнейшая переоценка начнётся от цены и времени этого расчёта.") }
                Label { Layout.fillWidth: true; visible: dialog.kind === "expiry"; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14; text: qsTr("Позиция закроется. Укажите итоговое движение денег; при поставке добавьте полученные активы как уже имеющуюся позицию.") }
                FormField { id: descriptionField; Layout.fillWidth: true; placeholderText: qsTr("Описание (необязательно)") }
            }
        }
        Label { Layout.fillWidth: true; visible: dialog.localError.length > 0; text: dialog.localError; color: dialog.errorColor; wrapMode: Text.WordWrap; font.pixelSize: 14 }
        RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true }
            FormButton { text: qsTr("Отмена"); onClicked: dialog.close() }
            FormButton { primary: true; text: qsTr("Записать"); enabled: !!dialog.position && !!dialog.controller; onClicked: dialog.submit() }
        }
    }
}
