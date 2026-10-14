import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

Flickable {
    id: page

    property string statusText: ""
    property bool statusOk: true
    property bool confirmingClear: false

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: layout.implicitHeight + 16

    FileDialog {
        id: exportDialog

        title: qsTr("Экспорт операций в CSV")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("CSV-файлы (*.csv)")]
        defaultSuffix: "csv"
        onAccepted: {
            const result = financeController.exportTransactionsCsv(selectedFile);
            page.statusOk = result.ok;
            page.statusText = result.ok
                ? qsTr("Экспортировано операций: %1").arg(result.count)
                : qsTr("Не удалось экспортировать: %1").arg(result.error);
        }
    }

    FileDialog {
        id: importDialog

        title: qsTr("Импорт операций из CSV")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Все файлы (*.*)")]
        onAccepted: {
            const result = financeController.importTransactionsCsv(selectedFile);
            page.statusOk = result.ok;
            page.statusText = result.ok
                ? qsTr("Импортировано: %1, пропущено дубликатов: %2").arg(result.imported).arg(result.skipped)
                : qsTr("Не удалось импортировать: %1").arg(result.error);
        }
    }

    ColumnLayout {
        id: layout

        width: page.width
        spacing: 16

        Text {
            text: qsTr("Операции в CSV")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        MobileCard {
            Layout.fillWidth: true
            gap: 12

            Text {
                Layout.fillWidth: true
                text: qsTr("Экспорт сохраняет все операции в файл. Импорт добавляет операции из файла и пропускает уже существующие.")
                wrapMode: Text.Wrap
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }

            MobileButton {
                Layout.fillWidth: true
                text: qsTr("Экспортировать в CSV")
                onClicked: exportDialog.open()
            }
            MobileButton {
                Layout.fillWidth: true
                text: qsTr("Импортировать из CSV")
                onClicked: importDialog.open()
            }

            Text {
                Layout.fillWidth: true
                visible: page.statusText.length > 0
                text: page.statusText
                wrapMode: Text.Wrap
                color: page.statusOk ? MobileTheme.income : MobileTheme.red
                font.pixelSize: MobileTheme.sizeCaption
            }
        }

        Text {
            text: qsTr("Опасная зона")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        MobileCard {
            Layout.fillWidth: true
            gap: 12

            Text {
                Layout.fillWidth: true
                text: qsTr("Удаляет все счета, операции, бюджеты, цели и заметки. Восстановить данные после этого нельзя.")
                wrapMode: Text.Wrap
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }

            MobileButton {
                Layout.fillWidth: true
                variant: "danger"
                text: page.confirmingClear ? qsTr("Нажмите ещё раз, чтобы удалить всё") : qsTr("Очистить все данные")
                onClicked: {
                    if (!page.confirmingClear) {
                        page.confirmingClear = true;
                        return;
                    }
                    page.confirmingClear = false;
                    const result = financeController.clearAllData();
                    page.statusOk = result.ok;
                    page.statusText = result.ok
                        ? qsTr("Все данные удалены")
                        : qsTr("Не удалось очистить данные: %1").arg(result.error);
                }
            }
        }
    }
}
