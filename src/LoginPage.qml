import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    title: i18n("Anmelden")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 22)
        spacing: Kirigami.Units.largeSpacing * 2

        Kirigami.Icon {
            source: "view-pim-tasks"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
        }

        Kirigami.Heading {
            text: i18n("Mit Microsoft anmelden")
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        QQC2.Label {
            text: i18n("Merkzettel synchronisiert deine Aufgaben mit Microsoft To Do. " +
                       "Es oeffnet sich der Standardbrowser fuer die Anmeldung.")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        QQC2.Button {
            text: i18n("Anmelden")
            icon.name: "go-next"
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.login()
        }
    }
}
