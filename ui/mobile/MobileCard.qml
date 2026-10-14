import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: card

    property bool hard: false
    property color fill: MobileTheme.panel
    property int cardPadding: 16
    property int gap: 12

    default property alias content: column.data

    padding: cardPadding
    rightPadding: cardPadding + (hard ? MobileTheme.shadowDepth : 0)
    bottomPadding: cardPadding + (hard ? MobileTheme.shadowDepth : 0)

    contentItem: ColumnLayout {
        id: column
        spacing: card.gap
    }

    background: Item {
        Rectangle {
            visible: card.hard
            x: MobileTheme.shadowDepth
            y: MobileTheme.shadowDepth
            width: face.width
            height: face.height
            radius: MobileTheme.radiusCard
            color: MobileTheme.accent
        }
        Rectangle {
            id: face
            width: parent.width - (card.hard ? MobileTheme.shadowDepth : 0)
            height: parent.height - (card.hard ? MobileTheme.shadowDepth : 0)
            radius: MobileTheme.radiusCard
            color: card.fill
            border.width: 1
            border.color: card.hard ? MobileTheme.accent : MobileTheme.line
        }
    }
}
