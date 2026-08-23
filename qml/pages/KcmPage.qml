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
                    text: qsTr("Advanced system tool")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("This specialized workflow opens in KDE’s supported configuration shell. It is not embedded because some installed modules are QWidget-based or need their own privilege, recovery, or authentication surface.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                MeoButton {
                    text: qsTr("Open advanced tool")
                    type: "filled"
                    enabled: KcmBridge.launcherAvailable
                    onClicked: KcmBridge.open(root.moduleId)
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: KcmBridge.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: KcmBridge.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !root.installed
            icon: "extension_off"
            title: qsTr("Settings module unavailable")
            description: qsTr("This advanced system tool is not installed in the current system image.")
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
                    text: qsTr("Why this is a fallback")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings is the normal settings experience. This advanced tool remains separate because its complex workflow is already tested for system-specific permissions, recovery, or hardware behavior.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
