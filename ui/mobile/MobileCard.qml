import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: card

    // Kept for the pages that set it: every card now has the solid accent shadow
    property bool hard: false
    property color fill: MobileTheme.panel
    property int cardPadding: 16
    property int gap: 12

    default property alias content: column.data

    padding: cardPadding
    rightPadding: cardPadding
    bottomPadding: cardPadding + MobileTheme.shadowDepth

    contentItem: ColumnLayout {
        id: column
        spacing: card.gap
    }

    // Solid accent shadow 4 px below the card (neobrutalism, as in the Figma design)
    background: Item {
        Rectangle {
            x: 0
            y: MobileTheme.shadowDepth
            width: parent.width
            height: face.height
            radius: MobileTheme.radiusCard
            color: MobileTheme.accent
        }

        Rectangle {
            id: face
            width: parent.width
            height: parent.height - MobileTheme.shadowDepth
            radius: MobileTheme.radiusCard
            color: card.fill
            border.width: 1
            border.color: MobileTheme.accent
        }
    }
}
