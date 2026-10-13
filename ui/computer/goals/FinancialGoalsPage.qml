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
    function openDeletion(row) {
        deletion.goalId = row.id;
        deletion.goalName = row.name;
        deletion.errorText = "";
        deletion.open();
    }
    function openGoalContextMenu(row, sourceItem, localX, localY) {
        const point = sourceItem.mapToItem(page, localX, localY);
        goalContextMenu.goalData = row;
        goalContextMenu.x = Math.max(
            8,
            Math.min(point.x, page.width - goalContextMenu.width - 8)
        );
        goalContextMenu.y = Math.max(
            8,
            Math.min(point.y, page.height - goalContextMenu.implicitHeight - 8)
        );
        goalContextMenu.open();
    }
    component Action: StyledButton {
        neo: true
        cornerRadius: 8
        font.weight: Font.DemiBold
        leftPadding: 14; rightPadding: 14
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
    component GoalMenuItem: MenuItem {
        id: menuItem
        property bool destructive: false
        hoverEnabled: true
        implicitHeight: 40
        leftPadding: 12
        rightPadding: 12
        contentItem: Text {
            text: menuItem.text
            color: !menuItem.enabled ? theme.muted
                 : menuItem.highlighted || menuItem.hovered ? theme.white
                 : menuItem.destructive ? theme.red : theme.accent
            font.pixelSize: 14
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 8
            color: menuItem.highlighted || menuItem.hovered
                 ? (menuItem.destructive ? theme.red : theme.accent)
                 : "transparent"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: qsTr("Текущий капитал по последним доступным курсам и котировкам")
            color: theme.muted; wrapMode: Text.WordWrap }
        Action { text: qsTr("+ Цель"); primary: true; onClicked: editor.openForm(null) }
    }
    ListView {
        MiddleScrollArea { parent: list; scroller: theme.scrollController; scrollTarget: list }
        id: list
        Layout.fillWidth: true; Layout.fillHeight: true
        clip: true; spacing: 14
        model: controller.financialGoals
        delegate: Rectangle {
            HardShadow { depth: 6; shadowColor: theme.accent }

            id: card
            required property var modelData
            width: ListView.view.width
            height: cardContent.implicitHeight + 36
            radius: 16; color: theme.panel; border.color: theme.accent
            border.width: 1
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: modelData.name
            ToolTip.delay: 650
            ToolTip.visible: goalMenuArea.containsMouse || activeFocus
            ToolTip.text: qsTr("Правый клик или Enter: изменить или удалить")
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                    || event.key === Qt.Key_Menu
                    || (event.key === Qt.Key_F10
                        && (event.modifiers & Qt.ShiftModifier))) {
                    page.openGoalContextMenu(modelData, card, card.width - 24, 24);
                    event.accepted = true;
                }
            }
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
                    Layout.fillWidth: true; height: 9; radius: 5; color: theme.soft
                    border.width: 1; border.color: theme.accent
                    Rectangle { height: parent.height; radius: parent.radius
                        width: parent.width * Math.min(1, Math.max(0, card.modelData.ratio))
                        color: !card.modelData.complete ? theme.muted : card.modelData.achieved ? theme.income : theme.accent }
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
                }
            }
            MouseArea {
                id: goalMenuArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                onClicked: function(mouse) {
                    card.forceActiveFocus();
                    page.openGoalContextMenu(
                        card.modelData,
                        goalMenuArea,
                        mouse.x,
                        mouse.y
                    );
                }
            }
        }
        Text { anchors.centerIn: parent; visible: list.count === 0
            width: Math.min(430, parent.width - 40); horizontalAlignment: Text.AlignHCenter
            text: qsTr("Создайте цель: укажите нужную сумму, валюту и при желании срок.")
            color: theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap }
        ScrollBar.vertical: StyledScrollBar {
            appAccentColor: theme.accentSoft
            appTrackColor: theme.line
        }
    }
    Menu {
        id: goalContextMenu
        width: 224
        padding: 6
        property var goalData: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        GoalMenuItem {
            width: goalContextMenu.availableWidth
            text: qsTr("Редактировать")
            enabled: goalContextMenu.goalData !== null
            onTriggered: {
                if (goalContextMenu.goalData)
                    editor.openForm(goalContextMenu.goalData);
            }
        }

        MenuSeparator {
            width: goalContextMenu.availableWidth
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: theme.line
            }
        }

        GoalMenuItem {
            width: goalContextMenu.availableWidth
            text: qsTr("Удалить")
            destructive: true
            enabled: goalContextMenu.goalData !== null
            onTriggered: {
                if (goalContextMenu.goalData)
                    page.openDeletion(goalContextMenu.goalData);
            }
        }

        background: Rectangle {
            HardShadow { depth: 4; shadowColor: theme.accent }

            color: theme.panel
            radius: 12
            border.width: 1
            border.color: theme.accent
        }
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
        background: Rectangle {
            HardShadow { depth: 6; shadowColor: theme.accent }
            radius: 16
            color: theme.panel
            border.width: 1
            border.color: theme.accent
        }
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
