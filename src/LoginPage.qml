import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    title: i18n("Sign in")

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
            text: i18n("Sign in with Microsoft")
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        QQC2.Label {
            text: i18n("Merkzettel synchronizes your tasks with Microsoft To Do. The default browser will open for sign-in.")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        QQC2.Button {
            text: i18n("Sign in")
            icon.name: "go-next"
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.login()
        }
    }
}
