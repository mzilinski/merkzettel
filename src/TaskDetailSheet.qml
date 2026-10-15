import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.OverlaySheet {
    id: sheet

    // Popups only close on Escape while they hold focus; without this the
    // sheet ignored Escape whenever focus stayed in the task list.
    focus: true

    property var task: app.detailTask
    property bool hasTask: task && task.taskId !== undefined

    signal datePickerRequested(var initialDate)
    signal reminderDatePickerRequested(var initialDate)
    signal reminderTimePickerRequested(var initialDate)
    signal customRecurrenceRequested(string taskId, var info)

    // Weekday + the locale's short date and time, e.g. "Do. 15.10.26, 09:00"
    // or "Thu 10/15/26, 9:00 AM".
    function formatDateTime(d) {
        const loc = Qt.locale();
        return loc.toString(d, "ddd " + loc.dateFormat(Locale.ShortFormat) + ", "
                               + loc.timeFormat(Locale.ShortFormat));
    }

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
                Kirigami.FormData.label: i18n("My Day:")
                QQC2.ToolButton {
                    icon.name: "weather-clear-symbolic"
                    checkable: true
                    checked: hasTask && task.inMyDay
                    text: hasTask && task.inMyDay ? i18n("In My Day") : i18n("Add to My Day")
                    onClicked: if (hasTask) app.setInMyDay([task.taskId], !task.inMyDay)
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
                Kirigami.FormData.label: i18n("Repeat:")
                spacing: Kirigami.Units.smallSpacing
                QQC2.ComboBox {
                    id: recurrenceCombo
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "value"
                    readonly property var info: hasTask && task.recurrence ? task.recurrence : ({})
                    model: [
                        { value: "",         label: i18n("Never") },
                        { value: "daily",    label: i18n("Daily") },
                        { value: "weekdays", label: i18n("Weekdays") },
                        { value: "weekly",   label: i18n("Weekly") },
                        { value: "monthly",  label: i18n("Monthly") },
                        { value: "yearly",   label: i18n("Yearly") },
                        // Shows the current custom rule; picking it opens the editor.
                        { value: "custom",   label: info.preset === "custom"
                                                    ? info.summary : i18n("Custom ...") },
                    ]
                    currentIndex: Math.max(0, indexOfValue(info.preset || ""))
                    onActivated: {
                        if (!hasTask) return;
                        if (currentValue === "custom") {
                            sheet.customRecurrenceRequested(task.taskId, info);
                            // Snap back until the dialog actually saves something.
                            currentIndex = Qt.binding(() => Math.max(0, indexOfValue(info.preset || "")));
                            return;
                        }
                        if (currentValue === (info.preset || "")) return;
                        app.setTaskRecurrencePattern(task.taskId, currentValue);
                    }
                }
                QQC2.Button {
                    icon.name: "document-edit"
                    visible: hasTask && recurrenceCombo.info.preset === "custom"
                    QQC2.ToolTip.text: i18n("Edit repeat ...")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: sheet.customRecurrenceRequested(task.taskId, recurrenceCombo.info)
                }
            }

            ColumnLayout {
                Kirigami.FormData.label: i18n("Categories:")
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    visible: hasTask && (task.categories || []).length > 0
                    Repeater {
                        model: hasTask ? (task.categories || []) : []
                        Kirigami.Chip {
                            required property string modelData
                            text: modelData
                            closable: true
                            checkable: false
                            onRemoved: app.setTaskCategories(task.taskId,
                                task.categories.filter(c => c !== modelData))
                        }
                    }
                }

                // Editable combo: type a new name or pick one already used on
                // another task. Graph has no category picker without the
                // Outlook master-category scope, so suggestions come from the cache.
                QQC2.ComboBox {
                    id: categoryInput
                    Layout.fillWidth: true
                    editable: true
                    model: []
                    function refreshSuggestions() {
                        const used = hasTask ? (task.categories || []) : [];
                        model = app.knownCategories().filter(c => used.indexOf(c) < 0);
                        currentIndex = -1;
                        editText = "";
                    }
                    function addCategory(name) {
                        const n = (name || "").trim();
                        if (!hasTask || n.length === 0) return;
                        app.setTaskCategories(task.taskId, (task.categories || []).concat([n]));
                        refreshSuggestions();
                    }
                    Component.onCompleted: refreshSuggestions()
                    Connections {
                        target: sheet
                        function onTaskChanged() { categoryInput.refreshSuggestions(); }
                    }
                    onAccepted: addCategory(editText)
                    onActivated: addCategory(currentText)
                    QQC2.Label {
                        anchors.fill: parent.contentItem
                        leftPadding: Kirigami.Units.smallSpacing
                        verticalAlignment: Text.AlignVCenter
                        visible: categoryInput.editText.length === 0 && !categoryInput.activeFocus
                        text: i18n("Add category ...")
                        color: Kirigami.Theme.disabledTextColor
                    }
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
                          ? sheet.formatDateTime(task.reminderDate)
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

        Kirigami.Heading {
            level: 4
            text: i18n("Links")
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: hasTask && task.linkedResources ? task.linkedResources : []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        source: "internet-services"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: `<a href="${modelData.webUrl}">${modelData.displayName || modelData.webUrl}</a>`
                        textFormat: Text.RichText
                        elide: Text.ElideRight
                        onLinkActivated: link => app.openLinkedResource(link)
                        QQC2.ToolTip.text: modelData.webUrl
                        QQC2.ToolTip.visible: hovered && (modelData.displayName || "").length > 0
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        HoverHandler { id: hoverArea }
                        property bool hovered: hoverArea.hovered
                    }
                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        QQC2.ToolTip.text: i18n("Remove link")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                        onClicked: if (hasTask) app.removeLinkedResource(task.taskId, modelData.id)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                QQC2.TextField {
                    id: newLinkUrl
                    Layout.fillWidth: true
                    placeholderText: i18n("https://... or file path")
                    onAccepted: addCurrent()
                    function addCurrent() {
                        if (text.trim().length > 0 && hasTask) {
                            app.addLinkedResource(task.taskId, text.trim(), newLinkName.text.trim());
                            text = "";
                            newLinkName.text = "";
                        }
                    }
                }
                QQC2.TextField {
                    id: newLinkName
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                    placeholderText: i18n("Label (optional)")
                    onAccepted: newLinkUrl.addCurrent()
                }
                QQC2.Button {
                    icon.name: "list-add"
                    enabled: newLinkUrl.text.trim().length > 0
                    onClicked: newLinkUrl.addCurrent()
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        Kirigami.Heading {
            level: 4
            text: i18n("Files")
        }

        // Drop zone around the whole Files section; highlights while files
        // are dragged over it from a file manager.
        Item {
            Layout.fillWidth: true
            implicitHeight: filesColumn.implicitHeight

            ColumnLayout {
                id: filesColumn
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: hasTask && task.attachments ? task.attachments : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Icon {
                            source: modelData.iconName || "mail-attachment"
                            fallback: "mail-attachment"
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: modelData.name
                            elide: Text.ElideMiddle
                        }
                        QQC2.Label {
                            text: modelData.sizeLabel
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                        }
                        QQC2.ToolButton {
                            icon.name: "document-open"
                            QQC2.ToolTip.text: i18n("Open")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: app.openAttachment(task.taskId, modelData.id)
                        }
                        QQC2.ToolButton {
                            icon.name: "document-save-as"
                            QQC2.ToolTip.text: i18n("Save as ...")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                saveDialog.attachmentId = modelData.id;
                                saveDialog.selectedFile = StandardPaths.writableLocation(
                                    StandardPaths.DownloadLocation) + "/" + modelData.name;
                                saveDialog.open();
                            }
                        }
                        QQC2.ToolButton {
                            icon.name: "edit-delete"
                            QQC2.ToolTip.text: i18n("Delete file")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: {
                                deleteFileDialog.attachmentId = modelData.id;
                                deleteFileDialog.fileName = modelData.name;
                                deleteFileDialog.open();
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    QQC2.Button {
                        text: i18n("Add file ...")
                        icon.name: "mail-attachment"
                        onClicked: openDialog.open()
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: i18n("or drop files here (max. 25 MB each)")
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        elide: Text.ElideRight
                    }
                }
            }

            DropArea {
                id: fileDrop
                anchors.fill: parent
                onEntered: drag => drag.accepted = drag.hasUrls
                onDropped: drop => {
                    if (hasTask && drop.hasUrls) {
                        app.addAttachments(task.taskId, drop.urls);
                        drop.accept();
                    }
                }
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: -Kirigami.Units.smallSpacing
                visible: fileDrop.containsDrag
                radius: Kirigami.Units.cornerRadius
                color: Qt.alpha(Kirigami.Theme.highlightColor, 0.12)
                border.color: Kirigami.Theme.highlightColor
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

        Kirigami.Separator { Layout.fillWidth: true }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            component MetaLabel: QQC2.Label {
                Layout.fillWidth: true
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
                visible: text.length > 0
            }

            MetaLabel {
                text: hasTask && (task.listName || "").length > 0
                      ? i18n("List: %1", task.listName) : ""
            }
            MetaLabel {
                text: hasTask && task.createdDate && !isNaN(task.createdDate.getTime())
                      ? i18n("Created: %1",
                             sheet.formatDateTime(task.createdDate))
                      : ""
            }
            MetaLabel {
                text: hasTask && task.lastModified && !isNaN(task.lastModified.getTime())
                      ? i18n("Last modified: %1",
                             sheet.formatDateTime(task.lastModified))
                      : ""
            }
        }
    }

    footer: RowLayout {
        // Safe default on the left: habitual bottom-left clicks must never
        // complete or delete anything, only dismiss the sheet.
        QQC2.Button {
            text: i18n("OK")
            icon.name: "dialog-ok"
            highlighted: true
            onClicked: sheet.close()
        }
        Item { Layout.fillWidth: true }
        QQC2.Button {
            text: hasTask && task.completed ? i18n("Reopen") : i18n("Mark done")
            icon.name: hasTask && task.completed ? "edit-undo" : "checkmark"
            icon.color: hasTask && task.completed ? Kirigami.Theme.textColor
                                                  : Kirigami.Theme.positiveTextColor
            palette.buttonText: hasTask && task.completed ? Kirigami.Theme.textColor
                                                          : Kirigami.Theme.positiveTextColor
            onClicked: {
                if (!hasTask) return;
                if (task.completed) app.uncompleteTask(task.taskId);
                else app.completeTask(task.taskId);
                sheet.close();
            }
        }
        QQC2.Button {
            text: i18n("Delete")
            icon.name: "edit-delete"
            icon.color: Kirigami.Theme.negativeTextColor
            palette.buttonText: Kirigami.Theme.negativeTextColor
            onClicked: {
                if (!hasTask) return;
                app.deleteTask(task.taskId);
                sheet.close();
            }
        }
    }

    FileDialog {
        id: openDialog
        title: i18n("Attach files")
        fileMode: FileDialog.OpenFiles
        onAccepted: if (hasTask) app.addAttachments(task.taskId, selectedFiles)
    }

    FileDialog {
        id: saveDialog
        property string attachmentId: ""
        title: i18n("Save file")
        fileMode: FileDialog.SaveFile
        onAccepted: if (hasTask) app.saveAttachment(task.taskId, attachmentId, selectedFile)
    }

    Kirigami.PromptDialog {
        id: deleteFileDialog
        property string attachmentId: ""
        property string fileName: ""
        parent: QQC2.Overlay.overlay
        title: i18n("Delete file?")
        subtitle: i18n("\"%1\" will be removed from the task. This cannot be undone.", fileName)
        standardButtons: Kirigami.PromptDialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: deleteFileDialog.close()
            },
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    if (hasTask) app.deleteAttachment(task.taskId, deleteFileDialog.attachmentId);
                    deleteFileDialog.close();
                }
            }
        ]
    }
}
