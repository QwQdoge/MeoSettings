import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property bool signedIn: AccountBackend.signedIn
    readonly property string identityName: signedIn
                                                   ? (AccountBackend.cloudName || qsTr("Meo Account"))
                                                   : qsTr("Meo Account")
    readonly property string identityAvatar: signedIn ? AccountBackend.cloudAvatarSource : ""
    readonly property var securityRows: [
        { "id": "email", "title": qsTr("Email address"),
          "subtitle": qsTr("Manage your sign-in email on the protected account page"),
          "icon": "mail", "tone": "primary", "trailingKind": "navigation", "interactive": signedIn,
          "action": "change_email" },
        { "id": "password", "title": qsTr("Password"),
          "subtitle": qsTr("Change your password after browser verification"),
          "icon": "password", "tone": "secondary", "trailingKind": "navigation", "interactive": signedIn,
          "action": "change_password" },
        { "id": "mfa", "title": qsTr("Multi-factor authentication"),
          "subtitle": AccountBackend.mfaEnabled ? qsTr("Enabled — review or remove verification factors") : qsTr("Not enabled — add a verification factor"),
          "icon": "verified_user", "tone": "tertiary", "trailingKind": "navigation", "interactive": signedIn,
          "action": "mfa" },
        { "id": "sessions", "title": qsTr("Devices and sessions"),
          "subtitle": qsTr("Review active sessions and revoke devices you no longer use"),
          "icon": "devices", "tone": "neutral", "trailingKind": "navigation", "interactive": signedIn,
          "action": "sessions" }
    ]
    readonly property var clientRows: {
        const rows = []
        const clients = AccountBackend.clients || []
        for (let index = 0; index < clients.length; ++index) {
            const client = clients[index]
            rows.push({
                "id": client.clientId || ("client-" + index),
                "title": client.clientId === "org.meo.OmniStore" ? qsTr("OmniStore") : (client.clientId || qsTr("Meo application")),
                "subtitle": client.registeredAt ? qsTr("Authorized on this device · %1 · Select to revoke").arg(client.registeredAt) : qsTr("Authorized on this device · Select to revoke"),
                "icon": client.clientId === "org.meo.OmniStore" ? "apps" : "extension",
                "tone": "secondary", "trailingKind": "navigation", "interactive": true
            })
        }
        if (rows.length === 0) {
            rows.push({ "id": "empty", "title": qsTr("No Meo applications are connected"),
                        "subtitle": qsTr("Applications you authorize through the system account will appear here."),
                        "icon": "link_off", "tone": "neutral", "trailingKind": "none", "interactive": false })
        }
        return rows
    }
    readonly property var sessionRows: {
        const rows = []
        const sessions = AccountBackend.sessions || []
        for (let index = 0; index < sessions.length; ++index) {
            const session = sessions[index]
            const agent = session.user_agent || qsTr("Unknown device")
            const address = session.ip || qsTr("Unknown network")
            rows.push({
                "id": session.id || ("session-" + index),
                "title": session.is_current ? qsTr("This device") : agent,
                "subtitle": session.is_current
                    ? qsTr("Current session · %1").arg(address)
                    : qsTr("%1 · Last active %2").arg(address).arg(session.updated_at || qsTr("unknown")),
                "icon": session.is_current ? "computer" : "devices",
                "tone": session.is_current ? "primary" : "neutral",
                "trailingKind": session.is_current ? "choice" : "navigation",
                "trailingText": session.is_current ? qsTr("Current") : "",
                "interactive": !session.is_current
            })
        }
        if (rows.length === 0) {
            rows.push({ "id": "empty-session", "title": qsTr("No remote sessions are available"),
                        "subtitle": qsTr("Refresh when you are online, or verify your account if requested."),
                        "icon": "cloud_off", "tone": "neutral", "trailingKind": "none", "interactive": false })
        }
        return rows
    }
    readonly property var providerRows: [
        { "id": "local-users", "title": qsTr("Local users"),
          "subtitle": qsTr("Manage Unix users for this device"), "icon": "group", "tone": "neutral",
          "route": "kcm:kcm_users", "enabled": KcmBridge.isAvailable("kcm_users"),
          "trailingKind": "choice", "trailingText": qsTr("Advanced") },
        { "id": "online-accounts", "title": qsTr("KDE online accounts"),
          "subtitle": KcmBridge.isAvailable("kcm_kaccounts") ? qsTr("Calendar, contacts, files, and provider accounts") : qsTr("The KDE online-account module is not installed"),
          "icon": "cloud", "tone": "secondary", "route": "kcm:kcm_kaccounts",
          "enabled": KcmBridge.isAvailable("kcm_kaccounts"), "trailingKind": "choice", "trailingText": qsTr("Advanced") },
        { "id": "wallet", "title": qsTr("Password wallet"),
          "subtitle": KcmBridge.isAvailable("kcm_kwallet5") ? qsTr("Protected credential storage used by KDE and Meo Account") : qsTr("The KDE wallet module is not installed"),
          "icon": "key", "tone": "tertiary", "route": "kcm:kcm_kwallet5",
          "enabled": KcmBridge.isAvailable("kcm_kwallet5"), "trailingKind": "choice", "trailingText": qsTr("Advanced") }
    ]

    function initials(name) {
        const parts = String(name || "").trim().split(/\s+/).filter(part => part.length > 0)
        if (parts.length === 0) return ""
        if (parts.length === 1) return parts[0].slice(0, 2)
        return parts[0].slice(0, 1) + parts[parts.length - 1].slice(0, 1)
    }

    function localizedRequestState(state) {
        switch (state) {
        case "waiting_for_user": return qsTr("Waiting for confirmation")
        case "opening_browser": return qsTr("Opening protected account page")
        case "waiting_for_callback": return qsTr("Waiting for browser verification")
        case "revoking_sessions": return qsTr("Revoking application sessions")
        case "refreshing": return qsTr("Refreshing identity")
        case "completed": return qsTr("Completed")
        case "failed": return qsTr("Failed — retry available")
        case "denied": return qsTr("Cancelled")
        case "expired": return qsTr("Timed out — retry available")
        default: return state
        }
    }

    function localizedSyncState(state) {
        switch (state) {
        case "loading": return qsTr("Refreshing account summary…")
        case "ready": return AccountBackend.lastSyncedAt
                              ? qsTr("Last synchronized %1").arg(AccountBackend.lastSyncedAt)
                              : qsTr("Account summary is synchronized")
        case "verification_required": return qsTr("Verification is required before synchronization can continue")
        case "retryable_error": return qsTr("Synchronization failed — check your connection and retry")
        case "not_configured": return qsTr("Cloud account synchronization is not configured")
        default: return qsTr("Account summary has not been synchronized yet")
        }
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Accounts & security")
        subtitle: qsTr("Manage the Meo Account connected to this device. Passwords and verification codes are entered only in your browser.")

        MeoCard {
            width: parent.width
            type: "filled"
            Accessible.name: root.signedIn ? qsTr("Connected Meo Account") : qsTr("Meo Account is not connected")

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
                        spacing: 4 * MeoTheme.globalScale
                        MeoText { width: parent.width; text: root.identityName; typeRole: "title"; typeSize: "medium"; emphasized: true; elide: Text.ElideRight }
                        MeoText { width: parent.width; text: AccountBackend.summary; typeRole: "body"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                        MeoText {
                            width: parent.width
                            visible: AccountBackend.requestState !== ""
                            text: qsTr("Authentication: %1").arg(root.localizedRequestState(AccountBackend.requestState))
                            typeRole: "label"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant
                        }
                    }
                }
                Row {
                    spacing: 10 * MeoTheme.globalScale
                    MeoButton {
                        text: root.signedIn ? qsTr("Verify account") : qsTr("Connect Meo Account")
                        type: "filled"
                        enabled: !AccountBackend.busy && AccountBackend.serviceRunning
                        loading: AccountBackend.busy
                        onClicked: AccountBackend.requestAuthentication(root.signedIn ? "reauthenticate" : "connect_system")
                    }
                    MeoButton {
                        text: qsTr("Refresh")
                        type: "outlined"
                        enabled: !AccountBackend.busy
                        onClicked: AccountBackend.refresh()
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !AccountBackend.serviceRunning
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "cloud_off"; size: 24; color: MeoTheme.contentOnSurfaceVariant }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: AccountBackend.settingsLauncherAvailable
                          ? qsTr("Meo Account is installed but its per-user service is unavailable. Try opening the account service or sign in again.")
                          : qsTr("Install the Meo Account system package to enable shared sign-in. Settings never stores your password or tokens.")
                    typeRole: "body"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Identity and security")
            subtitle: qsTr("Sensitive changes open the protected Meo Account page and return here when complete.")
            model: root.securityRows
            onRowActivated: (index, row) => { if (row.interactive) AccountBackend.openHostedAction(row.action) }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Active sessions")
            subtitle: root.localizedSyncState(AccountBackend.syncState)
            model: root.sessionRows
            onRowActivated: (index, row) => { if (row.interactive) AccountBackend.openHostedAction("sessions") }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Meo applications on this device")
            subtitle: qsTr("Signing out the system account also signs out every application listed here.")
            model: root.clientRows
            onRowActivated: (index, row) => { if (row.interactive) AccountBackend.revokeClient(row.id) }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Local and provider accounts")
            subtitle: qsTr("These remain under their protected system owners and are not copied into Meo Account.")
            model: root.providerRows
            onRowActivated: (index, row) => { if (row.enabled) root.navigateTo(row.route) }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: AccountBackend.error !== ""
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText { width: parent.width - 36 * MeoTheme.globalScale; text: AccountBackend.error; typeRole: "body"; typeSize: "medium"; color: MeoTheme.error; wrapMode: Text.WordWrap }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.signedIn
            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoText { text: qsTr("Danger zone"); typeRole: "title"; typeSize: "medium"; emphasized: true; color: MeoTheme.error }
                MeoText { width: parent.width; text: qsTr("Sign out this system account and every registered Meo application on this device."); typeRole: "body"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                Row {
                    spacing: 10 * MeoTheme.globalScale
                    MeoButton { text: qsTr("Sign out all on this device"); type: "outlined"; enabled: !AccountBackend.busy; onClicked: signOutDialog.open() }
                    MeoButton { text: qsTr("Delete account…"); type: "text"; onClicked: AccountBackend.openHostedAction("delete_account") }
                }
            }
        }
    }

    Dialog {
        id: signOutDialog
        modal: true
        anchors.centerIn: parent
        title: qsTr("Sign out all Meo applications?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: AccountBackend.signOutAll()
        contentItem: Label {
            width: 360 * MeoTheme.globalScale
            padding: 18 * MeoTheme.globalScale
            text: qsTr("OmniStore and every registered Meo application on this device will lose its local session. This does not sign out other devices.")
            wrapMode: Text.WordWrap
        }
    }
}
