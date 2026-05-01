import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: delegate

    signal check()
    signal remove()
    signal toggleStar()
    signal openDetails()
    signal pickDate()

    onClicked: openDetails()

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.CheckBox {
            checked: model.completed
            onToggled: delegate.check()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: model.title
                elide: Text.ElideRight
                font.strikeout: model.completed
                opacity: model.completed ? 0.6 : 1.0
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                visible: dueLbl.text.length > 0 || bodyHint.visible || progressLbl.visible

                Kirigami.Icon {
                    visible: model.dueLabel.length > 0
                    source: "view-calendar-day"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    id: dueLbl
                    text: model.dueLabel
                    visible: text.length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                }
                Item {
                    visible: dueLbl.visible && bodyHint.visible
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    id: bodyHint
                    visible: model.body && model.body.length > 0
                    source: "text-x-generic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }
                Item {
                    visible: recurrenceIcon.visible && (dueLbl.visible || bodyHint.visible)
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    id: recurrenceIcon
                    visible: model.hasRecurrence
                    source: "view-refresh-symbolic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }
                Item {
                    visible: linkIcon.visible && (dueLbl.visible || bodyHint.visible || recurrenceIcon.visible)
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    id: linkIcon
                    visible: (model.linkedResourceCount || 0) > 0
                    source: "internet-services"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }
                Item {
                    visible: progressLbl.visible && (dueLbl.visible || bodyHint.visible)
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    id: progressIcon
                    visible: progressLbl.visible
                    source: "checkbox"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    id: progressLbl
                    text: model.checklistProgress || ""
                    visible: text.length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                }
                Item { Layout.fillWidth: true }
            }
        }

        QQC2.ToolButton {
            icon.name: model.important ? "starred-symbolic" : "non-starred-symbolic"
            icon.color: model.important ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
            display: QQC2.AbstractButton.IconOnly
            opacity: (model.important || delegate.hovered) ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
            QQC2.ToolTip.text: model.important ? i18n("Remove importance") : i18n("Mark as important")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: delegate.toggleStar()
        }

        QQC2.ToolButton {
            icon.name: "edit-delete"
            display: QQC2.AbstractButton.IconOnly
            opacity: delegate.hovered ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: Kirigami.Units.shortDuration } }
            onClicked: delegate.remove()
        }
    }

    QQC2.Menu {
        id: ctxMenu
        QQC2.MenuItem {
            text: model.important ? i18n("Unstar") : i18n("Mark as important")
            icon.name: model.important ? "non-starred-symbolic" : "starred-symbolic"
            onTriggered: delegate.toggleStar()
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18n("Due: Today")
            icon.name: "view-calendar-day"
            onTriggered: app.setTaskDueDate(model.taskId, new Date(new Date().setHours(9, 0, 0, 0)))
        }
        QQC2.MenuItem {
            text: i18n("Due: Tomorrow")
            icon.name: "view-calendar-day"
            onTriggered: {
                const d = new Date();
                d.setDate(d.getDate() + 1);
                d.setHours(9, 0, 0, 0);
                app.setTaskDueDate(model.taskId, d);
            }
        }
        QQC2.MenuItem {
            text: i18n("Pick date ...")
            icon.name: "view-calendar"
            onTriggered: delegate.pickDate()
        }
        QQC2.MenuItem {
            text: i18n("Clear date")
            icon.name: "edit-clear"
            enabled: model.dueLabel.length > 0
            onTriggered: app.clearTaskDueDate(model.taskId)
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: i18n("Delete")
            icon.name: "edit-delete"
            onTriggered: delegate.remove()
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: ctxMenu.popup()
    }

    Kirigami.Separator {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: 0.4
    }
}
