import QtQuick
import QtQuick.Layouts

Flickable {
    id: more

    signal navigate(string key)

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: layout.implicitHeight + 16

    ColumnLayout {
        id: layout

        width: more.width
        spacing: 16

        Text {
            Layout.topMargin: 4
            text: qsTr("Планирование")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        MobileCard {
            Layout.fillWidth: true
            cardPadding: 0
            gap: 0

            MobileListRow {
                Layout.fillWidth: true
                first: true
                title: qsTr("Бюджеты")
                subtitle: qsTr("Лимиты по категориям и счетам")
                svg: "qrc:/icons/budgets.svg"
                chevron: true
                onClicked: more.navigate("budgets")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Цели")
                subtitle: qsTr("Накопления с датой и источниками")
                svg: "qrc:/icons/goals.svg"
                chevron: true
                onClicked: more.navigate("goals")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Траектория")
                subtitle: qsTr("Прогноз капитала и сценарии")
                svg: "qrc:/icons/trajectory.svg"
                chevron: true
                onClicked: more.navigate("trajectory")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Регулярные операции")
                subtitle: qsTr("Зарплата, аренда и подписки")
                icon: "calendar"
                chevron: true
                onClicked: more.navigate("scheduled")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Проекты")
                subtitle: qsTr("Доходы и расходы отдельных задач")
                svg: "qrc:/icons/projects.svg"
                chevron: true
                onClicked: more.navigate("projects")
            }
        }

        Text {
            text: qsTr("Данные")
            color: MobileTheme.muted
            font.pixelSize: MobileTheme.sizeLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
        }

        MobileCard {
            Layout.fillWidth: true
            cardPadding: 0
            gap: 0

            MobileListRow {
                Layout.fillWidth: true
                first: true
                title: qsTr("Категории")
                subtitle: qsTr("Доходы и расходы по группам")
                svg: "qrc:/icons/categories.svg"
                chevron: true
                onClicked: more.navigate("categories")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Заметки")
                subtitle: qsTr("Текст, который хранится вместе с данными")
                svg: "qrc:/icons/notes.svg"
                chevron: true
                onClicked: more.navigate("notes")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Импорт и экспорт")
                subtitle: qsTr("Операции в CSV и очистка данных")
                icon: "swap"
                chevron: true
                onClicked: more.navigate("data")
            }
            MobileListRow {
                Layout.fillWidth: true
                title: qsTr("Настройки")
                subtitle: qsTr("Валюта, курсы, язык и защита")
                svg: "qrc:/icons/settings.svg"
                chevron: true
                onClicked: more.navigate("settings")
            }
        }
    }
}
