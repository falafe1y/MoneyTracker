import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: settings
    spacing: 16
    Text { text: qsTr("Защита паролем"); color: "#031528"; font.pixelSize: 17; font.bold: true }
    Text {
        Layout.fillWidth: true
        text: appAccess.enabled ? qsTr("Пароль включён. Автоблокировка через 10 минут бездействия.")
                                : qsTr("Пароль необязателен. Можно использовать от 1 до 1024 символов.")
        color: "#687483"
        wrapMode: Text.WordWrap
    }
    Text {
        visible: !appAccess.storeAvailable
        Layout.fillWidth: true
        text: !appAccess.storeChecked ? qsTr("Проверяем системное хранилище…")
              : (appAccess.storeError || qsTr("Системное хранилище недоступно. Защита паролем недоступна."))
        color: "#687483"
        wrapMode: Text.WordWrap
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        StyledButton {
            text: appAccess.enabled ? qsTr("Изменить пароль") : qsTr("Установить пароль")
            enabled: appAccess.storeChecked && appAccess.storeAvailable && appAccess.ready && !appAccess.busy
            onClicked: { dialog.removing = false; dialog.open() }
        }
        StyledButton {
            visible: appAccess.enabled
            text: qsTr("Отключить пароль")
            enabled: appAccess.storeChecked && appAccess.storeAvailable && appAccess.ready && !appAccess.busy
            onClicked: { dialog.removing = true; dialog.open() }
        }
        StyledButton {
            visible: appAccess.enabled
            text: qsTr("Заблокировать")
            onClicked: appAccess.lock()
        }
    }
    Dialog {
        id: dialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        modal: true
        property bool removing: false
        title: removing ? qsTr("Отключить пароль") : appAccess.enabled ? qsTr("Изменить пароль") : qsTr("Установить пароль")
        padding: 24
        background: Rectangle { color: "#FFFFF8"; radius: 12; border.color: "#D8D7C7" }
        onOpened: { current.clear(); password.clear(); confirmation.clear(); current.visible ? current.forceActiveFocus() : password.forceActiveFocus() }
        onClosed: { current.clear(); password.clear(); confirmation.clear() }
        contentItem: ColumnLayout {
            spacing: 16
            StyledTextField {
                id: current
                visible: appAccess.enabled
                Layout.fillWidth: true
                placeholderText: qsTr("Текущий пароль или код восстановления")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
            StyledTextField {
                id: password
                visible: !dialog.removing
                Layout.fillWidth: true
                placeholderText: qsTr("Новый пароль: 1–1024 символа")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
            StyledTextField {
                id: confirmation
                visible: !dialog.removing
                Layout.fillWidth: true
                placeholderText: qsTr("Повторите пароль")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
            Text {
                visible: appAccess.error.length > 0
                Layout.fillWidth: true
                text: appAccess.error
                color: "#B94F48"
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                StyledButton { text: qsTr("Отмена"); onClicked: dialog.close() }
                Item { Layout.fillWidth: true }
                StyledButton {
                    text: dialog.removing ? qsTr("Отключить") : qsTr("Сохранить")
                    primary: true
                    dimWhenDisabled: false
                    enabled: appAccess.storeAvailable && !appAccess.busy && (!appAccess.enabled || current.text.length > 0)
                             && (dialog.removing || (password.text.length > 0 && password.text === confirmation.text))
                    onClicked: {
                        if (dialog.removing) appAccess.disablePassword(current.text)
                        else appAccess.changePassword(current.text, password.text, confirmation.text)
                        current.clear(); password.clear(); confirmation.clear()
                    }
                }
            }
        }
    }
}
