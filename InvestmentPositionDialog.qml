import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    property var controller
    property color panelColor: "#FFFFF8"
    property color softColor: "#FFFFF0"
    property color textColor: "#031528"
    property color mutedColor: "#687483"
    property color lineColor: "#D8D7C7"
    property color accentColor: "#031528"
    property color hoverColor: "#E4E8F1"
    property color errorColor: "#B94F48"
    property int selectedResult: -1
    property string localError: ""
    property string fixedAccountId: ""
    property string editingPositionId: ""
    property var editingInstrument: null
    property bool showingCurrentInstrument: false
    readonly property bool manualInstrument: sourceBox.currentIndex === 1
    readonly property var selectedInstrument: showingCurrentInstrument ? editingInstrument
        : controller && selectedResult >= 0 && selectedResult < controller.investmentSearchResults.length
          ? controller.investmentSearchResults[selectedResult] : null
    readonly property int instrumentType: manualInstrument ? typeBox.currentValue
        : selectedInstrument ? selectedInstrument.type : -1
    readonly property string pricing: selectedInstrument ? selectedInstrument.pricing : "manual"
    readonly property bool derivative: instrumentType === 9 || instrumentType === 10
    readonly property bool margined: pricing === "future" || pricing === "margined_option"
    readonly property var selectedAccount: accountBox.currentIndex >= 0 && controller
        ? controller.investmentAccounts[accountBox.currentIndex] : null
    readonly property string accountCurrency: selectedAccount ? selectedAccount.currency : "RUB"

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 680, 680)
    height: Math.min(parent ? parent.height - 32 : 820, 820)
    modal: true
    title: editingPositionId ? qsTr("Редактирование позиции") : qsTr("Добавить инвестиционный инструмент")
    standardButtons: Dialog.NoButton
    header: Label {
        text: dialog.title; color: dialog.textColor; font.pixelSize: 21; font.weight: Font.Bold
        leftPadding: dialog.leftPadding; rightPadding: dialog.rightPadding; topPadding: dialog.topPadding
        wrapMode: Text.WordWrap
    }
    background: Rectangle {
        color: dialog.panelColor; radius: 16; border.width: 1; border.color: dialog.accentColor
        HardShadow { depth: 6; shadowColor: dialog.accentColor }
    }

    function resetFields() {
        localError = ""; selectedResult = -1; searchField.text = ""; quantityField.text = "";
        averagePriceField.text = ""; nameField.text = ""; symbolField.text = ""; manualValueField.text = "";
        referencePriceField.text = ""; referenceDateField.text = ""; marginField.text = "0";
        adjustmentField.text = "0"; fxField.text = ""; cashField.text = "";
        sourceBox.currentIndex = 0; directionBox.currentIndex = 0; entryBox.currentIndex = 0;
        optionSchemeBox.currentIndex = 0; rightBox.currentIndex = 0; strikeField.text = ""; maturityField.text = ""; underlyingField.text = "";
        manualCheck.checked = false; accountBox.currentIndex = -1;
    }
    function chooseAccount(id) {
        if (!controller) return;
        const accounts = controller.investmentAccounts;
        for (let i = 0; i < accounts.length; ++i) if (!id || accounts[i].id === id) { accountBox.currentIndex = i; break; }
    }
    function openForNewPosition(accountId) {
        editingPositionId = ""; editingInstrument = null; showingCurrentInstrument = false;
        fixedAccountId = accountId || ""; resetFields(); chooseAccount(fixedAccountId); open();
        Qt.callLater(function() { searchField.forceActiveFocus(); });
    }
    function openForEdit(position) {
        if (!position) return;
        resetFields(); editingPositionId = position.id; editingInstrument = Object.assign({}, position, {type: position.instrumentType === undefined ? position.type : position.instrumentType});
        showingCurrentInstrument = true; fixedAccountId = ""; selectedResult = 0;
        chooseAccount(position.accountId); searchField.text = position.symbol;
        quantityField.text = position.quantityText; averagePriceField.text = position.averagePriceText;
        directionBox.currentIndex = position.direction === -1 ? 1 : 0;
        manualCheck.checked = position.manualValuation; manualValueField.text = position.manualValueText || "";
        referencePriceField.text = position.referencePriceText || "";
        referenceDateField.text = position.referenceDate ? Qt.formatDateTime(new Date(position.referenceDate), "dd.MM.yyyy HH:mm:ss") : "";
        marginField.text = position.blockedMarginText || "0"; adjustmentField.text = position.adjustmentText || "0";
        fxField.text = position.fxRateText || ""; open();
    }
    function submit() {
        const result = controller.saveInvestmentPosition({
            id: editingPositionId, accountId: accountBox.currentValue,
            searchIndex: manualInstrument ? -2 : showingCurrentInstrument ? -1 : selectedResult,
            instrumentType: typeBox.currentValue, name: nameField.text, symbol: symbolField.text,
            quantity: quantityField.text, averagePrice: averagePriceField.text,
            direction: derivative && directionBox.currentIndex === 1 ? -1 : 1,
            manualValuation: manualInstrument || manualCheck.checked, manualValue: manualValueField.text,
            referencePrice: referencePriceField.text, referenceDate: referenceDateField.text,
            blockedMargin: marginField.text, adjustment: adjustmentField.text, fxRate: fxField.text,
            purchase: !editingPositionId && entryBox.currentIndex === 1, cashAmount: cashField.text,
            optionPricing: optionSchemeBox.currentIndex === 1 ? "margined_option" : "premium_option",
            optionRight: rightBox.currentIndex === 1 ? "P" : "C", strike: strikeField.text,
            maturity: maturityField.text, underlying: underlyingField.text
        });
        if (result.ok) close(); else localError = result.error || qsTr("Не удалось сохранить позицию");
    }
    onClosed: { editingPositionId = ""; editingInstrument = null; showingCurrentInstrument = false; fixedAccountId = ""; }
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
            id: positionScroll
            contentWidth: availableWidth
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: positionScroll.availableWidth; spacing: 8
                Label { text: qsTr("Брокерский счёт"); color: dialog.mutedColor; font.pixelSize: 14 }
                FormCombo {
                    id: accountBox; Layout.fillWidth: true; model: dialog.controller ? dialog.controller.investmentAccounts : []
                    textRole: "name"; valueRole: "id"; enabled: !dialog.fixedAccountId && !(dialog.editingInstrument && dialog.editingInstrument.hasOperations)
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: qsTr("Остаток брокерского счёта — только деньги, включая заблокированное обеспечение. Стоимость позиций программа прибавит отдельно.")
                }
                Label {
                    Layout.fillWidth: true; visible: !!dialog.selectedAccount; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: dialog.selectedAccount ? qsTr("Деньги: %1 · заблокировано: %2 · свободно: %3 %4")
                        .arg(Number(dialog.selectedAccount.cashMinor / 100).toLocaleString(Qt.locale("ru_RU"), "f", 2))
                        .arg(Number(dialog.selectedAccount.blockedMarginMinor / 100).toLocaleString(Qt.locale("ru_RU"), "f", 2))
                        .arg(Number(dialog.selectedAccount.freeCashMinor / 100).toLocaleString(Qt.locale("ru_RU"), "f", 2)).arg(dialog.accountCurrency) : ""
                }
                FormCombo {
                    id: sourceBox; Layout.fillWidth: true; visible: !dialog.editingPositionId
                    model: [qsTr("Найти на бирже"), qsTr("Добавить с ручной оценкой")]
                    onActivated: { dialog.localError = ""; if (currentIndex === 1) manualCheck.checked = true; }
                }
                RowLayout {
                    Layout.fillWidth: true; visible: !dialog.manualInstrument
                    FormField { id: searchField; Layout.fillWidth: true; placeholderText: qsTr("Название, обозначение или ISIN"); onAccepted: searchButton.clicked() }
                    FormButton {
                        id: searchButton; text: dialog.controller && dialog.controller.investmentSearchBusy ? qsTr("Ищем…") : qsTr("Найти")
                        enabled: dialog.controller && !dialog.controller.investmentSearchBusy && searchField.text.trim().length >= 2
                            && !(dialog.editingInstrument && dialog.editingInstrument.hasOperations)
                        onClicked: { dialog.showingCurrentInstrument = false; dialog.selectedResult = -1; dialog.localError = "";
                            dialog.controller.searchInvestmentInstruments(searchField.text); }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 180; visible: !dialog.manualInstrument
                    color: dialog.softColor; radius: 12; border.width: 1; border.color: dialog.lineColor
                    ListView {
                        anchors.fill: parent; anchors.margins: 8; clip: true; spacing: 4
                        model: dialog.showingCurrentInstrument ? [dialog.editingInstrument] : dialog.controller ? dialog.controller.investmentSearchResults : []
                        delegate: Rectangle {
                            required property var modelData; required property int index
                            width: ListView.view.width; height: 60; radius: 8
                            color: index === dialog.selectedResult ? dialog.hoverColor : "transparent"
                            Column {
                                anchors.fill: parent; anchors.margins: 8; spacing: 4
                                Text { width: parent.width; text: modelData.symbol + " · " + modelData.name; color: dialog.textColor; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { width: parent.width; text: modelData.typeName + (modelData.hasPrice ? " · " + modelData.priceText + (modelData.type === 2 ? "%" : modelData.type >= 9 ? " " + qsTr("пункт.") : " " + modelData.currency) : ""); color: dialog.mutedColor; font.pixelSize: 14; elide: Text.ElideRight }
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor; enabled: !dialog.showingCurrentInstrument && !dialog.controller.investmentQuoteBusy
                                onClicked: { dialog.selectedResult = index; dialog.localError = ""; manualCheck.checked = false;
                                    referencePriceField.text = ""; referenceDateField.text = ""; averagePriceField.text = "";
                                    dialog.controller.selectInvestmentSearchResult(index); }
                            }
                        }
                        Label { anchors.centerIn: parent; visible: parent.count === 0; text: qsTr("Акции, облигации, фонды, металлы, валюты и контракты"); color: dialog.mutedColor; width: parent.width - 24; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                    }
                }
                FormCombo {
                    id: typeBox; Layout.fillWidth: true; visible: dialog.manualInstrument
                    textRole: "name"; valueRole: "value"
                    model: [ {name: qsTr("Акция"), value: 0}, {name: qsTr("Привилегированная акция"), value: 5},
                        {name: qsTr("Депозитарная расписка"), value: 6}, {name: qsTr("Биржевой фонд"), value: 1},
                        {name: qsTr("Пай ПИФ"), value: 3}, {name: qsTr("Облигация"), value: 2},
                        {name: qsTr("Драгоценный металл"), value: 7}, {name: qsTr("Валюта"), value: 8},
                        {name: qsTr("Фьючерс"), value: 9}, {name: qsTr("Опцион"), value: 10}, {name: qsTr("Другой инструмент"), value: 4} ]
                }
                FormField { id: nameField; Layout.fillWidth: true; visible: dialog.manualInstrument; placeholderText: qsTr("Название инструмента") }
                FormField { id: symbolField; Layout.fillWidth: true; visible: dialog.manualInstrument; placeholderText: qsTr("Обозначение инструмента") }
                FormCombo { id: optionSchemeBox; Layout.fillWidth: true; visible: dialog.manualInstrument && dialog.instrumentType === 10; model: [qsTr("Премиальный опцион"), qsTr("Маржируемый опцион")] }
                FormCombo { id: rightBox; Layout.fillWidth: true; visible: dialog.manualInstrument && dialog.instrumentType === 10; model: [qsTr("Право покупки"), qsTr("Право продажи")] }
                FormField { id: strikeField; Layout.fillWidth: true; visible: dialog.manualInstrument && dialog.instrumentType === 10; placeholderText: qsTr("Цена исполнения") }
                FormField { id: underlyingField; Layout.fillWidth: true; visible: dialog.manualInstrument && dialog.derivative; placeholderText: qsTr("Базовый актив") }
                FormField { id: maturityField; Layout.fillWidth: true; visible: dialog.manualInstrument && dialog.derivative; placeholderText: qsTr("Дата исполнения, дд.мм.гггг") }
                Label {
                    Layout.fillWidth: true; visible: !!dialog.selectedInstrument; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: !dialog.selectedInstrument ? "" : dialog.instrumentType === 2
                        ? qsTr("Цена: %1% · текущий номинал: %2 · НКД: %3 %4").arg(dialog.selectedInstrument.priceText || "—").arg(dialog.selectedInstrument.faceValueText || "—").arg(dialog.selectedInstrument.accruedInterestText || "—").arg(dialog.selectedInstrument.quoteCurrency || "")
                        : dialog.derivative
                          ? qsTr("Базовый актив: %1 · исполнение: %2 · шаг: %3 · стоимость шага: %4 %5")
                              .arg(dialog.selectedInstrument.underlying || "—").arg(dialog.selectedInstrument.maturity || "—")
                              .arg(dialog.selectedInstrument.priceStepText || "—").arg(dialog.selectedInstrument.stepValueText || "—").arg(dialog.selectedInstrument.quoteCurrency || "")
                          : qsTr("Количество указывается в единицах актива, а не в лотах. Лот: %1.").arg(dialog.selectedInstrument.lotSizeText || "1")
                }
                Label {
                    Layout.fillWidth: true; visible: dialog.instrumentType === 10 && !!dialog.selectedInstrument; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: !dialog.selectedInstrument ? "" : (dialog.selectedInstrument.optionRight === "C" ? qsTr("Право покупки") : dialog.selectedInstrument.optionRight === "P" ? qsTr("Право продажи") : qsTr("Право не определено"))
                        + qsTr(" · цена исполнения: %1 · %2").arg(dialog.selectedInstrument.strikeText || "—")
                          .arg(dialog.pricing === "premium_option" ? qsTr("Премиальный опцион") : dialog.pricing === "margined_option" ? qsTr("Маржируемый опцион") : qsTr("Расчёт по ручной оценке"))
                }
                Label { text: qsTr("Количество (%1)").arg(dialog.selectedInstrument ? dialog.selectedInstrument.quantityUnit : dialog.instrumentType === 7 ? qsTr("г") : dialog.derivative ? qsTr("контрактов") : qsTr("единиц")); color: dialog.mutedColor; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    FormField { id: quantityField; Layout.fillWidth: true; placeholderText: qsTr("Количество"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    FormField { id: averagePriceField; Layout.fillWidth: true; placeholderText: dialog.instrumentType === 2 ? qsTr("Средняя цена покупки, %") : qsTr("Средняя цена покупки"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                }
                FormCombo { id: directionBox; Layout.fillWidth: true; visible: dialog.derivative; model: [qsTr("Купленная позиция"), qsTr("Проданная позиция")] }
                StyledCheckBox {
                    id: manualCheck; Layout.fillWidth: true; Layout.minimumWidth: 0; visible: !dialog.manualInstrument; text: qsTr("Указать стоимость позиции вручную")
                    appTextColor: dialog.textColor; appSoftColor: dialog.softColor; appLineColor: dialog.lineColor; appAccentColor: dialog.accentColor
                }
                Label {
                    Layout.fillWidth: true; visible: !!dialog.selectedInstrument && dialog.derivative && dialog.pricing === "manual"
                    text: qsTr("Особые условия расчёта: используйте ручную оценку по данным брокера.")
                    wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                }
                FormField {
                    id: manualValueField; Layout.fillWidth: true; visible: dialog.manualInstrument || manualCheck.checked
                    placeholderText: qsTr("Стоимость всей позиции, %1").arg(dialog.accountCurrency); inputMethodHints: Qt.ImhFormattedNumbersOnly
                }
                Label {
                    Layout.fillWidth: true; visible: dialog.manualInstrument || manualCheck.checked; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: dialog.derivative ? qsTr("Укажите только ещё не включённый в деньги результат или обязательство со знаком. Для премиального опциона — его текущую стоимость со знаком.")
                        : qsTr("Укажите общую стоимость актива, для облигаций — с НКД. Деньги на счёте сюда не включаются.")
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: dialog.margined && !manualCheck.checked; text: qsTr("Последний проведённый расчёт или открытие позиции"); color: dialog.mutedColor; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true; visible: dialog.margined && !manualCheck.checked
                    FormField { id: referencePriceField; Layout.fillWidth: true; placeholderText: qsTr("Расчётная цена") }
                    FormField { id: referenceDateField; Layout.fillWidth: true; placeholderText: qsTr("дд.мм.гггг чч:мм:сс") }
                }
                Label {
                    Layout.fillWidth: true; visible: dialog.margined && !manualCheck.checked; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14
                    text: qsTr("Прибыль до указанного расчёта уже должна находиться в денежном остатке. В капитал добавится только изменение после него.")
                }
                RowLayout {
                    Layout.fillWidth: true; visible: dialog.derivative
                    FormField { id: marginField; Layout.fillWidth: true; placeholderText: qsTr("Заблокировано, %1").arg(dialog.accountCurrency) }
                    FormField { id: adjustmentField; Layout.fillWidth: true; placeholderText: qsTr("Ещё не проведённая поправка, %1").arg(dialog.accountCurrency) }
                }
                Label { visible: dialog.derivative; text: qsTr("Обеспечение входит в деньги и повторно к капиталу не прибавляется."); Layout.fillWidth: true; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14 }
                FormField {
                    id: fxField; Layout.fillWidth: true; visible: !dialog.manualInstrument && !manualCheck.checked && !!dialog.selectedInstrument && dialog.selectedInstrument.quoteCurrency !== dialog.accountCurrency
                    placeholderText: qsTr("Курс %1 → %2 (пусто — автоматически)").arg(dialog.selectedInstrument ? dialog.selectedInstrument.quoteCurrency : "").arg(dialog.accountCurrency)
                }
                FormCombo { id: entryBox; Layout.fillWidth: true; visible: !dialog.editingPositionId; model: [qsTr("Добавить уже имеющуюся позицию"), qsTr("Записать новую сделку и движение денег")] }
                FormField { id: cashField; Layout.fillWidth: true; visible: !dialog.editingPositionId && entryBox.currentIndex === 1; placeholderText: qsTr("Изменение денег со знаком, %1").arg(dialog.accountCurrency) }
                Label { Layout.fillWidth: true; visible: !dialog.editingPositionId; wrapMode: Text.WordWrap; color: dialog.mutedColor; font.pixelSize: 14; text: entryBox.currentIndex === 0 ? qsTr("Деньги не изменятся: используйте этот режим для переноса текущего портфеля.") : qsTr("Укажите фактическую сумму с комиссиями: расход со знаком минус, поступление со знаком плюс.") }
            }
        }
        Label {
            Layout.fillWidth: true; visible: text.length > 0; text: dialog.localError || (dialog.controller ? dialog.controller.investmentLastError : "")
            color: dialog.errorColor; wrapMode: Text.WordWrap; font.pixelSize: 14
        }
        RowLayout {
            Layout.fillWidth: true; Item { Layout.fillWidth: true }
            FormButton { id: cancelButton; text: qsTr("Отмена"); onClicked: dialog.close() }
            FormButton { id: saveButton; primary: true; text: dialog.editingPositionId ? qsTr("Сохранить") : qsTr("Добавить")
                enabled: dialog.controller && !dialog.controller.investmentQuoteBusy && accountBox.currentIndex >= 0
                    && (dialog.manualInstrument || dialog.selectedResult >= 0)
                onClicked: dialog.submit() }
        }
    }
}
