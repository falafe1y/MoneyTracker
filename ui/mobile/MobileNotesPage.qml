import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property bool loaded: false

    Component.onCompleted: {
        editor.text = financeController.notesText;
        page.loaded = true;
    }
    Component.onDestruction: financeController.saveNotes()

    Connections {
        target: financeController

        function onNotesChanged() {
            if (!page.loaded || editor.text === financeController.notesText)
                return;
            page.loaded = false;
            editor.text = financeController.notesText;
            page.loaded = true;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: !financeController.notesAvailable
                    ? qsTr("Заметки недоступны")
                    : financeController.notesDirty ? qsTr("Есть несохранённые изменения") : qsTr("Все изменения сохранены")
                color: MobileTheme.muted
                font.pixelSize: MobileTheme.sizeCaption
                wrapMode: Text.Wrap
            }

            MobileButton {
                variant: "primary"
                implicitHeight: 40
                text: qsTr("Сохранить")
                enabled: financeController.notesAvailable && financeController.notesDirty
                onClicked: financeController.saveNotes()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: financeController.notesError.length > 0
            text: financeController.notesError
            wrapMode: Text.Wrap
            color: MobileTheme.red
            font.pixelSize: MobileTheme.sizeCaption
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: MobileTheme.radiusCard
            color: MobileTheme.panel
            border.width: 1
            border.color: MobileTheme.line
            clip: true

            ScrollView {
                id: notesScroll

                anchors.fill: parent
                anchors.margins: 1
                clip: true

                TextArea {
                    id: editor

                    width: notesScroll.availableWidth
                    readOnly: !financeController.notesAvailable
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    padding: 12
                    font.pixelSize: MobileTheme.sizeBody
                    color: MobileTheme.accent
                    placeholderText: qsTr("Запишите мысли, планы и напоминания")
                    placeholderTextColor: MobileTheme.muted
                    background: Rectangle {
                        color: "transparent"
                    }

                    onTextChanged: {
                        if (page.loaded)
                            financeController.setNotesText(text);
                    }
                    onActiveFocusChanged: {
                        if (!activeFocus && page.loaded)
                            financeController.saveNotes();
                    }
                }
            }
        }
    }
}
