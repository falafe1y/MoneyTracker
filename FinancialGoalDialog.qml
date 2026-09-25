import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var controller
    required property var theme
    property string editingId: ""
    property string errorText: ""
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
        allSources.checked = row ? row.allSources : true;
        sources.clear();
        const selected = row ? row.sourceIds : [];
        const available = controller.goalSources;
        const seen = [];
        for (let i = 0; i < available.length; ++i) {
            const item = available[i];
            seen.push(item.id);
            sources.append({sourceId: item.id, label: item.name,
                selected: selected.indexOf(item.id) >= 0});
        }
        for (let j = 0; j < selected.length; ++j) {
            if (seen.indexOf(selected[j]) < 0)
                sources.append({sourceId: selected[j],
                    label: qsTr("Недоступный счёт или кошелёк"), selected: true});
        }
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
    component Field: TextField {
        id: field
        implicitHeight: 44
        color: theme.accent
        placeholderTextColor: theme.muted
        leftPadding: 12
        background: Rectangle {
            radius: 10; color: theme.soft
            border.color: field.activeFocus ? theme.accentSoft : theme.line
        }
    }
    component Action: Button {
        id: action
        property bool primary: false
        implicitHeight: 40
        leftPadding: 16; rightPadding: 16
        contentItem: Text { text: action.text; color: action.primary ? theme.white : theme.accent
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 9; color: action.primary ? theme.accent : theme.soft; border.color: theme.line }
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
                    ComboBox {
                        id: currencyBox; model: ["RUB", "USD", "EUR"]
                        implicitHeight: 44; Layout.preferredWidth: 110
                        contentItem: Text { text: currencyBox.displayText; color: theme.accent
                            verticalAlignment: Text.AlignVCenter; leftPadding: 12 }
                        background: Rectangle { radius: 10; color: theme.soft; border.color: theme.line }
                    }
                }
                CheckBox { id: limited; text: qsTr("Указать срок"); palette.windowText: theme.accent }
                Field { id: deadlineField; visible: limited.checked; Layout.fillWidth: true
                    placeholderText: qsTr("ДД.ММ.ГГГГ"); maximumLength: 10 }
                Text { visible: limited.checked; Layout.fillWidth: true
                    text: qsTr("Срок включительно. После него цель продолжит обновляться.")
                    color: theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                CheckBox { id: allSources; text: qsTr("Учитывать весь текущий капитал"); palette.windowText: theme.accent }
                Text { Layout.fillWidth: true; text: qsTr("Фиат, инвестиции и криптовалюта за вычетом задолженности. Деньги не резервируются: несколько целей могут учитывать одни средства.")
                    color: theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                Repeater {
                    model: sources
                    delegate: CheckBox {
                        required property int index
                        required property string label
                        required property bool selected
                        Layout.fillWidth: true
                        visible: !allSources.checked
                        checked: selected
                        text: label
                        palette.windowText: theme.accent
                        contentItem: Text { text: parent.text; color: theme.accent; elide: Text.ElideMiddle
                            leftPadding: parent.indicator.width + parent.spacing; verticalAlignment: Text.AlignVCenter }
                        onToggled: sources.setProperty(index, "selected", checked)
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
