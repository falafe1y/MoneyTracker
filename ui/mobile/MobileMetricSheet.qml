import QtQuick
import QtQuick.Layouts

MobileSheet {
    id: sheet

    property string metric: ""
    property string displayValue: ""
    readonly property var entry: MobileMetricDefinitions.entries[metric]
        || [qsTr("Показатель"), qsTr("Подробности приведены в пояснениях к расчёту."), "", ""]

    title: entry[0]

    function openFor(key, value) {
        metric = key;
        displayValue = value;
        open();
    }

    Text {
        Layout.fillWidth: true
        visible: sheet.displayValue.length > 0
        text: qsTr("Сейчас: %1").arg(sheet.displayValue)
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeBody
        font.weight: Font.DemiBold
    }
    Text {
        Layout.fillWidth: true
        text: sheet.entry[1]
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeBody
    }
    Text {
        Layout.fillWidth: true
        visible: sheet.entry[2].length > 0
        text: qsTr("Расчёт: %1").arg(sheet.entry[2])
        wrapMode: Text.Wrap
        color: MobileTheme.accent
        font.pixelSize: MobileTheme.sizeBody
    }
    Text {
        Layout.fillWidth: true
        visible: sheet.entry[3].length > 0
        text: sheet.entry[3]
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }
    Text {
        Layout.fillWidth: true
        text: qsTr("Если показано «—», проверьте блок «Качество данных» и пояснения к расчёту.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }
}
