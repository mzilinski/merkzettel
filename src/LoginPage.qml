import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    title: i18n("Sign in")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing * 2

        Item { Layout.preferredHeight: Kirigami.Units.gridUnit }

        Kirigami.Icon {
            source: "io.github.mzilinski.merkzettel"
            fallback: "view-pim-tasks"
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
            Layout.maximumWidth: Kirigami.Units.gridUnit * 26
            Layout.alignment: Qt.AlignHCenter
        }

        // Builds without an Azure registration (e.g. a plain source build)
        // cannot sign in; say so instead of failing after the browser opens.
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 26
            Layout.alignment: Qt.AlignHCenter
            visible: !app.signInAvailable
            type: Kirigami.MessageType.Warning
            text: i18n("This build has no Microsoft app registration, so signing in is not possible. Rebuild with your own client ID or try the demo.")
            actions: Kirigami.Action {
                text: i18n("Setup guide")
                icon.name: "help-contents"
                onTriggered: Qt.openUrlExternally("https://mzilinski.github.io/merkzettel/docs/en/azure-setup/")
            }
        }

        QQC2.Button {
            text: i18n("Sign in")
            icon.name: "go-next"
            enabled: app.signInAvailable
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.login()
        }

        QQC2.Button {
            text: i18n("Try the demo")
            icon.name: "media-playback-start"
            flat: true
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.enterDemoMode()
            QQC2.ToolTip.text: i18n("Explore Merkzettel with sample tasks. Nothing is sent to Microsoft.")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 26
            Layout.alignment: Qt.AlignHCenter
        }

        // What the consent screen in the browser will ask for, in plain words.
        Kirigami.Heading {
            level: 4
            text: i18n("What Merkzettel asks Microsoft for")
            Layout.alignment: Qt.AlignHCenter
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: Kirigami.Units.gridUnit * 26
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { icon: "view-pim-tasks", text: i18n("Read and change your tasks and lists in Microsoft To Do (Tasks.ReadWrite)") },
                    { icon: "user", text: i18n("Your name and sign-in address to identify the account (User.Read, profile)") },
                    { icon: "lock", text: i18n("Stay signed in between sessions; the token is kept in your system keychain (offline_access)") },
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon {
                        source: modelData.icon
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        Layout.alignment: Qt.AlignTop
                    }
                    QQC2.Label {
                        text: modelData.text
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                text: i18n("Merkzettel only talks to Microsoft. There is no telemetry and no other server involved.")
            }
        }
    }
}
