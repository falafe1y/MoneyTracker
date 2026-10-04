import QtQuick

// Flat "hard" shadow for the outlined card style: a solid, unblurred copy of the
// parent's silhouette shifted straight down. Replaces the blurred SurfaceShadow.
//
// The item also paints a copy of the parent's face (fill + outline) ABOVE the
// shadow. Stacking of a negative-z child relative to the parent's own fill is not
// reliable, so the shadow must never depend on it: whichever way the parent's fill
// is ordered, the face copy always sits on top of the shadow.
Item {
    id: shadowRoot
    property int depth: 6
    property color shadowColor: "#031528"
    readonly property var card: parent

    anchors.fill: parent
    z: -1

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: shadowRoot.depth
        anchors.bottomMargin: -shadowRoot.depth
        radius: shadowRoot.card.radius
        color: shadowRoot.shadowColor
    }

    Rectangle {
        anchors.fill: parent
        radius: shadowRoot.card.radius
        color: shadowRoot.card.color
        border.width: shadowRoot.card.border.width
        border.color: shadowRoot.card.border.color
    }
}
