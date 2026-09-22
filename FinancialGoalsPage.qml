import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var controller
    required property var theme
    spacing: 14
    Component.onCompleted: controller.refreshInvestmentQuotes()
    function money(amount, currency) { return theme.money(amount, currency, false); }
    function status(row) {
        if (!row.complete) return qsTr("Неполный расчёт");
        if (row.achieved) return qsTr("Цель достигнута");
        return row.overdue ? qsTr("Срок истёк") : qsTr("В процессе");
    }
    component Action: Button {
        id: action
        property bool primary: false
        property bool destructive: false
        implicitHeight: 40
        leftPadding: 14; rightPadding: 14
        contentItem: Text { text: action.text; color: action.primary || action.destructive ? theme.white : theme.accent
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 9; color: action.destructive ? theme.red : action.primary ? theme.accent : theme.soft
            border.color: theme.line }
    }
    RowLayout {
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: qsTr("Текущий капитал по последним доступным курсам и котировкам")
            color: theme.muted; wrapMode: Text.WordWrap }
        Action { text: qsTr("+ Цель"); primary: true; onClicked: editor.openForm(null) }
    }
    ListView {
        id: list
        Layout.fillWidth: true; Layout.fillHeight: true
        clip: true; spacing: 14
        model: controller.financialGoals
        delegate: Rectangle {
            id: card
            required property var modelData
            width: ListView.view.width
            height: cardContent.implicitHeight + 36
            radius: 16; color: theme.panel; border.color: theme.line
            ColumnLayout {
                id: cardContent
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 18
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: card.modelData.name; color: theme.accent
                        font.pixelSize: 20; font.bold: true; elide: Text.ElideRight }
                    Text { text: page.status(card.modelData); color: card.modelData.achieved ? theme.income
                            : !card.modelData.complete || card.modelData.overdue ? theme.red : theme.muted }
                }
                Text { Layout.fillWidth: true; color: theme.accent; font.pixelSize: 23; font.bold: true
                    wrapMode: Text.Wrap
                    text: (card.modelData.complete ? "" : qsTr("Известно: "))
                        + page.money(card.modelData.currentMinor, card.modelData.currency)
                        + " / " + page.money(card.modelData.targetMinor, card.modelData.currency) }
                Rectangle {
                    Layout.fillWidth: true; height: 9; radius: 5; color: theme.line
                    Rectangle { height: parent.height; radius: parent.radius
                        width: parent.width * Math.min(1, Math.max(0, card.modelData.ratio))
                        color: !card.modelData.complete ? theme.muted : card.modelData.achieved ? theme.income : theme.accentSoft }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; color: theme.muted; wrapMode: Text.WordWrap
                        text: card.modelData.complete
                            ? qsTr("%1% · Осталось: %2").arg(Math.max(0, card.modelData.ratio * 100).toFixed(1))
                                .arg(page.money(card.modelData.remainingMinor, card.modelData.currency))
                            : qsTr("Проверьте доступность счетов, балансов и котировок") }
                    Text { color: theme.muted; text: card.modelData.deadline.length > 0
                        ? qsTr("До %1").arg(card.modelData.deadline.split("-").reverse().join(".")) : qsTr("Без срока") }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; color: theme.muted
                        text: card.modelData.allSources ? qsTr("Весь капитал")
                            : qsTr("Выбрано источников: %1").arg(card.modelData.sourceIds.length) }
                    Action { text: qsTr("Изменить"); onClicked: editor.openForm(card.modelData) }
                    Action { text: qsTr("Удалить"); destructive: true
                        onClicked: { deletion.goalId = card.modelData.id; deletion.goalName = card.modelData.name;
                            deletion.errorText = ""; deletion.open(); } }
                }
            }
        }
        Text { anchors.centerIn: parent; visible: list.count === 0
            width: Math.min(430, parent.width - 40); horizontalAlignment: Text.AlignHCenter
            text: qsTr("Создайте цель: укажите нужную сумму, валюту и при желании срок.")
            color: theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
        ScrollBar.vertical: ScrollBar {}
    }
    FinancialGoalDialog { id: editor; controller: page.controller; theme: page.theme; parent: Overlay.overlay }
    Dialog {
        id: deletion
        parent: Overlay.overlay
        property string goalId: ""
        property string goalName: ""
        property string errorText: ""
        width: Math.min(470, parent ? parent.width - 40 : 470)
        anchors.centerIn: parent; modal: true; padding: 24
        background: Rectangle { radius: 18; color: theme.panel; border.color: theme.line }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: qsTr("Удалить цель?"); color: theme.accent; font.pixelSize: 21; font.bold: true }
            Text { Layout.fillWidth: true; text: deletion.goalName; color: theme.accent; elide: Text.ElideRight }
            Text { Layout.fillWidth: true; text: qsTr("Счета, деньги и операции останутся без изменений.")
                color: theme.muted; wrapMode: Text.WordWrap }
            Text { visible: deletion.errorText.length > 0; text: deletion.errorText; color: theme.red }
            RowLayout {
                Item { Layout.fillWidth: true }
                Action { text: qsTr("Отмена"); onClicked: deletion.close() }
                Action { text: qsTr("Удалить"); destructive: true; onClicked: {
                    if (controller.deleteFinancialGoal(deletion.goalId)) deletion.close();
                    else deletion.errorText = qsTr("Не удалось удалить цель");
                } }
            }
        }
    }
}
