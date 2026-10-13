import QtQuick
import QtQuick.Controls
import QtTest
import "../../ui/computer/common"

Item {
    id: scene
    width: 640
    height: 480
    property int firstCreated: 0
    property int secondCreated: 0

    QtObject {
        id: input
        property var window: null
        property bool active: false
        signal middlePressed(point position)
        signal pointerMoved(point position)
        signal cancelled()
    }
    AutoScrollController { id: scroller; input: input }
    StyledComboBox {
        id: combo
        width: 180; height: 44
        model: Array.from({length: 50}, function(_, i) { return "Вариант " + i })
    }
    CachedPages {
        id: pages
        x: 0; y: 80; width: 640; height: 400
        currentKey: "first"
        pages: [{key: "first", component: first}, {key: "second", component: second}]
    }
    Component {
        id: first
        Item {
            objectName: "firstPage"
            property alias viewport: viewport
            property alias editor: editor
            property alias choice: choice
            property bool expanded: false
            Component.onCompleted: ++scene.firstCreated
            Flickable {
                id: viewport
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: 1600
                Column {
                    TextField { id: editor; text: "Начальный текст" }
                    ComboBox { id: choice; model: ["Один", "Два", "Три"] }
                    Rectangle { width: 640; height: 1500; color: "ivory" }
                }
            }
        }
    }
    Component {
        id: second
        Flickable {
            objectName: "secondPage"
            anchors.fill: parent
            contentWidth: width
            contentHeight: 1600
            Component.onCompleted: ++scene.secondCreated
        }
    }
    TestCase {
        name: "NavigationAndMiddleScroll"
        when: windowShown
        function init() {
            scroller.stop()
            combo.popup.close()
            pages.currentKey = "first"
            waitForRendering(scene)
        }
        function cleanup() { scroller.stop(); combo.popup.close() }
        function page(name) { return findChild(scene, name) }
        function startAt(item, x, y) {
            var point = item.mapToItem(input.window.contentItem, x, y)
            input.middlePressed(point)
        }
        function test_pageStateSurvivesRepeatedNavigation() {
            var first = page("firstPage")
            verify(first !== null)
            compare(scene.firstCreated, 1)
            compare(scene.secondCreated, 0)
            first.viewport.contentY = 370
            first.editor.text = "Несохранённый ввод"
            first.choice.currentIndex = 2
            first.expanded = true
            pages.currentKey = "second"
            waitForRendering(scene)
            var second = page("secondPage")
            verify(second !== null)
            second.contentY = 215
            verify(!first.visible)
            verify(!first.enabled)
            for (var i = 0; i < 3; ++i) {
                pages.currentKey = "first"
                waitForRendering(scene)
                compare(page("firstPage"), first)
                compare(first.viewport.contentY, 370)
                compare(first.editor.text, "Несохранённый ввод")
                compare(first.choice.currentIndex, 2)
                verify(first.expanded)
                pages.currentKey = "second"
                waitForRendering(scene)
                compare(second.contentY, 215)
            }
            compare(scene.firstCreated, 1)
            compare(scene.secondCreated, 1)
        }
        function test_scrollStillWorksAfterListCloses() {
            var first = page("firstPage")
            startAt(first.viewport, 350, 200)
            verify(scroller.active)
            compare(scroller.scrollTarget, first.viewport)
            scroller.stop()
            // Use real popup lifecycle: its overlay may stay visible after closing.
            for (var i = 0; i < 3; ++i) {
                combo.popup.open()
                tryCompare(combo.popup, "visible", true)
                combo.popup.close()
                tryCompare(combo.popup, "visible", false)
                startAt(first.viewport, 350, 200)
                verify(scroller.active)
                compare(scroller.scrollTarget, first.viewport)
                var before = first.viewport.contentY
                input.pointerMoved(first.viewport.mapToItem(input.window.contentItem, 350, 280))
                tryVerify(function() { return first.viewport.contentY > before })
                input.cancelled()
                verify(!scroller.active)
            }
        }
        function test_popupListTakesPriorityOverPage() {
            combo.popup.open()
            tryCompare(combo.popup, "visible", true)
            waitForRendering(scene)
            var list = combo.popup.contentItem
            verify(list.contentHeight > list.height)
            startAt(list, 50, 70)
            verify(scroller.active)
            compare(scroller.scrollTarget, list)
            var before = list.contentY
            input.pointerMoved(list.mapToItem(input.window.contentItem, 50, 130))
            tryVerify(function() { return list.contentY > before })
            combo.popup.close()
            tryCompare(combo.popup, "visible", false)
            // A hidden list must not keep scrolling.
            tryCompare(scroller, "active", false)
        }
    }
}
