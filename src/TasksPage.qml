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
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                QQC2.TextField {
                    id: addField
                    Layout.fillWidth: true
                    placeholderText: i18n("New task ... (\"tomorrow\", \"mo\", \"25.5.\" at end for due date)")
                    onAccepted: addCurrent()
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
            onCheck: completed ? app.uncompleteTask(taskId) : app.completeTask(taskId)
            onRemove: app.deleteTask(taskId)
            onToggleStar: app.toggleImportance(taskId)
            onOpenDetails: app.openTaskDetails(index)
            onPickDate: app.requestPickDateForDue(taskId)
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: tasksView.count === 0
            text: searchField.text.length > 0 ? i18n("No matching tasks") : i18n("No tasks")
            explanation: searchField.text.length > 0
                ? i18n("Try a different search term.")
                : i18n("Type a task above and press Enter.")
            icon.name: searchField.text.length > 0 ? "system-search" : "checkmark"
        }
    }
}
