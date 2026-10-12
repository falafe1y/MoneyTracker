import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 16
    visible: databaseProtection.supported
    Text { text: qsTr("Шифрование базы"); color: "#031528"; font.pixelSize: 17; font.bold: true }
    Text {
        Layout.fillWidth: true
        text: databaseProtection.encrypted
            ? qsTr("Локальная база зашифрована. Ключ хранится в учётной записи Windows.")
            : qsTr("Шифрование защищает файл базы. Оно независимо от пароля входа в Vexa.")
        color: "#687483"
        wrapMode: Text.WordWrap
    }
    StyledButton {
        text: databaseProtection.encrypted ? qsTr("Отключить шифрование") : qsTr("Включить шифрование")
        enabled: databaseProtection.ready && !databaseProtection.busy
        onClicked: confirmation.open()
    }
    Dialog {
        id: confirmation
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 32)
        padding: 24
        modal: true
        title: databaseProtection.encrypted ? qsTr("Отключить шифрование базы?") : qsTr("Зашифровать базу?")
        background: Rectangle { color: "#FFFFF8"; radius: 12; border.color: "#D8D7C7" }
        contentItem: ColumnLayout {
            spacing: 16
            Text {
                Layout.fillWidth: true
                text: databaseProtection.encrypted
                    ? qsTr("После отключения файл базы будет доступен без ключа. Ключ сохранится для чтения ранее зашифрованных копий. Vexa перезапустится.")
                    : qsTr("База будет зашифрована ключом из системного хранилища Windows. Для её открытия потребуется этот ключ. Старые копии останутся без изменений. Vexa перезапустится.")
                color: "#031528"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                StyledButton { text: qsTr("Отмена"); onClicked: confirmation.close() }
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: qsTr("Продолжить")
                    primary: true
                    onClicked: { confirmation.close(); databaseProtection.requestChange(!databaseProtection.encrypted) }
                }
            }
        }
    }
}
