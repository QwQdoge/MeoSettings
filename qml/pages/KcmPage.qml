import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property string moduleId: ""
    property string pageTitle: ""
    property string pageDescription: ""
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property bool installed: KcmBridge.isAvailable(moduleId)

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : root.pageTitle
        subtitle: root.pageDescription

        MeoCard {
            width: parent.width
            type: "filled"
            visible: root.installed
            Accessible.role: Accessible.StatusBar
            Accessible.name: qsTr("Advanced system setting")
            Accessible.description: root.pageDescription

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                MeoIcon {
                    icon: "open_in_new"
                    size: 28
                    color: MeoTheme.primary
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Advanced system settings")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("This setting opens in the system tool that supports it. It stays separate because it may need its own permission, recovery, or sign-in steps.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                MeoButton {
                    text: qsTr("Open system settings")
                    type: "filled"
                    enabled: KcmBridge.launcherAvailable
                    onClicked: KcmBridge.open(root.moduleId)
                }
            }
        }

        Column {
            width: parent.width
            visible: KcmBridge.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("System settings could not open")
                text: qsTr("Check that this system tool is installed, then try opening it again.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(KcmBridge.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !root.installed
            icon: "extension_off"
            title: qsTr("This setting is not available")
            description: qsTr("This advanced setting is not included in this system.")
            actionText: qsTr("Back to Home")
            onActionClicked: root.navigateTo("home")
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.installed

            Column {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale
                MeoText {
                    text: qsTr("Why this opens in system settings")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings is your everyday settings app. This advanced setting stays in its system tool because its controls have their own tested permission, recovery, or hardware steps.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
