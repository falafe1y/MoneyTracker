import QtQuick
import QtQuick.Controls
import QtTest
import "../.."

Item {
    id: scene
    width: 800; height: 600
    InvestmentMetricHelp { id: help; metric: "gain"; displayValue: "1 500 RUB" }
    InvestmentDataQualityPanel {
        id: quality
        y: 64; width: scene.width
        detailsOpen: true
        quality: ({checkedRows: 6, errors: 1, warnings: 1, notes: 1, items: [
            {code:"stale_valuation", severity:"error", label:"Устарела цена", account:"Брокер", asset:"ABC", impact:"Результат недоступен", action:"Обновите текущую оценку", dateFrom:"2025-01-01", dateTo:"2025-01-03", dates:["2025-01-01","2025-01-03"], count:2},
            {code:"unknown_fee", severity:"warning", label:"Не указана комиссия", account:"Другой счёт", asset:"BOND", impact:"Расходы неизвестны", action:"Проверьте операцию", count:1},
            {code:"short_history", severity:"info", label:"Короткая история", impact:"Нужны две оценки", action:"Накопите историю", count:1}
        ]})
    }
    TestCase {
        name: "AnalyticsExplanationsAndQuality"
        when: windowShown
        function cleanup() {
            var popup = findChild(help, "metricHelpDialog")
            popup.close()
            findChild(quality,"qualitySearch").text = ""
            findChild(quality,"qualitySeverity").currentIndex = 0
        }
        function test_qualityFiltersBySeverityAndAccount() {
            compare(quality.filtered.length, 3)
            findChild(quality,"qualitySeverity").currentIndex = 1
            compare(quality.filtered.length, 1)
            compare(quality.filtered[0].code, "stale_valuation")
            findChild(quality,"qualitySeverity").currentIndex = 0
            findChild(quality,"qualitySearch").text = "другой"
            compare(quality.filtered.length, 1)
            compare(quality.filtered[0].asset, "BOND")
            findChild(quality,"qualitySearch").text = "2025-01-03"
            compare(quality.filtered.length, 1)
            findChild(quality,"qualitySearch").text = "отсутствует"
            compare(quality.filtered.length, 0)
        }
        function test_everyMetricHasAnExplanation() {
            var names = Object.keys(help.definitions)
            verify(names.length >= 28)
            for (var i = 0; i < names.length; ++i) {
                help.metric = names[i]
                verify(help.entry[0].length > 0)
                verify(help.entry[1].length > 0)
                verify(help.entry[2].length > 0)
                verify(help.entry[3].length > 0)
            }
        }
        function test_longExplanationOpensAndFitsWindow() {
            help.metric = "structure"
            mouseClick(help)
            var popup = findChild(help, "metricHelpDialog")
            tryCompare(popup, "visible", true)
            verify(popup.height <= scene.height)
            verify(popup.width <= scene.width)
            popup.close()
            tryCompare(popup, "visible", false)
        }
    }
}
