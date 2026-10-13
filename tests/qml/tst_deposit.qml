import QtQuick
import QtQuick.Controls
import QtTest
import "../.."

Item {
    id: scene
    width: 900; height: 900
    QtObject {
        id: controller
        property var performanceDeposit: ({rate:"16.0000",capitalization:"monthly"})
        property var investmentPerformance: ({depositComparison:{available:true,configured:true,currency:"RUB",from:"2025-01-01",to:"2025-01-03",
            openingMinor:100000,portfolioGainMinor:5000,depositGainMinor:88,differenceMinor:4912,
            series:[{date:"2025-01-01",portfolioMinor:100000,depositMinor:100000},{date:"2025-01-02",portfolioMinor:102000,depositMinor:100044},{date:"2025-01-03",portfolioMinor:105000,depositMinor:100088}]}})
        property string lastRate:""
        property string lastMode:""
        function setPerformanceDeposit(rate, mode) {
            lastRate=rate;lastMode=mode
            if(rate==="invalid")return false
            performanceDeposit={rate:rate,capitalization:mode}
            return true
        }
    }
    InvestmentDepositPanel { id:panel; width:scene.width; controller:controller }
    TestCase {
        name:"DepositComparisonPanel"
        when:windowShown
        function test_applyAndPreserveUnsavedInputDuringValuationRefresh() {
            var rate=findChild(panel,"depositRate"),mode=findChild(panel,"depositCapitalization")
            compare(rate.text,"16.0000");compare(mode.currentIndex,1)
            rate.text="20,5";mode.currentIndex=2
            controller.performanceDeposit={rate:"16.0000",capitalization:"monthly"}
            compare(rate.text,"20,5");compare(mode.currentIndex,2)
            mouseClick(findChild(panel,"calculateDeposit"))
            compare(controller.lastRate,"20,5");compare(controller.lastMode,"daily")
            rate.text="invalid";mouseClick(findChild(panel,"calculateDeposit"))
            verify(findChild(panel,"depositError").text.length>0)
            controller.performanceDeposit={rate:"12.0000",capitalization:"none"}
            compare(rate.text,"12.0000");compare(mode.currentIndex,0)
        }
        function test_graphAndUnavailableState() {
            compare(panel.points.length,3)
            verify(panel.implicitHeight>300)
            controller.investmentPerformance={depositComparison:{available:false,configured:true,currency:"RUB",reason:"Недостаточно оценок"}}
            compare(panel.points.length,0)
            wait(20)
            verify(panel.implicitHeight>0)
        }
    }
}
