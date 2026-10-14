import QtQuick
import QtQuick.Layouts

MobileSheet {
    id: sheet

    property string editingId: ""
    property string currencyCode: "RUB"
    property bool hasDeadline: false
    property bool allSourcesChecked: true
    property string errorText: ""
    property bool confirmingDelete: false

    title: editingId.length > 0 ? qsTr("Цель") : qsTr("Новая цель")

    ListModel {
        id: sourceModel
    }

    function openFor(row) {
        const sourceIds = row ? row.sourceIds : [];
        editingId = row ? row.id : "";
        currencyCode = row ? row.currency : "RUB";
        nameField.text = row ? row.name : "";
        targetField.text = row ? MobileFormat.amountText(row.targetMinor) : "";
        hasDeadline = row ? !!row.deadline : false;
        deadlineField.text = row && row.deadline ? MobileFormat.isoDate(row.deadline) : "";
        allSourcesChecked = row ? row.allSources : true;
        errorText = "";
        confirmingDelete = false;

        sourceModel.clear();
        const sources = financeController.goalSources;
        for (let i = 0; i < sources.length; ++i) {
            const source = sources[i];
            sourceModel.append({
                sourceId: source.id,
                title: source.name,
                selected: sourceIds.indexOf(source.id) >= 0
            });
        }
        open();
    }

    function save() {
        const sourceIds = [];
        for (let i = 0; i < sourceModel.count; ++i) {
            const source = sourceModel.get(i);
            if (source.selected)
                sourceIds.push(source.sourceId);
        }
        const result = financeController.saveFinancialGoal({
            id: editingId,
            name: nameField.text,
            amountText: targetField.text,
            currency: currencyCode,
            deadline: hasDeadline ? deadlineField.text : "",
            allSources: allSourcesChecked,
            sourceIds: sourceIds
        });
        if (result.ok)
            sheet.close();
        else
            errorText = result.error || qsTr("Не удалось сохранить цель");
    }

    MobileField {
        id: nameField
        Layout.fillWidth: true
        label: qsTr("Название")
        placeholder: qsTr("Например, отпуск")
    }

    MobileSegmented {
        Layout.fillWidth: true
        options: [
            { label: "RUB", value: "RUB" },
            { label: "USD", value: "USD" },
            { label: "EUR", value: "EUR" },
            { label: "CNY", value: "CNY" }
        ]
        value: sheet.currencyCode
        onActivated: function(chosen) { sheet.currencyCode = chosen; }
    }

    MobileField {
        id: targetField
        Layout.fillWidth: true
        label: qsTr("Сумма цели")
        placeholder: "0"
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    MobileCheckRow {
        Layout.fillWidth: true
        label: qsTr("Указать срок")
        checked: sheet.hasDeadline
        onToggled: function(nowChecked) { sheet.hasDeadline = nowChecked; }
    }

    MobileField {
        id: deadlineField
        visible: sheet.hasDeadline
        Layout.fillWidth: true
        label: qsTr("Срок")
        placeholder: qsTr("дд.мм.гггг")
    }

    MobileCheckRow {
        Layout.fillWidth: true
        label: qsTr("Все счета и кошельки")
        checked: sheet.allSourcesChecked
        onToggled: function(nowChecked) { sheet.allSourcesChecked = nowChecked; }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: !sheet.allSourcesChecked
        spacing: 0

        Repeater {
            model: sourceModel

            delegate: MobileCheckRow {
                required property int index
                required property string title
                required property bool selected

                Layout.fillWidth: true
                label: title
                checked: selected
                onToggled: function(nowChecked) { sourceModel.setProperty(index, "selected", nowChecked); }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: sheet.errorText.length > 0
        text: sheet.errorText
        wrapMode: Text.Wrap
        color: MobileTheme.red
        font.pixelSize: MobileTheme.sizeCaption
    }

    MobileButton {
        Layout.fillWidth: true
        variant: "primary"
        text: qsTr("Сохранить")
        onClicked: sheet.save()
    }

    MobileButton {
        Layout.fillWidth: true
        visible: sheet.editingId.length > 0
        variant: "danger"
        text: sheet.confirmingDelete ? qsTr("Нажмите ещё раз, чтобы удалить") : qsTr("Удалить цель")
        onClicked: {
            if (!sheet.confirmingDelete) {
                sheet.confirmingDelete = true;
                return;
            }
            if (financeController.deleteFinancialGoal(sheet.editingId))
                sheet.close();
            else
                sheet.errorText = qsTr("Не удалось удалить цель");
        }
    }
}
