import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// "Move to list" menu listing every real (non-smart) list. Used as a
// submenu of the task context menu and from the selection bar.
QQC2.Menu {
    id: menu

    // Lists the selected tasks may not be moved to (their current list).
    property string excludedListId: ""

    signal listChosen(string listId)

    title: i18n("Move to")
    icon.name: "go-jump"

    Instantiator {
        model: app.listsModel
        delegate: QQC2.MenuItem {
            required property string listId
            required property string displayName
            required property string iconName
            required property bool isVirtual

            text: app.plain(displayName)
            icon.name: iconName
            enabled: listId !== menu.excludedListId
            // Smart-lists aren't move targets. Collapse instead of removing so
            // the Instantiator indices stay aligned with the menu.
            visible: !isVirtual
            height: visible ? implicitHeight : 0
            onTriggered: menu.listChosen(listId)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
