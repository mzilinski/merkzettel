import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: ""

    actions: [
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
                placeholderText: app.currentListId === "__all__"
                                 ? i18n("Search across all lists ...")
                                 : i18n("Search tasks ...")
                onTextChanged: app.tasksModel.filterText = text
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
            height: Kirigami.Units.gridUnit * 1.8
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
