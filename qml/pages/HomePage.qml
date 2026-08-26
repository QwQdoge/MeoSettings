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

    // This mirrors the familiar search-first Android settings hierarchy while
    // retaining Meo's actual category routes and live service summaries.
    // Names such as "Display & touch" are navigation labels only; the target
    // page still exposes only the desktop controls it can verify and write.
    readonly property var referenceRows: {
        const rows = [{
            "title": qsTr("Network & Internet"),
            "subtitle": NetworkBackend.connectionName || (NetworkBackend.wifiEnabled ? qsTr("Wi-Fi on, not connected") : qsTr("Wi-Fi off or unavailable")),
            "icon": "wifi", "tone": "primary", "route": "category:network", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Connected devices"),
            "subtitle": BluetoothBackend.enabled ? qsTr("Bluetooth on") : qsTr("Bluetooth off or unavailable"),
            "icon": "devices", "tone": "primary", "route": "category:devices", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Apps & notifications"),
            "subtitle": root.doNotDisturbEnabled ? qsTr("Do Not Disturb is on") : qsTr("Notifications and default apps"),
            "icon": "apps", "tone": "secondary", "route": "category:apps", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Notifications"),
            "subtitle": root.doNotDisturbEnabled ? qsTr("Do Not Disturb is on")
                                                : qsTr("Popups and focus"),
            "icon": root.doNotDisturbEnabled ? "do_not_disturb_on" : "notifications",
            "tone": "secondary", "route": "notifications", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Sound & vibration"),
            "subtitle": AudioBackend.outputName || qsTr("Choose an output"),
            "icon": "volume_up", "tone": "primary", "route": "sound", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Control Center"),
            "subtitle": qsTr("Quick Settings tiles, layout, and density"),
            "icon": "tune", "tone": "secondary", "route": "control-center", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Display & touch"),
            "subtitle": qsTr("Brightness, Night Light, displays, and text"),
            "icon": "monitor", "tone": "tertiary", "route": "display", "trailingKind": "navigation"
        },
        {
            "title": qsTr("Wallpaper & style"),
            "subtitle": qsTr("Dynamic color, wallpaper, icons, and fonts"),
            "icon": "palette", "tone": "tertiary", "route": "appearance", "trailingKind": "navigation"
        }]
        return rows
    }

    readonly property var systemRows: [{
        "title": qsTr("Storage"),
        "subtitle": root.storageSummary(),
        "icon": "storage", "tone": "secondary", "route": "storage", "trailingKind": "navigation"
    },
    {
        "title": qsTr("Power & battery"),
        "subtitle": root.powerSummary(),
        "icon": PowerBackend.charging ? "battery_charging_full" : "battery_full",
        "tone": "primary", "route": "power", "trailingKind": "navigation"
    },
    root.categoryRow("system"),
    root.categoryRow("privacy"),
    root.categoryRow("accessibility"),
    root.categoryRow("updates"),
    {
        "title": qsTr("About Meo"),
        "subtitle": qsTr("MeoArch, hardware, and runtime information"),
        "icon": "info", "tone": "neutral", "route": "about", "trailingKind": "navigation"
    }]

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
        compactWidth: Math.min(680 * MeoTheme.globalScale, MeoTheme.settingsContentMaxWidth)
        mediumWidth: MeoTheme.settingsContentMaxWidth
        expandedWidth: MeoTheme.settingsContentMaxWidth
        // The desktop product and Pixel reference are search-first.  A second
        // in-page title duplicated the window title and shifted every measured
        // surface downward, so Home starts directly with the 64 dp search.
        title: ""
        subtitle: ""
        topPadding: 0

        MeoSearchBar {
            id: searchBar
            width: parent.width
            placeholder: qsTr("Search settings")
            trailingIcon: ""
            visualStyle: "settings"
            Accessible.name: qsTr("Search settings")
        }

        // The broker is the only identity authority here.  Once it reports a
        // signed-in Meo Account, every Settings identity surface uses its
        // scoped cloud name/avatar rather than silently mixing it with the
        // local Unix profile.  A denied profile ID remains absent by design.
        MeoSettingsAccountCard {
            width: parent.width
            visible: !root.searching
            title: root.accountDisplayName
            subtitle: root.accountSummary
            avatarSource: root.accountAvatarSource
            initials: root.initials(root.accountDisplayName)
            Accessible.name: root.showingCloudIdentity ? qsTr("Meo Account and device")
                                                        : qsTr("Local user and device")
            onClicked: root.navigateTo("accounts")
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.searching && root.searchRows.length > 0
            title: qsTr("Search results")
            subtitle: ""
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
            title: ""
            subtitle: ""
            model: root.referenceRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !root.searching
            title: ""
            subtitle: ""
            model: root.systemRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }
    }
}
