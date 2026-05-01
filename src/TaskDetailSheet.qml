import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.OverlaySheet {
    id: sheet

    property var task: app.detailTask
    property bool hasTask: task && task.taskId !== undefined

    signal datePickerRequested(var initialDate)
    signal reminderDatePickerRequested(var initialDate)
    signal reminderTimePickerRequested(var initialDate)

    function openDatePicker(initial) {
        datePickerRequested(initial);
    }

    onHasTaskChanged: {
        if (hasTask) open();
        else close();
    }

    onClosed: app.closeTaskDetails()

    title: hasTask ? task.title : ""

    ColumnLayout {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 30
        spacing: Kirigami.Units.largeSpacing

        Kirigami.FormLayout {
            Layout.fillWidth: true

            QQC2.TextField {
                id: titleField
                Kirigami.FormData.label: i18n("Title:")
                Layout.fillWidth: true
                text: hasTask ? task.title : ""
                onEditingFinished: {
                    if (hasTask && text.trim().length > 0 && text !== task.title) {
                        app.setTaskTitle(task.taskId, text.trim());
                    }
                }
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Importance:")
                QQC2.ToolButton {
                    icon.name: hasTask && task.important ? "starred-symbolic" : "non-starred-symbolic"
                    text: hasTask && task.important ? i18n("Important") : i18n("Normal")
                    onClicked: if (hasTask) app.toggleImportance(task.taskId)
                }
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Due:")
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    Layout.fillWidth: true
                    text: hasTask && task.dueDate && !isNaN(task.dueDate.getTime())
                          ? Qt.formatDate(task.dueDate, Qt.DefaultLocaleShortDate)
                          : i18n("no date")
                    color: text === i18n("no date") ? Kirigami.Theme.disabledTextColor
                                                    : Kirigami.Theme.textColor
                }
                QQC2.Button {
                    text: i18n("Today")
                    onClicked: if (hasTask) app.setTaskDueDate(task.taskId,
                        new Date(new Date().setHours(9, 0, 0, 0)))
                }
                QQC2.Button {
                    text: i18n("Tomorrow")
                    onClicked: if (hasTask) {
                        const d = new Date();
                        d.setDate(d.getDate() + 1);
                        d.setHours(9, 0, 0, 0);
                        app.setTaskDueDate(task.taskId, d);
                    }
                }
                QQC2.Button {
                    icon.name: "view-calendar"
                    QQC2.ToolTip.text: i18n("Pick date ...")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        if (!hasTask) return;
                        const initial = task.dueDate && !isNaN(task.dueDate.getTime())
                            ? task.dueDate : new Date();
                        sheet.openDatePicker(initial);
                    }
                }
                QQC2.Button {
                    icon.name: "edit-clear"
                    enabled: hasTask && task.dueDate && !isNaN(task.dueDate.getTime())
                    onClicked: if (hasTask) app.clearTaskDueDate(task.taskId)
                }
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Reminder:")
                spacing: Kirigami.Units.smallSpacing
                QQC2.Switch {
                    id: reminderSwitch
                    checked: hasTask && task.hasReminder
                    onToggled: {
                        if (!hasTask) return;
                        if (checked) {
                            const base = (task.dueDate && !isNaN(task.dueDate.getTime()))
                                ? new Date(task.dueDate)
                                : new Date(new Date().setDate(new Date().getDate() + 1));
                            base.setHours(9, 0, 0, 0);
                            app.setTaskReminder(task.taskId, base);
                        } else {
                            app.clearTaskReminder(task.taskId);
                        }
                    }
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    enabled: reminderSwitch.checked
                    text: hasTask && task.hasReminder && task.reminderDate
                          && !isNaN(task.reminderDate.getTime())
                          ? Qt.formatDateTime(task.reminderDate, "ddd d. MMM yyyy, HH:mm")
                          : i18n("off")
                    color: reminderSwitch.checked ? Kirigami.Theme.textColor
                                                  : Kirigami.Theme.disabledTextColor
                }
                QQC2.Button {
                    icon.name: "view-calendar"
                    enabled: reminderSwitch.checked
                    QQC2.ToolTip.text: i18n("Change date ...")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        if (!hasTask) return;
                        const initial = (task.reminderDate && !isNaN(task.reminderDate.getTime()))
                            ? task.reminderDate : new Date();
                        sheet.reminderDatePickerRequested(initial);
                    }
                }
                QQC2.Button {
                    icon.name: "clock"
                    enabled: reminderSwitch.checked
                    QQC2.ToolTip.text: i18n("Change time ...")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        if (!hasTask) return;
                        const initial = (task.reminderDate && !isNaN(task.reminderDate.getTime()))
                            ? task.reminderDate : new Date();
                        sheet.reminderTimePickerRequested(initial);
                    }
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        Kirigami.Heading {
            level: 4
            text: i18n("Subtasks")
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: hasTask && task.checklistItems ? task.checklistItems : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    QQC2.CheckBox {
                        checked: modelData.isChecked
                        onToggled: if (hasTask) app.toggleChecklistItem(task.taskId, modelData.id)
                    }
                    QQC2.TextField {
                        Layout.fillWidth: true
                        text: modelData.displayName
                        font.strikeout: modelData.isChecked
                        color: modelData.isChecked ? Kirigami.Theme.disabledTextColor
                                                   : Kirigami.Theme.textColor
                        onEditingFinished: {
                            const trimmed = text.trim();
                            if (hasTask && trimmed.length > 0 && trimmed !== modelData.displayName) {
                                app.renameChecklistItem(task.taskId, modelData.id, trimmed);
                            }
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        QQC2.ToolTip.text: i18n("Delete subtask")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: if (hasTask) app.deleteChecklistItem(task.taskId, modelData.id)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                QQC2.TextField {
                    id: newItemField
                    Layout.fillWidth: true
                    placeholderText: i18n("Add subtask ...")
                    // Graph caps checklist items at 20 per task; disable the
                    // input rather than letting the request fail.
                    enabled: hasTask && (task.totalChecklistCount || 0) < 20
                    onAccepted: addCurrent()
                    function addCurrent() {
                        if (text.trim().length > 0 && hasTask) {
                            app.addChecklistItem(task.taskId, text.trim());
                            text = "";
                        }
                    }
                }
                QQC2.Button {
                    icon.name: "list-add"
                    enabled: newItemField.enabled && newItemField.text.trim().length > 0
                    onClicked: newItemField.addCurrent()
                }
            }

            QQC2.Label {
                visible: hasTask && (task.totalChecklistCount || 0) >= 20
                text: i18n("Maximum of 20 subtasks reached.")
                color: Kirigami.Theme.disabledTextColor
                font.italic: true
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        QQC2.TextArea {
            id: bodyArea
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8
            placeholderText: i18n("Notes ...")
            wrapMode: TextEdit.Wrap
            text: hasTask ? (task.body || "") : ""
            onEditingFinished: {
                if (hasTask && text !== (task.body || "")) {
                    app.setTaskBody(task.taskId, text);
                }
            }
        }
    }

    footer: RowLayout {
        QQC2.Button {
            text: hasTask && task.completed ? i18n("Reopen") : i18n("Mark done")
            icon.name: hasTask && task.completed ? "edit-undo" : "checkmark"
            onClicked: {
                if (!hasTask) return;
                if (task.completed) app.uncompleteTask(task.taskId);
                else app.completeTask(task.taskId);
                sheet.close();
            }
        }
        Item { Layout.fillWidth: true }
        QQC2.Button {
            text: i18n("Delete")
            icon.name: "edit-delete"
            onClicked: {
                if (!hasTask) return;
                app.deleteTask(task.taskId);
                sheet.close();
            }
        }
    }
}
