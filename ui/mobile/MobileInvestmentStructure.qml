import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: panel

    property var allocation: ({})
    property var contributions: ({})
    property string currency: "RUB"
    property int groupIndex: 0

    readonly property var colors: ["#3F735F", "#6B92AD", "#B68B4C", "#8B749F", "#597C8A", "#C08A7B", "#9AA766", "#66728B"]
    readonly property var holdings: (groupIndex === 1 ? allocation.types
        : groupIndex === 2 ? allocation.accounts
        : groupIndex === 3 ? allocation.currencies : allocation.items) || []
    readonly property var resultRows: contributions.items || []
    readonly property real maximumResult: {
        let maximum = 0;
        for (let i = 0; i < resultRows.length; ++i)
            maximum = Math.max(maximum, Math.abs(Number(resultRows[i].amountMinor)));
        return Math.max(maximum, 1);
    }

    function available(value) {
        return value !== undefined && value !== null && isFinite(Number(value));
    }
    function amount(value) {
        return available(value) ? MobileFormat.money(value, panel.currency, false) : "—";
    }
    function percent(value) {
        return available(value) ? Number(value).toLocaleString(MobileFormat.locale(), "f", 2) + " %" : "—";
    }

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: qsTr("Структура вложений")
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeHeading
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        MobileInfoButton {
            metric: "structure"
            displayValue: panel.amount(panel.allocation.capitalMinor)
        }
    }

    Flow {
        Layout.fillWidth: true
        spacing: 8

        Repeater {
            model: [qsTr("По активам"), qsTr("По типам активов"), qsTr("По счетам"), qsTr("По валютам счетов")]

            delegate: MobileChip {
                required property string modelData
                required property int index
                text: modelData
                selected: panel.groupIndex === index
                onClicked: panel.groupIndex = index
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("На конец выбранного периода · Стоимость: %1").arg(panel.amount(panel.allocation.capitalMinor))
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Canvas {
        id: donut
        visible: !!panel.allocation.pieAvailable
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 190
        Layout.preferredHeight: 190
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            if (!panel.allocation.pieAvailable)
                return;
            let start = -Math.PI / 2;
            for (let i = 0; i < panel.holdings.length; ++i) {
                const share = Number(panel.holdings[i].percent || 0);
                if (share <= 0)
                    continue;
                const end = start + share / 100 * Math.PI * 2;
                ctx.beginPath();
                ctx.arc(width / 2, height / 2, 76, start, end);
                ctx.strokeStyle = panel.colors[i % panel.colors.length];
                ctx.lineWidth = 28;
                ctx.stroke();
                start = end;
            }
        }

        Text {
            anchors.centerIn: parent
            text: "100 %"
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeHeading
            font.weight: Font.Bold
        }
    }
    onHoldingsChanged: donut.requestPaint()
    onAllocationChanged: donut.requestPaint()

    Text {
        Layout.fillWidth: true
        visible: !panel.allocation.pieAvailable
        text: !panel.allocation.available
            ? qsTr("Для долей нужна полная оценка всех активов на конец периода.")
            : qsTr("При нулевой или отрицательной стоимости круговые доли не рассчитываются. Суммы приведены ниже.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Repeater {
        model: panel.holdings

        delegate: RowLayout {
            required property var modelData
            required property int index

            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 3
                color: panel.colors[index % panel.colors.length]
            }
            Text {
                Layout.fillWidth: true
                text: modelData.label
                wrapMode: Text.Wrap
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeCaption
            }
            Text {
                text: panel.amount(modelData.amountMinor)
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeCaption
            }
            Text {
                Layout.preferredWidth: 66
                horizontalAlignment: Text.AlignRight
                text: panel.percent(panel.allocation.pieAvailable ? modelData.percent : null)
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: (panel.allocation.adjustments || []).length > 0
        text: qsTr("Неучтённый в деньгах результат контрактов: %1. Это переоценка, а не стоимость вложенного капитала; в круговые доли она не входит.").arg(panel.amount(panel.allocation.adjustmentMinor))
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Repeater {
        model: panel.allocation.adjustments || []

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: modelData.label
                wrapMode: Text.Wrap
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
            }
            Text {
                text: panel.amount(modelData.amountMinor)
                color: MobileTheme.accent
                font.pixelSize: MobileTheme.sizeCaption
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Валюты — валюты оценки на счетах, а не валютная экспозиция компаний и фондов. Заблокированное обеспечение контрактов не добавляется повторно к деньгам.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 8
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: qsTr("Вклад активов в результат")
            color: MobileTheme.accent
            font.pixelSize: MobileTheme.sizeHeading
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        MobileInfoButton {
            metric: "contribution"
            displayValue: panel.amount(panel.contributions.totalMinor)
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("За выбранный период, включая закрытые позиции, выплаты и учтённые расходы. Пополнения и выводы исключены.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        visible: !panel.contributions.available
        text: qsTr("Нужны достоверные начальная и конечная оценки, денежные потоки и курсы валют.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Repeater {
        model: panel.contributions.available ? panel.resultRows : []

        delegate: ColumnLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: modelData.label
                    wrapMode: Text.Wrap
                    color: MobileTheme.accent
                    font.pixelSize: MobileTheme.sizeCaption
                }
                Text {
                    text: panel.amount(modelData.amountMinor)
                    color: modelData.amountMinor < 0 ? MobileTheme.red : MobileTheme.income
                    font.pixelSize: MobileTheme.sizeCaption
                    font.weight: Font.Bold
                }
            }
            Item {
                Layout.fillWidth: true
                implicitHeight: 12

                Rectangle {
                    x: parent.width / 2
                    width: 1
                    height: parent.height
                    color: MobileTheme.line
                }
                Rectangle {
                    y: 2
                    height: 8
                    radius: 3
                    width: Math.abs(Number(modelData.amountMinor)) / panel.maximumResult * (parent.width / 2 - 2)
                    x: modelData.amountMinor < 0 ? parent.width / 2 - width : parent.width / 2 + 1
                    color: modelData.amountMinor < 0 ? MobileTheme.red : MobileTheme.income
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !!panel.contributions.available
        text: qsTr("Валютная переоценка счетов: %1\nНеобъяснённая разница: %2\nИтого: %3")
            .arg(panel.amount(panel.contributions.fxMinor))
            .arg(panel.amount(panel.contributions.unexplainedMinor))
            .arg(panel.amount(panel.contributions.totalMinor))
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Результат активов рассчитан при курсе валют счетов на начало периода. Валютная переоценка показана отдельно. Длина полос отражает сумму прибыли или убытка, а не долю вложений.")
        wrapMode: Text.Wrap
        color: MobileTheme.muted
        font.pixelSize: MobileTheme.sizeCaption
    }
}
