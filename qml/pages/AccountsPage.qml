import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property bool showingCloudIdentity: AccountBackend.signedIn
    readonly property string identityName: {
        if (AccountBackend.signedIn)
            return AccountBackend.cloudName || qsTr("Meo Account")
        return SystemInfoBackend.userName || qsTr("Local user")
    }
    readonly property string identityAvatar: AccountBackend.signedIn
                                          ? AccountBackend.cloudAvatarSource : SystemInfoBackend.userAvatarSource

    function initials(name) {
        const parts = String(name || "").trim().split(/\s+/).filter(part => part.length > 0)
        if (parts.length === 0)
            return ""
        if (parts.length === 1)
            return parts[0].slice(0, 2)
        return parts[0].slice(0, 1) + parts[parts.length - 1].slice(0, 1)
    }

    readonly property var providerRows: [
        {
            "id": "online-accounts",
            "title": qsTr("Connected providers"),
            "subtitle": KcmBridge.isAvailable("kcm_kaccounts")
                        ? qsTr("Calendar, contacts, files, and other KDE account providers")
                        : qsTr("The KDE online-account module is not installed"),
            "icon": "cloud", "tone": "secondary",
            "route": "kcm:kcm_kaccounts",
            "enabled": KcmBridge.isAvailable("kcm_kaccounts"),
            "trailingKind": "choice", "trailingText": qsTr("Advanced")
        },
        {
            "id": "wallet",
            "title": qsTr("Password wallet"),
            "subtitle": KcmBridge.isAvailable("kcm_kwallet5")
                        ? qsTr("Credential storage used by KDE applications and the Meo Account broker")
                        : qsTr("The KDE wallet module is not installed"),
            "icon": "key", "tone": "tertiary",
            "route": "kcm:kcm_kwallet5",
            "enabled": KcmBridge.isAvailable("kcm_kwallet5"),
            "trailingKind": "choice", "trailingText": qsTr("Advanced")
        }
    ]

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Accounts")
        subtitle: qsTr("Meo Account is the shared cloud identity. Local users and provider-specific credentials keep their own protected system owners.")

        MeoCard {
            width: parent.width
            type: "filled"

            Column {
                width: parent.width
                spacing: 14 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 14 * MeoTheme.globalScale

                    MeoAvatar {
                        anchors.verticalCenter: parent.verticalCenter
                        source: root.identityAvatar
                        initials: root.initials(root.identityName)
                        size: root.isCompact ? 52 : 60
                        color: MeoTheme.primaryContainer
                        textColor: MeoTheme.contentOnPrimaryContainer
                    }

                    Column {
                        width: parent.width - (root.isCompact ? 52 : 60) * MeoTheme.globalScale - parent.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3 * MeoTheme.globalScale

                        MeoText {
                            width: parent.width
                            text: root.showingCloudIdentity ? root.identityName : qsTr("Meo Account")
                            typeRole: "title"
                            typeSize: "medium"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                            elide: Text.ElideRight
                        }

                        MeoText {
                            width: parent.width
                            text: root.showingCloudIdentity
                                  ? (AccountBackend.cloudId !== ""
                                     ? qsTr("Cloud ID · %1").arg(AccountBackend.cloudId)
                                     : qsTr("Cloud profile is connected"))
                                  : AccountBackend.summary
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                MeoText {
                    width: parent.width
                    visible: root.showingCloudIdentity && !AccountBackend.identityGranted
                    text: qsTr("The broker has not granted this installed Settings package a profile ID. Your public account name and avatar remain visible; Settings will not invent or copy an ID.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoButton {
                    text: root.showingCloudIdentity ? qsTr("Manage Meo Account") : qsTr("Connect Meo Account")
                    type: "filled"
                    visible: AccountBackend.settingsLauncherAvailable || AccountBackend.serviceRunning
                    onClicked: AccountBackend.openAccountSettings()
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !AccountBackend.settingsLauncherAvailable && !AccountBackend.serviceRunning

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "cloud_off"; size: 24; color: MeoTheme.contentOnSurfaceVariant }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("Install and configure the Meo Account desktop broker to connect a cloud profile. Settings never stores your Meo Account token or password.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Local account")
            subtitle: qsTr("The local Unix account remains the device authority. Its administration is intentionally a button here, rather than a separate Settings destination.")
            model: [{
                "title": root.showingCloudIdentity ? qsTr("This device session") : (SystemInfoBackend.userName || qsTr("Local user")),
                "subtitle": SystemInfoBackend.deviceName !== ""
                            ? qsTr("Local device · %1").arg(SystemInfoBackend.deviceName)
                            : qsTr("Local device session"),
                "icon": "person", "tone": "neutral",
                "trailingKind": "none", "interactive": false
            }]
        }

        MeoButton {
            text: qsTr("Manage local users")
            type: "outlined"
            enabled: KcmBridge.isAvailable("kcm_users")
            onClicked: root.navigateTo("kcm:kcm_users")
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Other account providers")
            subtitle: qsTr("These providers retain their KDE-owned account and credential workflows.")
            model: root.providerRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: AccountBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: AccountBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoButton {
            text: qsTr("Refresh account status")
            type: "text"
            onClicked: AccountBackend.refresh()
        }
    }
}
