import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: panel
    property var controller
    property color ink: "#031528"
    property color muted: "#687483"
    property color line: "#D6D8D3"
    property color accent: "#39866B"
    readonly property var settings: controller ? (controller.performanceDeposit || ({})) : ({})
    readonly property var comparison: controller ? ((controller.investmentPerformance || ({})).depositComparison || ({})) : ({})
    readonly property var points: comparison.available ? (comparison.series || []) : []
    readonly property var modes: ["none", "monthly", "daily"]
    property string synchronizedSettings: "!"
    implicitHeight: content.implicitHeight

    function syncSettings() {
        var key=JSON.stringify(settings)
        if(key===synchronizedSettings) return
        synchronizedSettings=key
        rate.text=settings.rate || ""
        var index=modes.indexOf(settings.capitalization || "monthly")
        capitalization.currentIndex=index>=0 ? index : 1
        error.text=""
    }
    function amount(value) {
        return value===null || value===undefined ? "—" : Number(Number(value)/100).toLocaleString(Qt.locale(),'f',2)+" "+(comparison.currency || "RUB")
    }
    function date(value) { return value ? Qt.formatDate(new Date(value+"T12:00:00"),"dd.MM.yyyy") : "—" }
    onSettingsChanged: syncSettings()
    Component.onCompleted: syncSettings()
    onPointsChanged: { plot.hovered=-1; graph.requestPaint() }

    ColumnLayout {
        id: content
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Label { text:qsTr("А если бы деньги лежали на вкладе?"); color:panel.ink; font.pixelSize:17; font.bold:true; wrapMode:Text.WordWrap; Layout.fillWidth:true }
            InvestmentMetricHelp { metric:"deposit"; ink:panel.ink; muted:panel.muted }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.width<720 ? 2 : 4
            columnSpacing:16; rowSpacing:8
            ColumnLayout {
                Layout.fillWidth:true; spacing:8
                Label { text:qsTr("Годовая ставка, %"); color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true }
                TextField {
                    id: rate; objectName:"depositRate"
                    Layout.fillWidth:true; Layout.minimumWidth:100
                    implicitHeight:40; maximumLength:24
                    placeholderText:qsTr("Укажите ставку")
                    inputMethodHints:Qt.ImhFormattedNumbersOnly
                    Accessible.name:qsTr("Годовая ставка условного вклада")
                }
            }
            ColumnLayout {
                Layout.fillWidth:true; spacing:8
                Label { text:qsTr("Капитализация процентов"); color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true }
                ComboBox {
                    id: capitalization; objectName:"depositCapitalization"
                    Layout.fillWidth:true; implicitHeight:40
                    model:[qsTr("Без капитализации"),qsTr("Ежемесячная капитализация"),qsTr("Ежедневная капитализация")]
                    Accessible.name:qsTr("Капитализация процентов")
                }
            }
            Button {
                objectName:"calculateDeposit"
                text:qsTr("Рассчитать"); implicitHeight:40; Layout.minimumWidth:112; Layout.alignment:Qt.AlignBottom
                enabled:!!panel.controller
                onClicked:error.text=panel.controller.setPerformanceDeposit(rate.text,panel.modes[capitalization.currentIndex])
                    ? "" : qsTr("Введите ставку от 0 до 1000 %, до четырёх знаков после запятой.")
            }
            Label { text:qsTr("Валюта отчёта: %1").arg(panel.comparison.currency || "RUB"); wrapMode:Text.WordWrap; color:panel.muted; Layout.fillWidth:true; Layout.alignment:Qt.AlignBottom }
        }
        Label { id:error; objectName:"depositError"; visible:text.length>0; color:"#B94F48"; wrapMode:Text.WordWrap; Layout.fillWidth:true }
        Label {
            visible:!panel.comparison.available
            text:panel.comparison.reason || qsTr("Укажите ставку и накопите дневные оценки выбранных счетов.")
            color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true
        }
        Label {
            visible:!!panel.comparison.available
            text:qsTr("%1 — %2 · Начальный капитал: %3").arg(panel.date(panel.comparison.from)).arg(panel.date(panel.comparison.to)).arg(panel.amount(panel.comparison.openingMinor))
                + (panel.comparison.liveDay ? "\n"+qsTr("Оценка текущего дня ещё может измениться.") : "")
            color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true
        }
        GridLayout {
            visible:!!panel.comparison.available
            Layout.fillWidth:true; columns:panel.width<600 ? 1 : 3; columnSpacing:16; rowSpacing:16
            Repeater {
                model:[{title:qsTr("Результат вложений"),value:panel.comparison.portfolioGainMinor},
                    {title:qsTr("Доход условного вклада"),value:panel.comparison.depositGainMinor},
                    {title:qsTr("Разница результатов"),value:panel.comparison.differenceMinor}]
                delegate:Rectangle {
                    required property var modelData
                    Layout.fillWidth:true; Layout.preferredHeight:104
                    color:"transparent"; radius:8; border.color:panel.line
                    ColumnLayout {
                        anchors.fill:parent; anchors.margins:12
                        Label { text:modelData.title; color:panel.muted; wrapMode:Text.WordWrap; Layout.fillWidth:true }
                        Label { text:panel.amount(modelData.value); color:panel.ink; font.pixelSize:22; font.bold:true; elide:Text.ElideRight; Layout.fillWidth:true }
                    }
                }
            }
        }
        Item {
            id:plot
            visible:!!panel.comparison.available && panel.points.length>=2
            Layout.fillWidth:true; Layout.preferredHeight:264
            property int hovered:-1
            readonly property real leftMargin:88
            readonly property real rightMargin:16
            readonly property real topMargin:16
            readonly property real bottomMargin:32
            readonly property real plotWidth:Math.max(1,width-leftMargin-rightMargin)
            readonly property real plotHeight:Math.max(1,height-topMargin-bottomMargin)
            readonly property var bounds: {
                var low=Infinity,high=-Infinity
                for(var i=0;i<panel.points.length;++i){low=Math.min(low,Number(panel.points[i].portfolioMinor)/100,Number(panel.points[i].depositMinor)/100);high=Math.max(high,Number(panel.points[i].portfolioMinor)/100,Number(panel.points[i].depositMinor)/100)}
                if(!isFinite(low))return {low:0,high:1}
                var span=Math.max(1,high-low);return {low:low-span*0.08,high:high+span*0.08}
            }
            onWidthChanged:graph.requestPaint()
            onHeightChanged:graph.requestPaint()
            onBoundsChanged:graph.requestPaint()
            Canvas {
                id:graph; anchors.fill:parent
                onPaint: {
                    var ctx=getContext("2d");ctx.clearRect(0,0,width,height);var data=panel.points;if(data.length<2)return
                    var x=function(i){return plot.leftMargin+plot.plotWidth*i/(data.length-1)}
                    var y=function(v){return plot.topMargin+(plot.bounds.high-Number(v)/100)/(plot.bounds.high-plot.bounds.low)*plot.plotHeight}
                    ctx.strokeStyle=panel.line;ctx.lineWidth=1
                    for(var grid=0;grid<5;++grid){var gy=plot.topMargin+plot.plotHeight*grid/4;ctx.beginPath();ctx.moveTo(plot.leftMargin,gy);ctx.lineTo(width-plot.rightMargin,gy);ctx.stroke()}
                    var draw=function(key,color){ctx.beginPath();ctx.moveTo(x(0),y(data[0][key]));for(var i=1;i<data.length;++i)ctx.lineTo(x(i),y(data[i][key]));ctx.strokeStyle=color;ctx.lineWidth=2;ctx.stroke()}
                    draw("portfolioMinor",panel.ink);draw("depositMinor",panel.accent)
                    if(plot.hovered>=0){ctx.beginPath();ctx.moveTo(x(plot.hovered),plot.topMargin);ctx.lineTo(x(plot.hovered),plot.topMargin+plot.plotHeight);ctx.strokeStyle=panel.muted;ctx.lineWidth=1;ctx.stroke()}
                }
            }
            Repeater {
                model:5
                delegate:Label {
                    required property int index
                    x:0; y:plot.topMargin+plot.plotHeight*index/4-height/2; width:plot.leftMargin-8
                    text: {
                        var n=plot.bounds.high-(plot.bounds.high-plot.bounds.low)*index/4
                        return Math.abs(n)>=1000000 ? (n/1000000).toLocaleString(Qt.locale(),'f',1)+qsTr(" млн")
                            : Math.abs(n)>=10000 ? (n/1000).toLocaleString(Qt.locale(),'f',1)+qsTr(" тыс.") : n.toLocaleString(Qt.locale(),'f',0)
                    }
                    horizontalAlignment:Text.AlignRight; color:panel.muted; font.pixelSize:12
                }
            }
            Label { x:plot.leftMargin; anchors.bottom:parent.bottom; text:panel.date(panel.comparison.from); color:panel.muted; font.pixelSize:12 }
            Label { anchors.right:parent.right; anchors.rightMargin:plot.rightMargin; anchors.bottom:parent.bottom; text:panel.date(panel.comparison.to); color:panel.muted; font.pixelSize:12 }
            MouseArea {
                anchors.fill:parent; hoverEnabled:true; acceptedButtons:Qt.NoButton
                onPositionChanged:function(mouse){plot.hovered=mouse.x>=plot.leftMargin && mouse.x<=width-plot.rightMargin && mouse.y>=plot.topMargin && mouse.y<=height-plot.bottomMargin ? Math.max(0,Math.min(panel.points.length-1,Math.round((mouse.x-plot.leftMargin)/plot.plotWidth*(panel.points.length-1)))) : -1;graph.requestPaint()}
                onExited:{plot.hovered=-1;graph.requestPaint()}
                ToolTip.visible:plot.hovered>=0 && plot.hovered<panel.points.length
                ToolTip.text:plot.hovered>=0 && plot.hovered<panel.points.length ? panel.date(panel.points[plot.hovered].date)+"\n"+qsTr("Вложения: %1").arg(panel.amount(panel.points[plot.hovered].portfolioMinor))+"\n"+qsTr("Вклад: %1").arg(panel.amount(panel.points[plot.hovered].depositMinor)) : ""
            }
        }
        RowLayout {
            visible:!!panel.comparison.available; Layout.fillWidth:true
            Rectangle { implicitWidth:20; implicitHeight:3; color:panel.ink }
            Label { text:qsTr("Вложения"); color:panel.ink }
            Rectangle { implicitWidth:20; implicitHeight:3; color:panel.accent }
            Label { text:qsTr("Условный вклад"); color:panel.ink }
            Item { Layout.fillWidth:true }
        }
        Label {
            Layout.fillWidth:true; wrapMode:Text.WordWrap; color:panel.muted; font.pixelSize:12
            text:qsTr("Одинаковый начальный капитал и внешние пополнения/выводы. Ставка постоянная, вклад в валюте отчёта. Между дневными оценками пополнения считаются внесёнными до начисления процентов, выводы — после. Проценты: ставка / 365 или 366 за календарный день. Ежемесячная капитализация — в дату открытия, в коротком месяце в его последний день. Без капитализации проценты хранятся отдельно и не приносят доход; выводы сначала расходуют их. Начисленные, но ещё не выплаченные проценты включены в итог, но не доступны для вывода. Налоги, ограничения банка и потеря процентов при досрочном закрытии не учитываются.")
        }
    }
}
