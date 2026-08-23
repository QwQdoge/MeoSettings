import QtQuick
import QtQuick.Controls
import MeoUI
import org.kde.notificationmanager as NotificationManager
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool searching: searchBar.text.trim().length > 0
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property bool doNotDisturbEnabled: {
        const until = notificationSettings.notificationsInhibitedUntil
        const timestamp = until instanceof Date ? until.getTime() : new Date(until).getTime()
        return !isNaN(timestamp) && timestamp > Date.now()
    }

    // Read the same persisted Plasma preference that the dedicated
    // Notifications page edits.  Home never writes this object; it only makes
    // a frequent daily state discoverable at a glance.
    NotificationManager.Settings {
        id: notificationSettings
        live: true
    }

    function initials(name) {
        const parts = String(name || "").trim().split(/\s+/).filter(part => part.length > 0)
        if (parts.length === 0)
            return ""
        if (parts.length === 1)
            return parts[0].slice(0, 2)
        return parts[0].slice(0, 1) + parts[parts.length - 1].slice(0, 1)
    }

    readonly property bool showingCloudIdentity: AccountBackend.signedIn
    readonly property string accountDisplayName: showingCloudIdentity
                                                 ? (AccountBackend.cloudName || qsTr("Meo Account"))
                                                 : (SystemInfoBackend.userName || qsTr("Local user"))
    readonly property string accountAvatarSource: showingCloudIdentity
                                                ? AccountBackend.cloudAvatarSource
                                                : SystemInfoBackend.userAvatarSource
    readonly property string accountSummary: {
        if (!showingCloudIdentity) {
            return SystemInfoBackend.deviceName !== ""
                   ? qsTr("Local session · %1").arg(SystemInfoBackend.deviceName)
                   : qsTr("Local session")
        }
        if (AccountBackend.cloudId !== "")
            return qsTr("Meo Account · %1").arg(AccountBackend.cloudId)
        return qsTr("Meo Account")
    }

    function categoryRow(categoryId) {
        const category = SettingsRegistry.category(categoryId)
        return {
            "title": category.title || "",
            "subtitle": category.description || "",
            "icon": category.icon || "settings",
            "tone": category.tone || "primary",
            "route": category.route || "home",
            "trailingKind": "navigation"
        }
    }

    function powerSummary() {
        if (PowerBackend.available)
            return PowerBackend.summary
        if (Platform.powerProfilesAvailable) {
            if (Platform.activePowerProfile === "performance")
                return qsTr("Performance mode")
            if (Platform.activePowerProfile === "power-saver")
                return qsTr("Power saver mode")
            if (Platform.activePowerProfile === "balanced")
                return qsTr("Balanced mode")
            return Platform.activePowerProfile
        }
        return qsTr("Profiles, battery state, and screen lock")
    }

    function storageSummary() {
        if (OmniStoreAppsBackend.available && OmniStoreAppsBackend.applicationCount > 0) {
            return qsTr("%1 · %2")
                    .arg(StorageBackend.summary)
                    .arg(OmniStoreAppsBackend.summary)
        }
        return StorageBackend.summary
    }

    readonly property var quickRows: {
        const rows = [{
            "title": qsTr("Wi-Fi"),
            "subtitle": NetworkBackend.connectionName || (NetworkBackend.wifiEnabled ? qsTr("On, not connected") : qsTr("Off or unavailable")),
            "icon": "wifi", "tone": "primary", "route": "wifi", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Bluetooth"),
            "subtitle": BluetoothBackend.enabled ? qsTr("On") : qsTr("Off or unavailable"),
            "icon": "bluetooth", "tone": "secondary", "route": "bluetooth", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Sound"),
            "subtitle": AudioBackend.outputName || qsTr("Choose an output"),
            "icon": "volume_up", "tone": "secondary", "route": "sound", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Notifications"),
            "subtitle": root.doNotDisturbEnabled ? qsTr("Do Not Disturb is on")
                                                : qsTr("Popups and focus"),
            "icon": root.doNotDisturbEnabled ? "do_not_disturb_on" : "notifications",
            "tone": "primary", "route": "notifications", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Storage & apps"),
            "subtitle": root.storageSummary(),
            "icon": "storage", "tone": "tertiary", "route": "storage", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Power & battery"),
            "subtitle": root.powerSummary(),
            "icon": PowerBackend.charging ? "battery_charging_full" : "battery_full",
            "tone": "primary", "route": "power", "trailingKind": "navigation"
        }]
        return rows
    }

    readonly property var searchRows: {
        const rows = []
        const results = SettingsRegistry.search(searchBar.text)
        for (let index = 0; index < results.length; ++index) {
            const entry = results[index]
            if (!root.capabilityAvailable(entry.capability))
                continue
            rows.push({
                "title": entry.title,
                "subtitle": entry.category + " · " + entry.description,
                "icon": entry.icon,
                "tone": entry.tone || "primary",
                "route": entry.route,
                "trailingKind": entry.authority === "kde" ? "choice" : "navigation",
                "trailingText": entry.authority === "kde" ? qsTr("Advanced") : ""
            })
        }
        return rows
    }

    function capabilityAvailable(capability) {
        switch (capability || "") {
        case "wifi": return Capabilities.wifi
        case "bluetooth": return Capabilities.bluetooth
        case "audio": return Capabilities.audio
        case "display": return Capabilities.display
        default: return true
        }
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Settings")
        subtitle: root.searching
                  ? qsTr("Results are grouped by their system owner.")
                  : qsTr("Search first, then move through one clear category at a time.")

        MeoSearchBar {
            id: searchBar
            width: parent.width
            placeholder: qsTr("Search settings")
            trailingIcon: ""
            Accessible.name: qsTr("Search settings")
        }

        // The broker is the only identity authority here.  Once it reports a
        // signed-in Meo Account, every Settings identity surface uses its
        // scoped cloud name/avatar rather than silently mixing it with the
        // local Unix profile.  A denied profile ID remains absent by design.
        MeoCard {
            width: parent.width
            visible: !root.searching
            type: "filled"
            compact: root.isCompact
            interactive: true
            Accessible.name: root.showingCloudIdentity ? qsTr("Meo Account and device")
                                                        : qsTr("Local user and device")
            onClicked: root.navigateTo("accounts")

            Row {
                width: parent.width
                spacing: 14 * MeoTheme.globalScale

                MeoAvatar {
                    anchors.verticalCenter: parent.verticalCenter
                    source: root.accountAvatarSource
                    initials: root.initials(root.accountDisplayName)
                    size: root.isCompact ? 44 : 52
                    color: MeoTheme.primaryContainer
                    textColor: MeoTheme.contentOnPrimaryContainer
                }

                Column {
                    width: parent.width - (root.isCompact ? 44 : 52) * MeoTheme.globalScale
                           - parent.spacing - accountChevron.width
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        text: root.accountDisplayName
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                        elide: Text.ElideRight
                    }

                    MeoText {
                        width: parent.width
                        text: root.accountSummary
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        elide: Text.ElideRight
                    }
                }

                MeoIcon {
                    id: accountChevron
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "chevron_right"
                    size: 24
                    color: MeoTheme.contentOnSurfaceVariant
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.searching && root.searchRows.length > 0
            title: qsTr("Search results")
            subtitle: qsTr("A result opens its Meo page, or an explicitly marked protected advanced tool when necessary.")
            model: root.searchRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoEmptyState {
            width: parent.width
            height: 220 * MeoTheme.globalScale
            visible: root.searching && root.searchRows.length === 0
            icon: "search_off"
            title: qsTr("No settings found")
            description: qsTr("Try a device, feature, or system term such as Wi-Fi, storage, display, or notifications.")
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching
            title: qsTr("Frequent settings")
            subtitle: qsTr("Live status from the services that Meo Settings can safely manage.")
            model: root.quickRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        // This is a real session fact, not an imitation system-update banner:
        // it makes the active platform color source visible and inspectable.
        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching && MeoTheme.colorSchemeMode === "dynamic"
            title: qsTr("Appearance")
            model: [{
                "title": qsTr("Dynamic color"),
                "subtitle": qsTr("Using the complete Material color scheme from the active KDE session."),
                "icon": "colors",
                "tone": "tertiary",
                "trailingKind": "status",
                "trailingText": qsTr("Active"),
                "interactive": false
            }]
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching
            title: qsTr("Connections")
            model: [root.categoryRow("network"), root.categoryRow("devices"), root.categoryRow("display-sound")]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching
            title: qsTr("Personal")
            model: [root.categoryRow("personalization"), root.categoryRow("apps"), root.categoryRow("accounts")]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching
            title: qsTr("System")
            model: [root.categoryRow("storage"), root.categoryRow("system"), root.categoryRow("privacy"),
                    root.categoryRow("accessibility"), root.categoryRow("updates"), {
                "title": qsTr("About"),
                "subtitle": qsTr("MeoArch, hardware, and runtime information"),
                "icon": "info",
                "tone": "neutral",
                "route": "about",
                "trailingKind": "navigation"
            }]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }
    }
}
