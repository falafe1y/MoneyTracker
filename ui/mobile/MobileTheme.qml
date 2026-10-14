pragma Singleton
import QtQuick

QtObject {
    readonly property color canvas: "#F5F3E4"
    readonly property color panel: "#FFFFF8"
    readonly property color soft: "#FFFFF0"
    readonly property color line: "#D8D7C7"
    readonly property color accent: "#031528"
    readonly property color accentText: "#FFFFF0"
    readonly property color muted: "#687483"
    readonly property color accentSoft: "#536A98"
    readonly property color pale: "#E4E8F1"
    readonly property color navSelected: "#294477"
    readonly property color red: "#B94F48"
    readonly property color income: "#3F735F"
    readonly property color incomePanel: "#E8F0E9"
    readonly property color expensePanel: "#F5E6E2"
    readonly property color tableHeader: "#F1F0DF"
    readonly property color hover: "#F3F1E3"
    readonly property var chartColors: ["#072a50", "#0e54a0", "#3a8eee", "#31c7ed", "#70d6f0"]

    readonly property int margin: 16
    readonly property int touchHeight: 48
    readonly property int radiusControl: 9
    readonly property int radiusCard: 14
    readonly property int radiusSheet: 20
    readonly property int shadowDepth: 4
    readonly property int navInset: 56

    readonly property int sizeDisplay: 28
    readonly property int sizeTitle: 20
    readonly property int sizeHeading: 16
    readonly property int sizeBody: 14
    readonly property int sizeCaption: 12
    readonly property int sizeLabel: 11
}
