import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// "Repeat every N days/weeks/months/years", plus weekday toggles for weeks —
// the same shape as MS To Do's custom recurrence editor.
Kirigami.Dialog {
    id: dialog

    property string taskId: ""
    property int interval: 1
    property string unit: "week"
    // Graph weekday names ("monday" ...), only used for unit === "week".
    property var days: []

    // Prefill from TasksModel's recurrence info (may be empty for "Never").
    function openFor(id, info) {
        taskId = id;
        const editable = info && info.unit && info.unit.length > 0;
        interval = editable ? info.interval : 1;
        unit = editable ? info.unit : "week";
        days = editable && info.days ? info.days.slice() : [];
        open();
    }

    readonly property var graphWeekdays: ["sunday", "monday", "tuesday", "wednesday",
                                          "thursday", "friday", "saturday"]

    title: i18n("Custom repeat")
    standardButtons: Kirigami.Dialog.NoButton
    padding: Kirigami.Units.largeSpacing

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onTriggered: dialog.close()
        },
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "dialog-ok"
            onTriggered: {
                app.setTaskRecurrence(dialog.taskId, dialog.unit, dialog.interval,
                                      dialog.unit === "week" ? dialog.days : []);
                dialog.close();
            }
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label { text: i18nc("Repeat every [N] [weeks]", "Every") }
            QQC2.SpinBox {
                from: 1
                to: 999
                value: dialog.interval
                editable: true
                onValueModified: dialog.interval = value
            }
            // Static model: the labels change with the interval (singular/
            // plural), and swapping the model would reset the selection.
            QQC2.ComboBox {
                id: unitCombo
                model: ["day", "week", "month", "year"]
                function unitLabel(unit) {
                    switch (unit) {
                    case "day": return i18ncp("Repeat every N ...", "day", "days", dialog.interval);
                    case "week": return i18ncp("Repeat every N ...", "week", "weeks", dialog.interval);
                    case "month": return i18ncp("Repeat every N ...", "month", "months", dialog.interval);
                    case "year": return i18ncp("Repeat every N ...", "year", "years", dialog.interval);
                    }
                    return "";
                }
                currentIndex: model.indexOf(dialog.unit)
                displayText: unitLabel(dialog.unit)
                delegate: QQC2.ItemDelegate {
                    required property string modelData
                    required property int index
                    width: ListView.view ? ListView.view.width : implicitWidth
                    text: unitCombo.unitLabel(modelData)
                    highlighted: unitCombo.highlightedIndex === index
                }
                onActivated: index => dialog.unit = model[index]
            }
        }

        // Weekday toggles in the locale's week order (e.g. Mon..Sun in de).
        RowLayout {
            visible: dialog.unit === "week"
            spacing: Kirigami.Units.smallSpacing
            Repeater {
                model: 7
                QQC2.Button {
                    required property int index
                    readonly property int jsDay: (Qt.locale().firstDayOfWeek + index) % 7
                    readonly property string graphDay: dialog.graphWeekdays[jsDay]
                    text: Qt.locale().dayName(jsDay, Locale.ShortFormat)
                    checkable: true
                    checked: dialog.days.indexOf(graphDay) >= 0
                    onToggled: dialog.days = checked
                        ? dialog.days.concat([graphDay])
                        : dialog.days.filter(d => d !== graphDay)
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            wrapMode: Text.WordWrap
            visible: dialog.unit !== "day" && !(dialog.unit === "week" && dialog.days.length > 0)
            text: i18n("Anchored on the task's due date (or today if it has none).")
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
        }
    }
}
