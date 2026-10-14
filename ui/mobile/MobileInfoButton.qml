import QtQuick

Rectangle {
    id: button

    property string metric: ""
    property string displayValue: ""

    implicitWidth: 28
    implicitHeight: 28
    radius: 14
    color: tap.pressed ? MobileTheme.hover : MobileTheme.pale

    Text {
        anchors.centerIn: parent
        text: "?"
        color: MobileTheme.navSelected
        font.pixelSize: MobileTheme.sizeBody
        font.weight: Font.DemiBold
    }

    TapHandler {
        id: tap
        onTapped: {
            if (!sheetLoader.item)
                sheetLoader.active = true;
            sheetLoader.item.openFor(button.metric, button.displayValue);
        }
    }

    Loader {
        id: sheetLoader
        active: false
        sourceComponent: MobileMetricSheet {}
    }
}
