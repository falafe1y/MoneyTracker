import QtQuick
import QtQuick.Layouts

Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            boundsBehavior: Flickable.StopAtBounds
            model: financeController.financialGoals
            footer: Item {
                width: list.width
                height: 72
            }

            delegate: MobileCard {
                id: goalCard

                required property var modelData
                readonly property var row: modelData

                width: list.width
                gap: 10

                TapHandler {
                    onTapped: goalSheet.openFor(goalCard.row)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: goalCard.row.name
                        elide: Text.ElideRight
                        color: MobileTheme.accent
                        font.pixelSize: MobileTheme.sizeHeading
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: goalCard.row.achieved || goalCard.row.overdue
                        text: goalCard.row.achieved ? qsTr("Достигнута") : qsTr("Срок прошёл")
                        color: goalCard.row.achieved ? MobileTheme.income : MobileTheme.red
                        font.pixelSize: MobileTheme.sizeCaption
                        font.weight: Font.DemiBold
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: MobileFormat.money(goalCard.row.currentMinor, goalCard.row.currency, false)
                        color: MobileTheme.accent
                        font.pixelSize: MobileTheme.sizeTitle
                        font.weight: Font.Bold
                    }
                    Text {
                        text: qsTr("из %1").arg(MobileFormat.money(goalCard.row.targetMinor, goalCard.row.currency, false))
                        color: MobileTheme.muted
                        font.pixelSize: MobileTheme.sizeBody
                    }
                }

                MobileProgress {
                    Layout.fillWidth: true
                    value: goalCard.row.ratio
                    fill: goalCard.row.achieved ? MobileTheme.income : MobileTheme.accent
                }

                Text {
                    Layout.fillWidth: true
                    text: goalCard.row.remainingMinor > 0
                        ? qsTr("Осталось %1").arg(MobileFormat.money(goalCard.row.remainingMinor, goalCard.row.currency, false))
                        : qsTr("Цель достигнута")
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }

                Text {
                    Layout.fillWidth: true
                    text: goalCard.row.deadline
                        ? qsTr("Срок: %1").arg(MobileFormat.isoDate(goalCard.row.deadline))
                        : qsTr("Без срока")
                    color: MobileTheme.muted
                    font.pixelSize: MobileTheme.sizeCaption
                }
            }
        }
    }

    MobileEmptyState {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: list.count === 0
        svg: "qrc:/icons/goals.svg"
        title: qsTr("Целей пока нет")
        text: qsTr("Укажите сумму и срок, а приложение покажет, сколько осталось накопить.")
        actionText: qsTr("Создать цель")
        onAction: goalSheet.openFor(null)
    }

    MobileFab {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        onClicked: goalSheet.openFor(null)
    }

    MobileGoalSheet {
        id: goalSheet
    }
}
