import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Average annual return of the chosen instruments. Each rate is weighted by the money the instrument holds now.
Rectangle {
    id: panel
    required property var controller
    property color surface: "#FFFFF8"
    property color ink: "#031528"
    property color muted: "#687483"
    property color line: "#D8D7C7"
    property color positive: "#3F735F"
    property color negative: "#B94F48"

    readonly property var items: controller ? (controller.averageReturnItems || []) : []
    readonly property var summary: controller ? (controller.averageReturnSummary || ({})) : ({})
    readonly property var excluded: summary.excluded || []
    readonly property var groups: summary.groups || []
    readonly property bool merged: summary.merged !== false
    readonly property var pieColors: ["#031528", "#3F735F", "#B9873C", "#6B7FA3", "#8C5E7A", "#4E8C8A", "#A3A36B", "#B94F48"]

    function isIncluded(id) {
        return excluded.indexOf(id) < 0;
    }

    function setIncluded(id, included) {
        const next = excluded.filter(function (value) { return value !== id; });
        if (!included)
            next.push(id);
        controller.setAverageReturnSelection(next, merged);
    }

    function setMerged(value) {
        controller.setAverageReturnSelection(excluded, value);
    }

    function percentText(value) {
        if (value === null || value === undefined)
            return "—";
        return Number(value).toLocaleString(Qt.locale("ru_RU"), "f", 1) + " %";
    }

    function moneyText(minor) {
        return Number(minor / 100).toLocaleString(Qt.locale("ru_RU"), "f", 0) + " ₽";
    }

    color: surface
    radius: 14
    border.width: 1
    border.color: line
    implicitHeight: body.implicitHeight + 36

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 12

        Label {
            text: qsTr("Средняя годовая доходность")
            color: panel.ink
            font.pixelSize: 20
            font.bold: true
            Layout.fillWidth: true
        }

        CheckBox {
            text: qsTr("Объединить криптовалюту и инвестиции?")
            checked: panel.merged
            onToggled: panel.setMerged(checked)
        }

        Label {
            visible: panel.items.length === 0
            text: qsTr("Пока нет инструментов: добавьте позицию, вклад или кошелёк.")
            color: panel.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: panel.items
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 8

                CheckBox {
                    checked: panel.isIncluded(modelData.id)
                    onToggled: panel.setIncluded(modelData.id, checked)
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: modelData.title
                        color: panel.ink
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: panel.moneyText(modelData.valueMinor) + " · "
                              + (modelData.annualPercent === null || modelData.annualPercent === undefined
                                 ? modelData.reason : panel.percentText(modelData.annualPercent) + " в год")
                        color: panel.muted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Repeater {
            model: panel.groups
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: modelData.title
                    color: panel.muted
                    font.pixelSize: 14
                    Layout.fillWidth: true
                }
                Label {
                    text: panel.percentText(modelData.annualPercent)
                    color: panel.ink
                    font.pixelSize: 30
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("учтено %1 из %2 · денег сейчас %3")
                        .arg(modelData.counted).arg(modelData.shown).arg(panel.moneyText(modelData.valueMinor))
                    color: panel.muted
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Canvas {
                    id: pie
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 180
                    property var slices: modelData.slices || []
                    onSlicesChanged: requestPaint()
                    Component.onCompleted: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const cx = width / 2;
                        const cy = height / 2;
                        const radius = Math.min(width, height) / 2 - 2;
                        let start = -Math.PI / 2;
                        for (let i = 0; i < slices.length; ++i) {
                            const end = start + slices[i].share * 2 * Math.PI;
                            ctx.beginPath();
                            ctx.moveTo(cx, cy);
                            ctx.arc(cx, cy, radius, start, end);
                            ctx.closePath();
                            ctx.fillStyle = panel.pieColors[i % panel.pieColors.length];
                            ctx.fill();
                            start = end;
                        }
                    }
                }
                Repeater {
                    model: modelData.slices || []
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            radius: 2
                            color: panel.pieColors[index % panel.pieColors.length]
                        }
                        Label {
                            text: modelData.title
                            color: panel.ink
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: Math.round(modelData.share * 100) + " %"
                            color: panel.muted
                        }
                    }
                }
            }
        }

        Label {
            text: qsTr("Вес инструмента — сумма денег в нём сейчас. Учитываются инструменты с историей не короче 30 дней. Криптовалюта — монеты на бирже по сделкам за два года; цена покупки переведена в рубли по курсу ЦБ на день сделки. Кошельки и стейблкоины не учитываются. Если монета пришла не сделкой (ввод или вывод), доходность не считается, причина написана в списке.")
            color: panel.muted
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
