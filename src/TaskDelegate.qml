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
    // Multi-selection: Ctrl+click toggles, Shift+click extends.
    signal toggleSelected()
    signal extendSelection()
    signal moveTo(string listId)
    signal toggleMyDay()

    // Set by the view; drives the highlight together with keyboard focus.
    property bool selected: false
    // Secondary text/icons; on a highlighted (selected/current) row the grey
    // would vanish into the highlight, so switch to the highlighted text colour.
    readonly property color metaColor: highlighted
        ? Qt.alpha(Kirigami.Theme.highlightedTextColor, 0.8)
        : Kirigami.Theme.disabledTextColor
    // Hoisted out of `model` so the category Repeater (whose own `model`
    // shadows ours) can reach it.
    readonly property var categories: model.categories || []

    // The current row holds keyboard focus inside the ListView, and
    // ItemDelegate would treat Space as a click — so toggle the selection
    // here before the button sees it.
    Keys.onSpacePressed: delegate.toggleSelected()

    // Plain clicks open the details; modifier clicks only select. Separate
    // handlers because ItemDelegate.clicked carries no modifier state.
    TapHandler {
        acceptedButtons: Qt.LeftButton
        acceptedModifiers: Qt.NoModifier
        onTapped: delegate.openDetails()
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        acceptedModifiers: Qt.ControlModifier
        onTapped: delegate.toggleSelected()
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        acceptedModifiers: Qt.ShiftModifier
        onTapped: delegate.extendSelection()
    }

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
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: delegate.highlighted ? Kirigami.Theme.highlightedTextColor
                                            : Kirigami.Theme.textColor
                font.strikeout: model.completed
                opacity: model.completed ? 0.6 : 1.0
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                visible: listLbl.visible || dueLbl.text.length > 0 || bodyHint.visible
                         || progressLbl.visible || delegate.categories.length > 0
                         || model.hasAttachments || myDayIcon.visible

                // Like MS To Do: mark My Day tasks everywhere except in My Day.
                Kirigami.Icon {
                    id: myDayIcon
                    visible: model.inMyDay && app.currentListId !== "__myday__"
                    source: "weather-clear-symbolic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: delegate.metaColor
                }
                QQC2.Label {
                    visible: myDayIcon.visible
                    text: i18n("My Day")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: delegate.metaColor
                    rightPadding: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    visible: listLbl.visible
                    source: "view-list-text"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: delegate.metaColor
                }
                QQC2.Label {
                    id: listLbl
                    text: model.listName || ""
                    textFormat: Text.PlainText
                    // Originating list only matters in the aggregated view —
                    // hide for normal lists where every row shares the same.
                    visible: text.length > 0 && app.currentListVirtual
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: delegate.metaColor
                    elide: Text.ElideRight
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 8
                }
                Item {
                    visible: listLbl.visible && dueLbl.visible
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }

                Kirigami.Icon {
                    visible: model.dueLabel.length > 0
                    source: "view-calendar-day"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: delegate.metaColor
                }
                QQC2.Label {
                    id: dueLbl
                    text: model.dueLabel
                    visible: text.length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: delegate.metaColor
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
                    color: delegate.metaColor
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
                    color: delegate.metaColor
                }
                Item {
                    visible: linkIcon.visible && (dueLbl.visible || bodyHint.visible || recurrenceIcon.visible)
                    Layout.preferredWidth: Kirigami.Units.smallSpacing
                }
                Kirigami.Icon {
                    visible: model.hasAttachments
                    source: "mail-attachment"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: delegate.metaColor
                }
                Kirigami.Icon {
                    id: linkIcon
                    visible: (model.linkedResourceCount || 0) > 0
                    source: "internet-services"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    color: delegate.metaColor
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
                    color: delegate.metaColor
                }
                QQC2.Label {
                    id: progressLbl
                    text: model.checklistProgress || ""
                    visible: text.length > 0
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: delegate.metaColor
                }
                Repeater {
                    model: delegate.categories
                    // Compact category tag; colours would need Outlook master categories.
                    Rectangle {
                        required property string modelData
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        implicitWidth: catLbl.implicitWidth + Kirigami.Units.smallSpacing * 2
                        implicitHeight: catLbl.implicitHeight
                        radius: height / 2
                        color: delegate.highlighted ? Qt.alpha(Kirigami.Theme.highlightedTextColor, 0.2)
                                                    : Qt.alpha(Kirigami.Theme.highlightColor, 0.18)
                        QQC2.Label {
                            id: catLbl
                            anchors.centerIn: parent
                            text: parent.modelData
                            textFormat: Text.PlainText
                            color: delegate.highlighted ? Kirigami.Theme.highlightedTextColor
                                                        : Kirigami.Theme.textColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }
                    }
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
            text: model.inMyDay ? i18n("Remove from My Day") : i18n("Add to My Day")
            icon.name: "weather-clear-symbolic"
            onTriggered: delegate.toggleMyDay()
        }
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
        MoveToListMenu {
            excludedListId: model.listId
            onListChosen: listId => delegate.moveTo(listId)
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
