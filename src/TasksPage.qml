import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: ""

    // Exclusive radio-style sort entries; the model persists the choice per
    // list. Removing the sort again goes through the indicator below the
    // search field, like in MS To Do.
    component SortAction: Kirigami.Action {
        required property string mode
        checkable: true
        checked: app.tasksModel.sortMode === mode
        onTriggered: app.tasksModel.sortMode = mode
    }

    // Entry points for the window-level shortcuts and the global quick-add.
    // (Ctrl+F needs no handler: Kirigami.SearchField's focusSequence has it.)
    function focusAddField() {
        addField.forceActiveFocus();
    }

    // Task the keyboard shortcuts act on: the one open in the detail sheet,
    // otherwise the keyboard-selected row.
    function shortcutTask() {
        if (app.detailTask && app.detailTask.taskId !== undefined) return app.detailTask;
        if (tasksView.activeFocus && tasksView.currentIndex >= 0)
            return app.tasksModel.taskAt(tasksView.currentIndex);
        return null;
    }

    // Multi-selection, keyed by task id so it survives model resets.
    // Reassigned (not mutated) on change so bindings re-evaluate.
    property var selectedIds: []
    property int selectionAnchor: -1

    function isSelected(taskId) {
        return selectedIds.indexOf(taskId) >= 0;
    }

    function toggleSelected(row) {
        const id = app.tasksModel.taskAt(row).taskId;
        selectedIds = isSelected(id) ? selectedIds.filter(x => x !== id)
                                     : selectedIds.concat([id]);
        selectionAnchor = row;
        tasksView.currentIndex = row;
    }

    function extendSelection(row) {
        const from = selectionAnchor >= 0 ? selectionAnchor : row;
        const ids = selectedIds.slice();
        for (let i = Math.min(from, row); i <= Math.max(from, row); ++i) {
            const id = app.tasksModel.taskAt(i).taskId;
            if (ids.indexOf(id) < 0) ids.push(id);
        }
        selectedIds = ids;
        tasksView.currentIndex = row;
    }

    function selectAll() {
        const ids = [];
        for (let i = 0; i < tasksView.count; ++i) ids.push(app.tasksModel.taskAt(i).taskId);
        selectedIds = ids;
    }

    function clearSelection() {
        selectedIds = [];
        selectionAnchor = -1;
    }

    // Tasks an action on taskId applies to: the whole selection when taskId
    // is part of it, otherwise just that task.
    function targetsFor(taskId) {
        return isSelected(taskId) ? selectedIds.slice() : [taskId];
    }

    // Ids of the keyboard target(s): selection, else the current row.
    function keyboardTargets() {
        if (selectedIds.length > 0) return selectedIds.slice();
        if (tasksView.currentIndex >= 0) return [app.tasksModel.taskAt(tasksView.currentIndex).taskId];
        return [];
    }

    Connections {
        target: app
        // Selections don't carry across lists.
        function onCurrentListIdChanged() { page.clearSelection(); }
    }

    function focusTaskList() {
        if (tasksView.count === 0) return;
        if (tasksView.currentIndex < 0) tasksView.currentIndex = 0;
        tasksView.forceActiveFocus();
    }

    Shortcut {
        sequences: [StandardKey.New]
        onActivated: page.focusAddField()
    }
    // MS To Do: Ctrl+T adds to My Day (here it toggles, like the menu).
    Shortcut {
        sequence: "Ctrl+T"
        onActivated: {
            let ids = page.selectedIds.slice();
            if (ids.length === 0) {
                const t = page.shortcutTask();
                if (t) ids = [t.taskId];
            }
            app.toggleMyDay(ids);
        }
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: {
            if (page.selectedIds.length > 0) {
                app.completeTasks(page.selectedIds);
                page.clearSelection();
                return;
            }
            const t = page.shortcutTask();
            if (!t) return;
            if (t.completed) app.uncompleteTask(t.taskId);
            else app.completeTask(t.taskId);
        }
    }

    actions: [
        Kirigami.Action {
            text: i18n("Sort")
            icon.name: "view-sort"
            displayHint: Kirigami.DisplayHint.IconOnly
            SortAction { mode: "importance"; text: i18n("Importance") }
            SortAction { mode: "dueDate"; text: i18n("Due date") }
            SortAction { mode: "title"; text: i18n("Alphabetically") }
            SortAction { mode: "created"; text: i18n("Creation date") }
        },
        Kirigami.Action {
            text: i18n("Show completed tasks")
            displayHint: Kirigami.DisplayHint.AlwaysHide
            checkable: true
            // Irrelevant in the Completed smart-list, which always shows them.
            enabled: app.currentListId !== "__completed__"
            checked: app.tasksModel.showCompleted
            onTriggered: app.tasksModel.showCompleted = checked
        },
        Kirigami.Action {
            text: i18n("Sync")
            icon.name: "view-refresh"
            displayHint: Kirigami.DisplayHint.IconOnly
            onTriggered: app.refresh()
        }
    ]

    header: QQC2.ToolBar {
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            // My Day banner: today's date and the reminder that, unlike in
            // MS To Do, this list lives only on this device.
            RowLayout {
                Layout.fillWidth: true
                visible: app.currentListId === "__myday__"
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Icon {
                    source: "weather-clear-symbolic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                }
                Kirigami.Heading {
                    level: 3
                    text: Qt.locale().toString(new Date(), "dddd, d. MMMM")
                }
                Item { Layout.fillWidth: true }
                QQC2.Label {
                    text: i18n("Only on this device")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    QQC2.ToolTip.text: i18n("Microsoft syncs My Day through an internal API that other apps cannot use, so it is stored locally and starts empty every day.")
                    QQC2.ToolTip.visible: myDayHint.hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    HoverHandler { id: myDayHint }
                }
            }

            // Replaces the add row while tasks are selected; same height so
            // the list below doesn't jump between Ctrl+clicks.
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: addRow.implicitHeight
                visible: page.selectedIds.length > 0
                spacing: 0
                QQC2.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    text: i18np("%1 selected", "%1 selected", page.selectedIds.length)
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                QQC2.ToolButton {
                    icon.name: "checkmark"
                    text: i18n("Complete")
                    onClicked: { app.completeTasks(page.selectedIds); page.clearSelection(); }
                }
                QQC2.ToolButton {
                    icon.name: "weather-clear-symbolic"
                    text: i18n("Add to My Day")
                    display: QQC2.AbstractButton.IconOnly
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: app.setInMyDay(page.selectedIds, true)
                }
                QQC2.ToolButton {
                    icon.name: "starred-symbolic"
                    text: i18n("Important")
                    display: QQC2.AbstractButton.IconOnly
                    QQC2.ToolTip.text: i18n("Mark as important")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: app.setTasksImportant(page.selectedIds, true)
                }
                QQC2.ToolButton {
                    id: moveButton
                    icon.name: "go-jump"
                    text: i18n("Move to")
                    display: QQC2.AbstractButton.IconOnly
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: selectionMoveMenu.popup(moveButton, 0, moveButton.height)
                    MoveToListMenu {
                        id: selectionMoveMenu
                        excludedListId: app.currentListVirtual ? "" : app.currentListId
                        onListChosen: listId => {
                            app.moveTasks(page.selectedIds, listId);
                            page.clearSelection();
                        }
                    }
                }
                QQC2.ToolButton {
                    icon.name: "edit-delete"
                    text: i18n("Delete")
                    display: QQC2.AbstractButton.IconOnly
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: { app.deleteTasks(page.selectedIds); page.clearSelection(); }
                }
                QQC2.ToolButton {
                    icon.name: "dialog-close"
                    text: i18n("Clear selection")
                    display: QQC2.AbstractButton.IconOnly
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: page.clearSelection()
                }
            }
            RowLayout {
                id: addRow
                Layout.fillWidth: true
                visible: page.selectedIds.length === 0
                spacing: Kirigami.Units.smallSpacing
                QQC2.TextField {
                    id: addField
                    Layout.fillWidth: true
                    placeholderText: i18n("New task ... (\"tomorrow\", \"mo\", \"25.5.\" at end for due date)")
                    onAccepted: addCurrent()
                    Keys.onDownPressed: page.focusTaskList()
                    function addCurrent() {
                        if (text.trim().length > 0) {
                            app.addTask(text.trim());
                            text = "";
                        }
                    }
                }
                QQC2.Button {
                    icon.name: "list-add"
                    enabled: addField.text.trim().length > 0
                    onClicked: addField.addCurrent()
                }
            }
            Kirigami.SearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: app.currentListVirtual
                                 ? i18n("Search across all lists ...")
                                 : i18n("Search tasks ...")
                onTextChanged: app.tasksModel.filterText = text
                Keys.onDownPressed: page.focusTaskList()
            }
            RowLayout {
                Layout.fillWidth: true
                visible: app.tasksModel.sortMode !== "default"
                spacing: 0
                QQC2.Label {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    text: {
                        switch (app.tasksModel.sortMode) {
                        case "importance": return i18n("Sorted by importance");
                        case "dueDate": return i18n("Sorted by due date");
                        case "title": return i18n("Sorted alphabetically");
                        case "created": return i18n("Sorted by creation date");
                        }
                        return "";
                    }
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.ToolButton {
                    icon.name: app.tasksModel.sortDescending ? "view-sort-descending"
                                                             : "view-sort-ascending"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Reverse sort order")
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: app.tasksModel.sortDescending = !app.tasksModel.sortDescending
                }
                Item { Layout.fillWidth: true }
                QQC2.ToolButton {
                    icon.name: "dialog-close"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Remove sort order")
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: app.tasksModel.sortMode = "default"
                }
            }
        }
    }

    ListView {
        id: tasksView
        model: app.tasksModel
        spacing: 0
        // Arrow keys move the current row once the list has focus (Down from
        // the add/search field), Return opens it, Space toggles it in the
        // multi-selection, Ctrl+A selects all, Delete deletes.
        keyNavigationEnabled: true
        currentIndex: -1
        Keys.onReturnPressed: if (currentIndex >= 0) app.openTaskDetails(currentIndex)
        Keys.onEnterPressed: if (currentIndex >= 0) app.openTaskDetails(currentIndex)
        // No confirmation needed: deletes can be undone for a few seconds.
        Keys.onDeletePressed: {
            const ids = page.keyboardTargets();
            if (ids.length === 0) return;
            app.deleteTasks(ids);
            page.clearSelection();
        }
        Keys.onEscapePressed: event => {
            if (page.selectedIds.length > 0) page.clearSelection();
            else event.accepted = false;
        }
        Keys.onPressed: event => {
            if (event.matches(StandardKey.SelectAll)) {
                page.selectAll();
                event.accepted = true;
            } else if (event.key === Qt.Key_Space && currentIndex >= 0) {
                page.toggleSelected(currentIndex);
                event.accepted = true;
            }
        }

        section.property: "sectionLabel"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            width: ListView.view.width
            // Explicit sort orders put all open tasks in one unlabeled section.
            height: section.length > 0 ? Kirigami.Units.gridUnit * 1.8 : 0
            visible: section.length > 0
            Kirigami.Heading {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Kirigami.Units.largeSpacing
                level: 4
                text: section
                color: Kirigami.Theme.disabledTextColor
            }
        }

        delegate: TaskDelegate {
            width: ListView.view.width
            selected: page.isSelected(taskId)
            highlighted: selected || (ListView.isCurrentItem && tasksView.activeFocus)
            onCheck: completed ? app.uncompleteTask(taskId) : app.completeTask(taskId)
            onRemove: { app.deleteTasks(page.targetsFor(taskId)); page.clearSelection(); }
            onToggleStar: app.toggleImportance(taskId)
            onToggleSelected: page.toggleSelected(index)
            onExtendSelection: page.extendSelection(index)
            onMoveTo: listId => { app.moveTasks(page.targetsFor(taskId), listId); page.clearSelection(); }
            onToggleMyDay: app.toggleMyDay(page.targetsFor(taskId))
            onOpenDetails: {
                tasksView.currentIndex = index;
                app.openTaskDetails(index);
            }
            onPickDate: app.requestPickDateForDue(taskId)
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: tasksView.count === 0
            readonly property bool myDay: app.currentListId === "__myday__" && searchField.text.length === 0
            text: searchField.text.length > 0 ? i18n("No matching tasks")
                : myDay ? i18n("Focus on your day") : i18n("No tasks")
            explanation: searchField.text.length > 0
                ? i18n("Try a different search term.")
                : myDay ? i18n("Add tasks here, or press Ctrl+T on a task in any list.")
                : i18n("Type a task above and press Enter.")
            icon.name: searchField.text.length > 0 ? "system-search"
                : myDay ? "weather-clear-symbolic" : "checkmark"
        }
    }
}
