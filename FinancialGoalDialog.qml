import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var controller
    required property var theme
    property string editingId: ""
    property string errorText: ""
    property bool syncingSourceSelection: false
    width: Math.min(620, parent ? parent.width - 40 : 620)
    height: Math.min(720, parent ? parent.height - 40 : 720)
    anchors.centerIn: parent
    modal: true
    padding: 22
    closePolicy: Popup.CloseOnEscape
    ListModel { id: sources }

    function russianDateFromIso(value) {
        const parts = String(value || "").split("-");
        return parts.length === 3
             ? parts[2] + "." + parts[1] + "." + parts[0]
             : "";
    }

    function setAllSourcesSelected(selected) {
        syncingSourceSelection = true;
        for (let i = 0; i < sources.count; ++i)
            sources.setProperty(i, "selected", selected);
        syncingSourceSelection = false;
    }

    function syncAllSourcesCheckBox() {
        let everySourceSelected = sources.count > 0;
        for (let i = 0; i < sources.count && everySourceSelected; ++i)
            everySourceSelected = sources.get(i).selected;
        syncingSourceSelection = true;
        allSources.checked = everySourceSelected;
        syncingSourceSelection = false;
    }

    function openForm(row) {
        editingId = row ? row.id : "";
        errorText = "";
        nameField.text = row ? row.name : "";
        amountField.text = row ? (Number(row.targetMinor) / 100).toFixed(2) : "";
        currencyBox.currentIndex = Math.max(0, currencyBox.model.indexOf(
            row ? row.currency : controller.appCurrency));
        limited.checked = row ? row.deadline.length > 0 : false;
        deadlineField.text = row && row.deadline.length > 0
                           ? russianDateFromIso(row.deadline)
                           : Qt.formatDate(new Date(), "dd.MM.yyyy");
        const useAllSources = row ? row.allSources : true;
        sources.clear();
        const selected = row ? row.sourceIds : [];
        const available = controller.goalSources;
        const seen = [];
        for (let i = 0; i < available.length; ++i) {
            const item = available[i];
            seen.push(item.id);
            sources.append({sourceId: item.id, label: item.name,
                selected: useAllSources || selected.indexOf(item.id) >= 0});
        }
        for (let j = 0; j < selected.length; ++j) {
            if (seen.indexOf(selected[j]) < 0)
                sources.append({sourceId: selected[j],
                    label: qsTr("Недоступный счёт или кошелёк"), selected: true});
        }
        syncingSourceSelection = true;
        allSources.checked = useAllSources;
        syncingSourceSelection = false;
        open();
        Qt.callLater(function() { nameField.forceActiveFocus(); });
    }
    function submit() {
        const ids = [];
        for (let i = 0; i < sources.count; ++i)
            if (sources.get(i).selected) ids.push(sources.get(i).sourceId);
        const result = controller.saveFinancialGoal({id: editingId,
            name: nameField.text, amountText: amountField.text,
            currency: currencyBox.currentText,
            deadline: limited.checked ? deadlineField.text : "",
            allSources: allSources.checked, sourceIds: ids});
        if (result.ok) close();
        else errorText = result.error;
    }
    background: Rectangle { radius: 18; color: theme.panel; border.color: theme.line }
    component Field: StyledTextField {
        appTextColor: theme.accent
        appMutedColor: theme.muted
        appPanelColor: theme.panel
        appSoftColor: theme.soft
        appLineColor: theme.line
        appAccentColor: theme.accentSoft
        appOnAccentColor: theme.white
    }
    component Choice: StyledComboBox {
        appTextColor: theme.accent
        appMutedColor: theme.muted
        appPanelColor: theme.panel
        appSoftColor: theme.soft
        appLineColor: theme.line
        appAccentColor: theme.accentSoft
        appHoverColor: theme.controlHovered
    }
    component FormCheckBox: StyledCheckBox {
        appTextColor: theme.accent
        appMutedColor: theme.muted
        appSoftColor: theme.soft
        appLineColor: theme.line
        appAccentColor: theme.accent
        appHoverColor: theme.controlHovered
        appOnAccentColor: theme.white
    }
    component Action: StyledButton {
        appTextColor: theme.accent
        appMutedColor: theme.muted
        appPanelColor: theme.panel
        appSoftColor: theme.soft
        appLineColor: theme.line
        appAccentColor: theme.accent
        appHoverColor: theme.controlHovered
        appOnAccentColor: theme.white
        appErrorColor: theme.red
    }
    contentItem: ColumnLayout {
        spacing: 14
        Text { text: dialog.editingId ? qsTr("Редактирование цели") : qsTr("Новая финансовая цель")
            color: theme.accent; font.pixelSize: 21; font.bold: true }
        ScrollView {
            id: scroll
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12
                Text { text: qsTr("Название"); color: theme.muted }
                Field { id: nameField; Layout.fillWidth: true; maximumLength: 80
                    placeholderText: qsTr("Например, финансовая независимость") }
                Text { text: qsTr("Сумма и валюта цели"); color: theme.muted }
                RowLayout {
                    Layout.fillWidth: true
                    Field { id: amountField; Layout.fillWidth: true; maximumLength: 15
                        placeholderText: "200000,00"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    Choice {
                        id: currencyBox; model: ["RUB", "USD", "EUR"]
                        implicitHeight: 44; Layout.preferredWidth: 110
                    }
                }
                FormCheckBox { id: limited; text: qsTr("Указать срок") }
                Field { id: deadlineField; visible: limited.checked; Layout.fillWidth: true
                    placeholderText: qsTr("ДД.ММ.ГГГГ"); maximumLength: 10 }
                Text { visible: limited.checked; Layout.fillWidth: true
                    text: qsTr("Срок включительно. После него цель продолжит обновляться.")
                    color: theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                FormCheckBox {
                    id: allSources
                    text: qsTr("Учитывать весь текущий капитал")
                    palette.windowText: theme.accent
                    onToggled: {
                        if (!dialog.syncingSourceSelection)
                            dialog.setAllSourcesSelected(checked);
                    }
                }
                Text { Layout.fillWidth: true; text: qsTr("Фиат, инвестиции и криптовалюта за вычетом задолженности. Деньги не резервируются: несколько целей могут учитывать одни средства.")
                    color: theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                Repeater {
                    model: sources
                    delegate: FormCheckBox {
                        required property int index
                        required property string label
                        required property bool selected
                        Layout.fillWidth: true
                        checked: selected
                        text: label
                        onToggled: {
                            if (dialog.syncingSourceSelection)
                                return;
                            sources.setProperty(index, "selected", checked);
                            dialog.syncAllSourcesCheckBox();
                        }
                    }
                }
            }
        }
        Text { visible: dialog.errorText.length > 0; text: dialog.errorText
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: theme.red }
        RowLayout {
            Item { Layout.fillWidth: true }
            Action { text: qsTr("Отмена"); onClicked: dialog.close() }
            Action { text: qsTr("Сохранить"); primary: true; onClicked: dialog.submit() }
        }
    }
}
