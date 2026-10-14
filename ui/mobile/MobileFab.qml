import QtQuick

Item {
    id: fab

    property string icon: "plus"
    signal clicked()

    implicitWidth: 56
    implicitHeight: 56

    Rectangle {
        x: MobileTheme.shadowDepth
        y: MobileTheme.shadowDepth
        width: 52
        height: 52
        radius: 16
        color: MobileTheme.accentSoft
    }

    Rectangle {
        width: 52
        height: 52
        radius: 16
        color: tap.pressed ? MobileTheme.navSelected : MobileTheme.accent

        MobileIcon {
            anchors.centerIn: parent
            name: fab.icon
            width: 24
            height: 24
            color: MobileTheme.accentText
        }
    }

    TapHandler {
        id: tap
        onTapped: fab.clicked()
    }
}
