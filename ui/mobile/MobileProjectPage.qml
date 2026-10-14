import QtQuick
import QtQuick.Layouts

Item {
    id: page

    signal done()

    property bool confirmingDelete: false

    function findProject() {
        const rows = financeController.projects;
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].id === financeController.selectedProjectId)
                return rows[i];
        return null;
    }

    readonly property var project: findProject()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        MobileCard {
            Layout.fillWidth: true
            hard: true
            gap: 8

            Text {
                text: qsTr("Результат проекта")
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }
            Text {
                text: MobileFormat.money(page.project ? page.project.resultMinor : 0, financeController.appCurrency, true)
                color: page.project && page.project.resultMinor < 0 ? MobileTheme.red : MobileTheme.accent
                font.pixelSize: MobileTheme.sizeDisplay
                font.weight: Font.Bold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: qsTr("Доходы")
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeCaption
                    }
                    Text {
                        text: MobileFormat.money(page.project ? page.project.incomeMinor : 0, financeController.appCurrency, false)
                        color: MobileTheme.income
                        font.pixelSize: MobileTheme.sizeBody
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: qsTr("Расходы")
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeCaption
                    }
                    Text {
                        text: MobileFormat.money(page.project ? page.project.expenseMinor : 0, financeController.appCurrency, false)
                        color: MobileTheme.red
                        font.pixelSize: MobileTheme.sizeBody
                        font.weight: Font.DemiBold
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MobileButton {
                    Layout.fillWidth: true
                    text: qsTr("Переименовать")
                    onClicked: renameSheet.openFor(page.project)
                }
                MobileButton {
                    Layout.fillWidth: true
                    variant: "danger"
                    text: page.confirmingDelete ? qsTr("Точно удалить?") : qsTr("Удалить")
                    onClicked: {
                        if (!page.confirmingDelete) {
                            page.confirmingDelete = true;
                            return;
                        }
                        if (page.project && financeController.deleteProject(page.project.id))
                            page.done();
                    }
                }
            }
        }

        Text {
            text: qsTr("Операции")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        ListView {
            id: transactions

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: financeController.projectTransactions
            footer: Item {
                width: transactions.width
                height: 72
            }

            delegate: MobileListRow {
                required property var modelData
                required property int index

                width: transactions.width
                first: index === 0
                title: modelData.categoryName || modelData.description || qsTr("Без категории")
                subtitle: Qt.formatDateTime(new Date(modelData.date), "dd.MM.yyyy")
                    + (modelData.description && modelData.categoryName ? " · " + modelData.description : "")
                amount: MobileFormat.transactionText(modelData)
                amountColor: modelData.type === "income" ? MobileTheme.income
                           : modelData.type === "transfer" ? MobileTheme.muted : MobileTheme.red
            }
        }
    }

    MobileEmptyState {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: transactions.count === 0
        svg: "qrc:/icons/projects.svg"
        title: qsTr("Операций пока нет")
        text: qsTr("Добавьте доход или расход, и они попадут в сводку проекта.")
        actionText: qsTr("Добавить операцию")
        onAction: operationSheet.openNew()
    }

    MobileFab {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: operationSheet.openNew()
    }

    MobileSheet {
        id: renameSheet

        property string errorText: ""

        title: qsTr("Название проекта")

        function openFor(row) {
            if (!row)
                return;
            renameField.text = row.name;
            errorText = "";
            open();
        }

        MobileField {
            id: renameField
            Layout.fillWidth: true
            label: qsTr("Название")
        }

        Text {
            Layout.fillWidth: true
            visible: renameSheet.errorText.length > 0
            text: renameSheet.errorText
            wrapMode: Text.Wrap
            color: MobileTheme.red
            font.pixelSize: MobileTheme.sizeCaption
        }

        MobileButton {
            Layout.fillWidth: true
            variant: "primary"
            text: qsTr("Сохранить")
            onClicked: {
                if (page.project && financeController.renameProject(page.project.id, renameField.text))
                    renameSheet.close();
                else
                    renameSheet.errorText = qsTr("Укажите уникальное название проекта");
            }
        }
    }

    MobileSheet {
        id: operationSheet

        property string operationKind: "expense"
        property string categoryId: ""
        property string accountId: ""
        property string errorText: ""

        title: qsTr("Операция проекта")

        function categoriesFor(kind) {
            return financeController.categories.filter(function(item) { return item.type === kind; });
        }

        function setKind(kind) {
            if (operationKind === kind)
                return;
            operationKind = kind;
            categoryId = "";
        }

        function openNew() {
            operationKind = "expense";
            categoryId = "";
            accountId = "";
            errorText = "";
            amountField.text = "";
            descriptionField.text = "";
            dateField.text = Qt.formatDate(new Date(), "dd.MM.yyyy");
            open();
        }

        function save() {
            const minor = MobileFormat.minorFromText(amountField.text);
            const date = MobileFormat.dateFromInput(dateField.text);
            if (minor <= 0) {
                errorText = qsTr("Введите сумму больше нуля");
                return;
            }
            if (!date) {
                errorText = qsTr("Укажите дату в формате дд.мм.гггг");
                return;
            }
            if (categoryId.length === 0 || accountId.length === 0) {
                errorText = qsTr("Выберите категорию и счёт");
                return;
            }
            if (page.project && financeController.addProjectTransaction(
                    page.project.id, minor, descriptionField.text, categoryId, accountId, operationKind, date))
                close();
            else
                errorText = qsTr("Не удалось сохранить операцию");
        }

        MobileSegmented {
            Layout.fillWidth: true
            options: [
                { label: qsTr("Расход"), value: "expense" },
                { label: qsTr("Доход"), value: "income" }
            ]
            value: operationSheet.operationKind
            onActivated: function(chosen) { operationSheet.setKind(chosen); }
        }

        MobileField {
            id: amountField
            Layout.fillWidth: true
            label: qsTr("Сумма")
            placeholder: "0"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }

        MobileField {
            id: descriptionField
            Layout.fillWidth: true
            label: qsTr("Описание")
            placeholder: qsTr("Необязательно")
        }

        Text {
            text: qsTr("Категория")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
            font.weight: Font.Medium
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: operationSheet.categoriesFor(operationSheet.operationKind)

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.label
                    selected: operationSheet.categoryId === modelData.value
                    onClicked: operationSheet.categoryId = modelData.value
                }
            }
        }

        Text {
            text: qsTr("Счёт")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeCaption
            font.weight: Font.Medium
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: financeController.allAccounts

                delegate: MobileChip {
                    required property var modelData
                    text: modelData.displayName || modelData.name
                    selected: operationSheet.accountId === modelData.id
                    onClicked: operationSheet.accountId = modelData.id
                }
            }
        }

        MobileField {
            id: dateField
            Layout.fillWidth: true
            label: qsTr("Дата")
            placeholder: qsTr("дд.мм.гггг")
        }

        Text {
            Layout.fillWidth: true
            visible: operationSheet.errorText.length > 0
            text: operationSheet.errorText
            wrapMode: Text.Wrap
            color: MobileTheme.red
            font.pixelSize: MobileTheme.sizeCaption
        }

        MobileButton {
            Layout.fillWidth: true
            variant: "primary"
            text: qsTr("Сохранить")
            onClicked: operationSheet.save()
        }
    }
}
