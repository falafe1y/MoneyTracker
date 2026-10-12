import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    width: 560
    height: 440
    minimumWidth: 320
    minimumHeight: 340
    visible: !appAccess.locked && !databaseProtection.ready
    title: "Vexa"
    color: "#F5F3E4"
    BackupDialogs { id: recoveryDialogs; recoveryMode: true }
    font.family: "Commissioner"
    onClosing: function(event) {
        if (databaseProtection.busy) event.accepted = false
        else Qt.quit()
    }
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 64
        spacing: 16
        Text { text: "Vexa"; color: "#031528"; font.pixelSize: 28; font.bold: true }
        Text {
            Layout.fillWidth: true
            text: databaseProtection.busy ? qsTr("Подготовка базы…")
                : databaseProtection.restartRequired ? qsTr("Требуется перезапуск") : qsTr("Не удалось открыть базу")
            color: "#031528"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
        }
        BusyIndicator { visible: databaseProtection.busy; running: visible; Layout.alignment: Qt.AlignHCenter }
        ScrollView {
            visible: databaseProtection.error.length > 0
            Layout.fillWidth: true
            Layout.maximumHeight: 140
            implicitHeight: errorText.implicitHeight
            contentWidth: availableWidth
            TextArea {
                id: errorText
                text: databaseProtection.error
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: "#B94F48"
                font.pixelSize: 14
                background: null
            }
        }
        StyledButton {
            visible: !databaseProtection.busy && !databaseProtection.restartRequired && databaseProtection.error.length > 0
            text: qsTr("Восстановить из копии")
            onClicked: recoveryDialogs.openImport()
        }
        StyledButton {
            visible: !databaseProtection.busy
            text: databaseProtection.restartRequired ? qsTr("Перезапустить") : qsTr("Повторить")
            onClicked: databaseProtection.restartRequired ? databaseProtection.restart() : databaseProtection.prepare()
        }
    }
}
