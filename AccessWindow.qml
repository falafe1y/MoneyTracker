import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 520
    height: 700
    minimumWidth: 320
    minimumHeight: 540
    visible: appAccess.locked
    title: "Vexa"
    color: "#F5F3E4"
    font.family: "Commissioner"
    font.pixelSize: 14
    onClosing: Qt.quit()
    property bool recovering: false
    onVisibleChanged: if (visible) { proof.clear(); password.clear(); confirmation.clear(); proof.forceActiveFocus() }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: Math.min(400, window.width - 48)
            x: (window.width - width) / 2
            y: 40
            spacing: 16
            Text { text: "Vexa"; color: "#031528"; font.pixelSize: 32; font.bold: true }
            Text {
                Layout.fillWidth: true
                text: appAccess.recoveryCode.length > 0 ? qsTr("Сохраните код восстановления")
                      : window.recovering ? qsTr("Восстановление доступа") : qsTr("Вход в приложение")
                font.pixelSize: 20
                font.bold: true
                color: "#031528"
                wrapMode: Text.WordWrap
            }
            Text {
                visible: appAccess.recoveryCode.length > 0
                Layout.fillWidth: true
                text: qsTr("Код позволит войти и установить новый пароль, если вы забудете текущий. При смене пароля выдаётся новый код.")
                color: "#687483"
                wrapMode: Text.WordWrap
            }
            TextArea {
                visible: appAccess.recoveryCode.length > 0
                Layout.fillWidth: true
                text: appAccess.recoveryCode
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                color: "#031528"
                font.pixelSize: 19
                background: Rectangle { color: "#FFFFF8"; radius: 12; border.color: "#D8D7C7" }
            }
            StyledCheckBox {
                id: savedCode
                Layout.fillWidth: true
                visible: appAccess.recoveryCode.length > 0
                text: qsTr("Я сохранил код восстановления")
            }
            StyledButton {
                visible: appAccess.recoveryCode.length > 0
                Layout.fillWidth: true
                text: qsTr("Продолжить")
                primary: true
                    dimWhenDisabled: false
                enabled: savedCode.checked
                onClicked: { savedCode.checked = false; window.recovering = false; appAccess.acknowledgeRecovery(); proof.forceActiveFocus() }
            }
            ColumnLayout {
                visible: appAccess.ready && appAccess.recoveryCode.length === 0
                Layout.fillWidth: true
                spacing: 16
                StyledTextField {
                    id: proof
                    Layout.fillWidth: true
                    placeholderText: window.recovering ? qsTr("Код восстановления") : qsTr("Пароль")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: !appAccess.busy
                    onAccepted: if (!window.recovering && !appAccess.busy && !appAccess.retryAfter) { appAccess.unlock(text); clear() }
                }
                StyledTextField {
                    id: password
                    visible: window.recovering
                    Layout.fillWidth: true
                    placeholderText: qsTr("Новый пароль: 1–1024 символа")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: !appAccess.busy
                }
                StyledTextField {
                    id: confirmation
                    visible: window.recovering
                    Layout.fillWidth: true
                    placeholderText: qsTr("Повторите новый пароль")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: !appAccess.busy
                }
                StyledButton {
                    Layout.fillWidth: true
                    text: window.recovering ? qsTr("Установить пароль") : qsTr("Войти")
                    primary: true
                    dimWhenDisabled: false
                    enabled: !appAccess.busy && !appAccess.retryAfter && proof.text.length > 0
                             && (!window.recovering || (password.text.length > 0 && password.text === confirmation.text))
                    onClicked: {
                        if (window.recovering) appAccess.changePassword(proof.text, password.text, confirmation.text)
                        else appAccess.unlock(proof.text)
                        proof.clear(); password.clear(); confirmation.clear()
                    }
                }
                StyledButton {
                    Layout.fillWidth: true
                    flat: true
                    text: window.recovering ? qsTr("Вернуться ко входу") : qsTr("Забыли пароль?")
                    enabled: !appAccess.busy
                    onClicked: { window.recovering = !window.recovering; proof.clear(); password.clear(); confirmation.clear() }
                }
            }
            Text {
                visible: appAccess.error.length > 0
                Layout.fillWidth: true
                text: appAccess.error
                color: "#B94F48"
                wrapMode: Text.WordWrap
            }
            Text {
                visible: appAccess.retryAfter > 0
                text: qsTr("Повторить через %1 с").arg(appAccess.retryAfter)
                color: "#687483"
            }
            BusyIndicator { running: appAccess.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
            StyledButton {
                visible: !appAccess.busy && appAccess.recoveryCode.length === 0 && (!appAccess.ready || appAccess.error.length > 0)
                Layout.fillWidth: true
                text: qsTr("Повторить")
                onClicked: appAccess.retry()
            }
            Item { Layout.preferredHeight: 40 }
        }
    }
}
