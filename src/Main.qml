import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.dateandtime as KDA
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Merkzettel")
    minimumWidth: Kirigami.Units.gridUnit * 24
    minimumHeight: Kirigami.Units.gridUnit * 30
    width: Kirigami.Units.gridUnit * 36
    height: Kirigami.Units.gridUnit * 42
    visible: !app.startMinimized

    // Persists the "share explainer was acknowledged" flag across sessions
    // so we only show the banner once per user.
    Settings {
        id: shareSettings
        category: "Share"
        property bool explainerSeen: false
    }

    function shareList(listId, displayName) {
        // Microsoft Graph doesn't expose share-management for To-Do lists.
        // Hand off to the official web app, where the user already has the
        // familiar share UI; once the invitation is accepted, the list shows
        // up automatically here with isShared = true.
        const url = "https://to-do.office.com/tasks/list/" + listId;
        if (!shareSettings.explainerSeen) {
            shareExplainer.contextUrl = url;
            shareExplainer.contextName = displayName;
            shareExplainer.visible = true;
        } else {
            Qt.openUrlExternally(url);
        }
    }

    Connections {
        target: app
        function onWindowToggleRequested() {
            if (root.visible && root.windowState !== Qt.WindowMinimized) {
                root.hide();
            } else {
                root.show();
                root.raise();
                root.requestActivate();
            }
        }
        function onErrorOccurred(message) {
            errorBanner.text = message;
            errorBanner.visible = true;
        }
        function onPickDateRequested(taskId, initial) {
            datePopup.mode = "due";
            datePopup.contextTaskId = taskId;
            datePopup.value = initial;
            datePopup.open();
        }
    }

    onClosing: function(close) {
        // Hide to tray instead of quitting.
        close.accepted = false;
        root.hide();
    }

    globalDrawer: Kirigami.GlobalDrawer {
        title: i18n("Lists")
        titleIcon: "view-pim-tasks"
        isMenu: false
        modal: !root.wideScreen
        collapsible: true

        topContent: [
            Kirigami.InlineMessage {
                id: errorBanner
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                showCloseButton: true
                visible: false
            },
            Kirigami.InlineMessage {
                id: shareExplainer
                property string contextUrl: ""
                property string contextName: ""
                Layout.fillWidth: true
                type: Kirigami.MessageType.Information
                showCloseButton: true
                visible: false
                text: i18n("Sharing happens in the official Microsoft To Do web app. After the invitation is accepted, the list will appear here automatically.")
                actions: [
                    Kirigami.Action {
                        text: i18n("Open To Do web")
                        icon.name: "internet-services"
                        onTriggered: {
                            Qt.openUrlExternally(shareExplainer.contextUrl);
                            shareSettings.explainerSeen = true;
                            shareExplainer.visible = false;
                        }
                    },
                    Kirigami.Action {
                        text: i18n("Don't show again")
                        icon.name: "dialog-ok"
                        onTriggered: {
                            shareSettings.explainerSeen = true;
                            shareExplainer.visible = false;
                        }
                    }
                ]
            }
        ]

        actions: [
            Kirigami.Action {
                text: i18n("New list ...")
                icon.name: "list-add"
                enabled: app.loggedIn
                onTriggered: createListDialog.open()
            },
            Kirigami.Action {
                text: i18n("Sync")
                icon.name: "view-refresh"
                enabled: app.loggedIn
                onTriggered: app.refresh()
            },
            Kirigami.Action {
                text: i18n("Sign out")
                icon.name: "system-log-out"
                enabled: app.loggedIn
                onTriggered: app.logout()
            },
            Kirigami.Action { separator: true },
            Kirigami.Action {
                text: i18n("Donate ...")
                icon.name: "help-donate"
                onTriggered: Qt.openUrlExternally("https://paypal.me/eit31")
            },
            Kirigami.Action {
                text: i18n("About Merkzettel")
                icon.name: "help-about"
                onTriggered: {
                    const top = pageStack.layers.currentItem;
                    if (top && top.objectName === "aboutPage") return;
                    pageStack.layers.push(aboutPage);
                }
            }
        ]

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Repeater {
                model: app.listsModel
                delegate: QQC2.ItemDelegate {
                    id: listItem
                    Layout.fillWidth: true
                    text: root.globalDrawer.collapsed ? "" : model.displayName
                    icon.name: model.isDefault ? "emblem-favorite" : "view-list-text"
                    display: root.globalDrawer.collapsed
                             ? QQC2.AbstractButton.IconOnly
                             : QQC2.AbstractButton.TextBesideIcon
                    highlighted: app.currentListId === model.listId
                    QQC2.ToolTip.text: model.isShared
                                       ? i18n("%1 (shared)", model.displayName)
                                       : model.displayName
                    QQC2.ToolTip.visible: hovered && root.globalDrawer.collapsed
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        while (pageStack.layers.depth > 1) {
                            pageStack.layers.pop();
                        }
                        app.currentListId = model.listId;
                        if (!root.wideScreen) {
                            root.globalDrawer.close();
                        }
                    }

                    // Trailing share-indicator. Hidden when the drawer is
                    // collapsed (icon-only mode) — the tooltip carries the
                    // info there.
                    Kirigami.Icon {
                        anchors.right: parent.right
                        anchors.rightMargin: Kirigami.Units.smallSpacing
                        anchors.verticalCenter: parent.verticalCenter
                        width: Kirigami.Units.iconSizes.small
                        height: Kirigami.Units.iconSizes.small
                        source: "emblem-shared-symbolic"
                        visible: model.isShared && !root.globalDrawer.collapsed
                        opacity: 0.7
                    }

                    QQC2.Menu {
                        id: listCtxMenu
                        QQC2.MenuItem {
                            text: i18n("Share ...")
                            icon.name: "emblem-shared-symbolic"
                            onTriggered: shareList(model.listId, model.displayName)
                        }
                        QQC2.MenuSeparator {}
                        QQC2.MenuItem {
                            text: i18n("Rename list ...")
                            icon.name: "edit-rename"
                            enabled: !model.isDefault
                            onTriggered: {
                                renameListDialog.listId = model.listId;
                                renameListDialog.oldName = model.displayName;
                                renameListDialog.open();
                            }
                        }
                        QQC2.MenuItem {
                            text: i18n("Delete list ...")
                            icon.name: "edit-delete"
                            enabled: !model.isDefault
                            onTriggered: {
                                deleteListDialog.listId = model.listId;
                                deleteListDialog.listName = model.displayName;
                                deleteListDialog.open();
                            }
                        }
                    }

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: listCtxMenu.popup()
                    }
                }
            }
        }
    }

    Component {
        id: loginPage
        LoginPage {}
    }

    Component {
        id: tasksPage
        TasksPage {}
    }

    Component {
        id: aboutPage
        FormCard.AboutPage {
            objectName: "aboutPage"
            aboutData: ({
                "displayName": "Merkzettel",
                "productName": "merkzettel",
                "componentName": "merkzettel",
                "shortDescription": i18n("Microsoft To Do client for KDE"),
                "homepage": "https://invent.kde.org/",
                "bugAddress": "https://invent.kde.org/",
                "version": appVersion,
                "otherText": i18n("Built with Kirigami, KDE Frameworks 6 and Qt 6.\n\nIf Merkzettel is useful to you, consider tipping the author: https://paypal.me/eit31\n\nThe underlying KDE/Qt stack is maintained by KDE e.V.: https://kde.org/community/donations/\n\nDisclaimer: This program is provided \"as is\", without warranty of any kind. The author accepts no liability for any data loss, missed appointments or other damages arising from its use, except in cases of intent or gross negligence, or for damages to life, body or health. See the GPL-3.0 license, sections 15 and 16, for the full disclaimer."),
                "copyrightStatement": "© 2026 Malte Zilinski",
                "desktopFileName": "org.kde.merkzettel",
                "authors": [{
                    "name": "Malte Zilinski",
                    "task": i18n("Author"),
                    "emailAddress": "malte@zilinski.eu",
                    "webAddress": "",
                    "ocsUsername": ""
                }],
                "licenses": [{
                    "name": "GPL v3",
                    "text": "https://www.gnu.org/licenses/gpl-3.0.txt",
                    "spdx": "GPL-3.0-or-later"
                }]
            })
        }
    }

    Component.onCompleted: {
        pageStack.push(app.loggedIn ? tasksPage : loginPage);
    }

    Connections {
        target: app
        function onLoggedInChanged() {
            root.pageStack.replace(app.loggedIn ? tasksPage : loginPage);
        }
    }

    Connections {
        target: app
        function onStatusChanged() {
            if (app.status.length > 0) {
                showPassiveNotification(app.status, "short");
            }
        }
    }

    TaskDetailSheet {
        id: detailSheet
        parent: root.overlay
        onDatePickerRequested: function(initialDate) {
            datePopup.mode = "due";
            datePopup.contextTaskId = "";  // empty → use detailTask
            datePopup.value = initialDate;
            datePopup.open();
        }
        onReminderDatePickerRequested: function(initialDate) {
            datePopup.mode = "reminder";
            datePopup.contextTaskId = "";
            datePopup.value = initialDate;
            datePopup.open();
        }
        onReminderTimePickerRequested: function(initialDate) {
            timePopup.value = initialDate;
            timePopup.open();
        }
    }

    KDA.DatePopup {
        id: datePopup
        property string mode: "due"        // "due" or "reminder"
        property string contextTaskId: ""  // non-empty when triggered outside detail sheet
        parent: root.overlay
        implicitWidth: Math.min(root.width - Kirigami.Units.gridUnit * 4,
                                Kirigami.Units.gridUnit * 24)
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        modal: true
        onAccepted: {
            const tid = contextTaskId || (app.detailTask ? app.detailTask.taskId : "");
            if (!tid) return;
            const d = new Date(value);
            if (mode === "due") {
                d.setHours(9, 0, 0, 0);
                app.setTaskDueDate(tid, d);
            } else {
                const existing = app.detailTask ? app.detailTask.reminderDate : null;
                if (existing && !isNaN(existing.getTime())) {
                    d.setHours(existing.getHours(), existing.getMinutes(), 0, 0);
                } else {
                    d.setHours(9, 0, 0, 0);
                }
                app.setTaskReminder(tid, d);
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteListDialog
        property string listId: ""
        property string listName: ""
        parent: root.overlay
        title: i18n("Delete list?")
        subtitle: i18n("This will permanently delete the list \"%1\" and all of its tasks. This cannot be undone.", listName)
        standardButtons: Kirigami.PromptDialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: deleteListDialog.close()
            },
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    app.deleteList(deleteListDialog.listId);
                    deleteListDialog.close();
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: createListDialog
        parent: root.overlay
        title: i18n("New list")
        standardButtons: Kirigami.PromptDialog.NoButton

        onOpened: {
            createNameField.text = "";
            createNameField.forceActiveFocus();
        }

        QQC2.TextField {
            id: createNameField
            Layout.fillWidth: true
            placeholderText: i18n("List name")
            onAccepted: {
                if (text.trim().length > 0) {
                    app.createList(text.trim());
                    createListDialog.close();
                }
            }
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: createListDialog.close()
            },
            Kirigami.Action {
                text: i18n("Create")
                icon.name: "list-add"
                enabled: createNameField.text.trim().length > 0
                onTriggered: {
                    app.createList(createNameField.text.trim());
                    createListDialog.close();
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: renameListDialog
        property string listId: ""
        property string oldName: ""
        parent: root.overlay
        title: i18n("Rename list")
        standardButtons: Kirigami.PromptDialog.NoButton

        onOpened: {
            renameField.text = oldName;
            renameField.selectAll();
            renameField.forceActiveFocus();
        }

        QQC2.TextField {
            id: renameField
            Layout.fillWidth: true
            placeholderText: i18n("New list name")
            onAccepted: {
                if (text.trim().length > 0 && text.trim() !== renameListDialog.oldName) {
                    app.renameList(renameListDialog.listId, text.trim());
                }
                renameListDialog.close();
            }
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: renameListDialog.close()
            },
            Kirigami.Action {
                text: i18n("Rename")
                icon.name: "edit-rename"
                enabled: renameField.text.trim().length > 0
                         && renameField.text.trim() !== renameListDialog.oldName
                onTriggered: {
                    app.renameList(renameListDialog.listId, renameField.text.trim());
                    renameListDialog.close();
                }
            }
        ]
    }

    KDA.TimePopup {
        id: timePopup
        parent: root.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        modal: true
        onAccepted: {
            if (!app.detailTask || !app.detailTask.taskId) return;
            const base = (app.detailTask.reminderDate
                          && !isNaN(app.detailTask.reminderDate.getTime()))
                ? new Date(app.detailTask.reminderDate)
                : new Date();
            const t = new Date(value);
            base.setHours(t.getHours(), t.getMinutes(), 0, 0);
            app.setTaskReminder(app.detailTask.taskId, base);
        }
    }
}
