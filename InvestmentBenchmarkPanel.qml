import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    required property var controller
    property color ink: "#031528"
    property color muted: "#687483"
    property color line: "#D8D7C7"
    property color accent: "#3F735F"
    readonly property var selection: controller ? (controller.benchmarkSelection || ({})) : ({})
    readonly property var comparison: controller ? (controller.benchmarkComparison || ({})) : ({})
    readonly property var points: comparison.points || []
    readonly property var indices: controller ? (controller.benchmarkIndices || []) : []
    readonly property var choices: [{key:"",name:qsTr("Без сравнения")}].concat(indices).concat([{key:"fund",name:qsTr("Биржевой фонд Мосбиржи…")}])
    implicitHeight: content.implicitHeight
    function available(v) { return v !== null && v !== undefined && isFinite(Number(v)) }
    function percent(v) { return available(v) ? Number(v).toLocaleString(Qt.locale(), 'f', 2) + " %" : "—" }
    function date(s) { return s ? s.substr(8,2)+"."+s.substr(5,2)+"."+s.substr(0,4) : "—" }
    function selectionIndex() { if (selection.kind === "fund") return choices.length-1; for(var i=0;i<choices.length;++i) if(choices[i].key===selection.key) return i; return 0 }
    onPointsChanged: { plot.hovered=-1; graph.requestPaint() }
    Connections {
        target: panel.controller
        function onBenchmarkChanged() { choice.currentIndex=panel.selectionIndex() }
    }
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Сравнение с индексом или фондом"); color: panel.ink; font.pixelSize: 17; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            ComboBox {
                id: choice
                Layout.preferredWidth: Math.min(360,panel.width*0.48)
                model: panel.choices
                textRole: "name"
                currentIndex: panel.selectionIndex()
                onActivated: {
                    if(panel.choices[currentIndex].key==="fund") fundDialog.open()
                    else panel.controller.selectBenchmarkIndex(panel.choices[currentIndex].key)
                }
            }
        }
        RowLayout {
            visible: !!panel.selection.key
            Layout.fillWidth: true
            Label { text: panel.selection.name || panel.selection.symbol || ""; color: panel.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Button { text: qsTr("Выбрать другой фонд"); visible: panel.selection.kind==="fund"; onClicked: fundDialog.open() }
            Button { text: panel.controller && panel.controller.benchmarkBusy ? qsTr("Загрузка…") : qsTr("Обновить историю"); enabled: panel.controller && !panel.controller.benchmarkBusy && !!panel.comparison.canRefresh; onClicked: panel.controller.refreshBenchmark() }
        }
        Label {
            visible: !!panel.selection.key
            Layout.fillWidth: true
            color: panel.muted
            wrapMode: Text.WordWrap
            text: panel.selection.kind==="fund"
                ? qsTr("Сравнение по биржевой цене пая. Выплаты владельцу пая и дробления отдельно не учитываются. Расходы управления, уже отражённые в цене, остаются в ней.")
                : panel.selection.totalReturn ? qsTr("Полная доходность брутто: индекс включает реинвестирование выплат до налогов. Ваши личные комиссии и налоги в ориентире не учитываются.")
                : qsTr("Ценовой индекс: дивиденды не включены. Доходность вашего счёта учитывает записанные выплаты и расходы.")
        }
        Label { visible: panel.controller && panel.controller.benchmarkError.length>0; text: panel.controller ? panel.controller.benchmarkError : ""; color: "#B94F48"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label {
            visible: !!panel.comparison.available && panel.comparison.maximumQuoteAgeDays>7
            Layout.fillWidth:true;wrapMode:Text.WordWrap;color:panel.muted
            text:qsTr("В части периода последняя биржевая цена старше семи дней. Торги могли не проводиться или быть приостановлены. Фактическая дата цены видна в подсказке на графике.")
        }
        Label { visible: !panel.comparison.available; text: panel.comparison.reason || qsTr("Выберите ориентир для сравнения."); color: panel.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label {
            visible: !!panel.comparison.available
            Layout.fillWidth: true
            color: panel.muted
            wrapMode: Text.WordWrap
            text: qsTr("%1 — %2 · Валюта: %3 · Текущий день исключён").arg(panel.date(panel.comparison.from)).arg(panel.date(panel.comparison.to)).arg(panel.controller ? panel.controller.investmentPerformance.currency : "")
        }
        GridLayout {
            visible: !!panel.comparison.available
            Layout.fillWidth: true
            columns: 3
            Repeater {
                model: [{title:qsTr("Ваш результат"),value:panel.percent(panel.comparison.portfolioPercent)},
                    {title:qsTr("Результат ориентира"),value:panel.percent(panel.comparison.benchmarkPercent)},
                    {title:qsTr("Разница результатов"),value:panel.available(panel.comparison.differencePoints)?Number(panel.comparison.differencePoints).toLocaleString(Qt.locale(),'f',2)+qsTr(" п.п."):"—"}]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 86
                    color: "transparent"
                    radius: 8
                    border.color: panel.line
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 12
                        Label { text:modelData.title; color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true }
                        Label { text:modelData.value; color:panel.ink; font.pixelSize:22; font.bold:true; elide:Text.ElideRight; Layout.fillWidth:true }
                    }
                }
            }
        }
        Item {
            id: plot
            visible: !!panel.comparison.available && panel.points.length>=2
            Layout.fillWidth: true
            Layout.preferredHeight: 254
            property int hovered: -1
            readonly property real leftMargin: 68
            readonly property real rightMargin: 16
            readonly property real topMargin: 14
            readonly property real bottomMargin: 30
            readonly property real plotWidth: Math.max(1,width-leftMargin-rightMargin)
            readonly property real plotHeight: Math.max(1,height-topMargin-bottomMargin)
            readonly property var bounds: {
                var low=0,high=0
                for(var i=0;i<panel.points.length;++i){low=Math.min(low,panel.points[i].portfolio,panel.points[i].benchmark);high=Math.max(high,panel.points[i].portfolio,panel.points[i].benchmark)}
                var span=Math.max(1,high-low);return {low:low-span*0.08,high:high+span*0.08}
            }
            onWidthChanged: graph.requestPaint()
            onHeightChanged: graph.requestPaint()
            onBoundsChanged: graph.requestPaint()
            Canvas {
                id: graph
                anchors.fill: parent
                onPaint: {
                    var ctx=getContext("2d");ctx.clearRect(0,0,width,height);var data=panel.points;if(data.length<2)return
                    var x=function(i){return plot.leftMargin+plot.plotWidth*i/(data.length-1)}
                    var y=function(v){return plot.topMargin+(plot.bounds.high-v)/(plot.bounds.high-plot.bounds.low)*plot.plotHeight}
                    ctx.strokeStyle=panel.line;ctx.lineWidth=1
                    for(var grid=0;grid<5;++grid){var gy=plot.topMargin+plot.plotHeight*grid/4;ctx.beginPath();ctx.moveTo(plot.leftMargin,gy);ctx.lineTo(width-plot.rightMargin,gy);ctx.stroke()}
                    var draw=function(key,color){ctx.beginPath();ctx.moveTo(x(0),y(data[0][key]));for(var i=1;i<data.length;++i)ctx.lineTo(x(i),y(data[i][key]));ctx.strokeStyle=color;ctx.lineWidth=2;ctx.stroke()}
                    draw("portfolio",panel.ink);draw("benchmark",panel.accent)
                    if(plot.hovered>=0 && plot.hovered<data.length){ctx.beginPath();ctx.moveTo(x(plot.hovered),plot.topMargin);ctx.lineTo(x(plot.hovered),plot.topMargin+plot.plotHeight);ctx.strokeStyle=panel.muted;ctx.lineWidth=1;ctx.stroke()}
                }
            }
            Repeater {
                model: 5
                delegate: Label {
                    required property int index
                    x:0;y:plot.topMargin+plot.plotHeight*index/4-height/2;width:plot.leftMargin-8
                    text:Number(plot.bounds.high-(plot.bounds.high-plot.bounds.low)*index/4).toLocaleString(Qt.locale(),'f',1)+" %"
                    horizontalAlignment:Text.AlignRight;color:panel.muted;font.pixelSize:12
                }
            }
            Label { x:plot.leftMargin;anchors.bottom:parent.bottom;text:panel.date(panel.comparison.from);color:panel.muted;font.pixelSize:12 }
            Label { anchors.right:parent.right;anchors.rightMargin:plot.rightMargin;anchors.bottom:parent.bottom;text:panel.date(panel.comparison.to);color:panel.muted;font.pixelSize:12 }
            MouseArea {
                anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.NoButton
                onPositionChanged:function(mouse){plot.hovered=mouse.x>=plot.leftMargin && mouse.x<=width-plot.rightMargin && mouse.y>=plot.topMargin && mouse.y<=height-plot.bottomMargin?Math.max(0,Math.min(panel.points.length-1,Math.round((mouse.x-plot.leftMargin)/plot.plotWidth*(panel.points.length-1)))):-1;graph.requestPaint()}
                onExited:{plot.hovered=-1;graph.requestPaint()}
                ToolTip.visible:plot.hovered>=0 && plot.hovered<panel.points.length
                ToolTip.text:plot.hovered>=0 && plot.hovered<panel.points.length?panel.date(panel.points[plot.hovered].date)+"\n"+qsTr("Ваш результат: %1").arg(panel.percent(panel.points[plot.hovered].portfolio))+"\n"+qsTr("Ориентир: %1").arg(panel.percent(panel.points[plot.hovered].benchmark))+"\n"+qsTr("Дата цены ориентира: %1").arg(panel.date(panel.points[plot.hovered].quoteDate)):""
            }
        }
        RowLayout {
            visible: !!panel.comparison.available
            Rectangle { implicitWidth:20;implicitHeight:3;color:panel.ink }
            Label { text:qsTr("Ваш результат");color:panel.ink }
            Rectangle { implicitWidth:20;implicitHeight:3;color:panel.accent }
            Label { text:panel.selection.symbol || qsTr("Ориентир");color:panel.ink }
            Item { Layout.fillWidth:true }
        }
        Label {
            visible: !!panel.selection.key
            Layout.fillWidth:true;wrapMode:Text.WordWrap;color:panel.muted;font.pixelSize:12
            text:qsTr("Обе линии начинаются с нуля и показывают номинальную доходность без поправки на инфляцию. Разница указана в процентных пунктах. Доходность счёта очищена от пополнений и выводов. Сравнение идёт по календарным датам и ценам закрытия, без внутридневной синхронизации. В дни без новой биржевой цены сохраняется последняя предыдущая цена; её дата показана в подсказке.")
        }
    }
    Dialog {
        id: fundDialog
        parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(620,parent.width-32);modal:true;padding:24
        title:qsTr("Биржевой фонд для сравнения")
        onClosed:choice.currentIndex=panel.selectionIndex()
        contentItem:ColumnLayout {
            spacing:16
            RowLayout {
                Layout.fillWidth:true
                TextField { id:fundQuery;Layout.fillWidth:true;placeholderText:qsTr("Название или обозначение фонда");onAccepted:panel.controller.searchBenchmarkFunds(text) }
                Button { text:qsTr("Найти");enabled:panel.controller && !panel.controller.benchmarkSearchBusy;onClicked:panel.controller.searchBenchmarkFunds(fundQuery.text) }
            }
            Label { visible:panel.controller && panel.controller.benchmarkSearchBusy;text:qsTr("Ищем биржевые фонды…");color:panel.muted }
            Label { visible:panel.controller && panel.controller.benchmarkSearchError.length>0;text:panel.controller?panel.controller.benchmarkSearchError:"";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:panel.muted }
            ListView {
                Layout.fillWidth:true;Layout.preferredHeight:260;clip:true
                model:panel.controller?panel.controller.benchmarkSearchResults:[]
                delegate:ItemDelegate {
                    required property var modelData
                    width:ListView.view.width;text:modelData.symbol+" · "+modelData.name
                    onClicked:{panel.controller.selectBenchmarkFund(modelData.index);fundDialog.close()}
                }
            }
            Button { text:qsTr("Закрыть");onClicked:fundDialog.close() }
        }
    }
}
