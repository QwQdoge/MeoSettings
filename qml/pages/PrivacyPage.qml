import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function moduleRow(id, title, subtitle, icon) {
        const available = KcmBridge.isAvailable(id)
        return {
            "title": title,
            "subtitle": available ? subtitle : qsTr("This advanced system tool is not installed"),
            "icon": icon,
            "tone": "neutral",
            "route": "kcm:" + id,
            "enabled": available,
            "trailingKind": "choice",
            "trailingText": qsTr("Advanced")
        }
    }

    readonly property var applicationRows: {
        const rows = []
        const apps = AppPermissionsBackend.apps || []
        for (let index = 0; index < apps.length; ++index) {
            const app = apps[index]
            rows.push({
                "id": app.id,
                "title": app.name || app.id,
                "subtitle": qsTr("Review effective Flatpak sandbox permissions\n%1").arg(app.id),
                "icon": app.icon || "admin_panel_settings",
                "tone": "primary",
                "trailingKind": "navigation"
            })
        }
        return rows
    }

    function inspectApplication(row) {
        if (!row || !row.id)
            return
        permissionDetails.applicationId = row.id
        permissionDetails.applicationName = row.title || row.id
        AppPermissionsBackend.inspect(row.id)
        permissionDetails.open()
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Privacy & security")
        subtitle: qsTr("Review verifiable app sandbox access, then use maintained system tools for security-sensitive controls.")

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale

                MeoText {
                    width: parent.width
                    text: qsTr("Permission coverage")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo lists effective Flatpak sandbox permissions from local metadata. Linux does not expose a trustworthy universal history of which native desktop app used a camera, microphone, or file permission, so Meo does not invent one. Use these verified grants to review sandboxed apps.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.applicationRows.length > 0
            title: qsTr("App permissions")
            subtitle: AppPermissionsBackend.sourceDescription
            model: root.applicationRows
            onRowActivated: (index, row) => root.inspectApplication(row)
        }

        MeoEmptyState {
            width: parent.width
            height: 205 * MeoTheme.globalScale
            visible: !AppPermissionsBackend.busy && root.applicationRows.length === 0
            icon: "admin_panel_settings"
            title: AppPermissionsBackend.available
                   ? qsTr("No sandboxed applications found")
                   : qsTr("Flatpak permission source unavailable")
            description: AppPermissionsBackend.summary
            actionText: AppPermissionsBackend.available ? qsTr("Refresh") : ""
            onActionClicked: AppPermissionsBackend.refresh()
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Security controls")
            subtitle: qsTr("These protected workflows retain their established system owner")
            model: [
                root.moduleRow("kcm_screenlocker", qsTr("Screen lock"),
                               qsTr("Lock screen behavior and security"), "lock"),
                root.moduleRow("kcm_firewall", qsTr("Firewall"),
                               qsTr("Network firewall rules and status"), "security"),
                root.moduleRow("kcm_firmware_security", qsTr("Device security"),
                               qsTr("Firmware, TPM, and boot-security information"), "shield")
            ]
            onRowActivated: (index, row) => {
                if (row.enabled && row.route)
                    root.navigateTo(row.route)
            }
        }

        MeoButton {
            text: AppPermissionsBackend.busy ? qsTr("Refreshing…") : qsTr("Refresh permission inventory")
            type: "text"
            enabled: AppPermissionsBackend.available && !AppPermissionsBackend.busy
            onClicked: AppPermissionsBackend.refresh()
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: AppPermissionsBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: AppPermissionsBackend.error
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: permissionDetails
        popupParent: Overlay.overlay
        property string applicationId: ""
        property string applicationName: ""
        title: applicationName || qsTr("Application permissions")
        subtitle: qsTr("Effective Flatpak sandbox access for %1. This is a local metadata read, not a claim about historical usage.")
                  .arg(applicationId)
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: detailContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: detailContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 16 * MeoTheme.globalScale

                    MeoLoadingIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: AppPermissionsBackend.inspecting
                        size: "m"
                    }

                    MeoSettingsGroup {
                        width: parent.width
                        visible: !AppPermissionsBackend.inspecting
                                 && AppPermissionsBackend.selectedPermissions.length > 0
                        title: qsTr("Granted sandbox access")
                        subtitle: qsTr("Values are exactly the effective Flatpak permission entries")
                        model: AppPermissionsBackend.selectedPermissions.map(permission => ({
                            "title": permission.title,
                            "subtitle": permission.details,
                            "icon": permission.icon,
                            "tone": permission.tone,
                            "trailingKind": "none",
                            "interactive": false
                        }))
                    }

                    MeoEmptyState {
                        width: parent.width
                        height: 200 * MeoTheme.globalScale
                        visible: !AppPermissionsBackend.inspecting
                                 && AppPermissionsBackend.selectedPermissions.length === 0
                        icon: "verified_user"
                        title: AppPermissionsBackend.error !== ""
                               ? qsTr("Permission information unavailable")
                               : qsTr("No additional sandbox entries")
                        description: AppPermissionsBackend.error !== ""
                                     ? AppPermissionsBackend.error
                                     : qsTr("Flatpak did not report additional effective permission entries for this app.")
                    }
                }
            }
        }
    }

    Component.onCompleted: AppPermissionsBackend.refresh()
}
