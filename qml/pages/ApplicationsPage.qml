import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedApp: ({})
    property string pendingAction: ""
    property string requestedAppId: ""
    property string requestedAppName: ""
    property string requestedSection: ""
    property bool requestHandled: false
    property bool dataViewReady: false
    property int visibleLimit: 60
    property int pageStep: 60
    property bool hasMoreApplications: false
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property var selectedCapabilities: selectedApp.capabilities || ({})
    readonly property var selectedSettings: selectedApp.settings || ({})
    // "name", "size" or "source"
    property string sortMode: "name"
    readonly property real totalCacheBytes: {
        let total = 0
        const apps = OmniStoreAppsBackend.applications || []
        for (let index = 0; index < apps.length; ++index) {
            const storage = apps[index].storage || []
            for (let row = 0; row < storage.length; ++row)
                if (storage[row].id === "cache")
                    total += Number(storage[row].bytes) || 0
        }
        return total
    }
    function appTotalBytes(app) {
        return (Number(app.packageSizeBytes) || 0) + (Number(app.storageBytes) || 0)
    }
    function cacheLimitLabel(megabytes) {
        if (megabytes <= 0)
            return qsTr("Off")
        return megabytes >= 1024
               ? qsTr("Above %1 GB").arg(megabytes / 1024)
               : qsTr("Above %1 MB").arg(megabytes)
    }
    function openAppSettings() {
        OmniStorePreferences.refresh()
        UpdateScheduleBackend.refresh()
        appSettings.open()
    }

    function formatBytes(value) {
        const bytes = Math.max(0, Number(value) || 0)
        const units = ["B", "KiB", "MiB", "GiB", "TiB"]
        let amount = bytes
        let unit = 0
        while (amount >= 1024 && unit < units.length - 1) {
            amount /= 1024
            ++unit
        }
        return unit === 0 ? Math.round(amount) + " " + units[unit]
                          : amount.toFixed(amount >= 10 ? 0 : 1) + " " + units[unit]
    }

    function sourceIcon(sourceId) {
        switch (String(sourceId || "").toLowerCase()) {
        case "flatpak": return "deployed_code"
        case "appimage": return "rocket_launch"
        case "aur": return "extension"
        case "pacman": return "terminal"
        default: return "apps"
        }
    }

    function sourceTone(sourceId) {
        switch (String(sourceId || "").toLowerCase()) {
        case "flatpak": return "secondary"
        case "appimage": return "tertiary"
        case "aur": return "tertiary"
        case "pacman": return "primary"
        default: return "neutral"
        }
    }

    function categoryTitle(category) {
        switch (category) {
        case "config": return qsTr("Configuration")
        case "cache": return qsTr("Cache")
        case "data": return qsTr("App data")
        case "state": return qsTr("State")
        default: return category
        }
    }

    function categoryIcon(category) {
        switch (category) {
        case "config": return "tune"
        case "cache": return "cached"
        case "data": return "folder"
        case "state": return "history"
        default: return "folder"
        }
    }

    function appSubtitle(app) {
        let text = app.sourceName || qsTr("Unknown source")
        if (app.version)
            text += " · " + app.version
        const managedBytes = Number(app.storageBytes) || 0
        const packageBytes = Number(app.packageSizeBytes) || 0
        if (packageBytes > 0 || managedBytes > 0)
            text += qsTr(" · %1 used").arg(formatBytes(packageBytes + managedBytes))
        return text
    }

    // Build only a bounded window of rows. The old page materialised the whole
    // application list during construction, which amplified a large OmniStore
    // export into a multi-second QML freeze. Search still walks the source list,
    // but expensive row delegates are limited to visibleLimit.
    readonly property var filteredApplications: {
        if (!dataViewReady) {
            root.hasMoreApplications = false
            return []
        }

        const query = searchBar.text.trim().toLowerCase()
        let apps = OmniStoreAppsBackend.applications || []
        if (root.sortMode === "size") {
            apps = apps.slice(0).sort(function(a, b) { return root.appTotalBytes(b) - root.appTotalBytes(a) })
        } else if (root.sortMode === "source") {
            apps = apps.slice(0).sort(function(a, b) {
                const bySource = String(a.sourceName || "").localeCompare(String(b.sourceName || ""))
                return bySource !== 0 ? bySource : String(a.name || "").localeCompare(String(b.name || ""))
            })
        }
        const rows = []
        let more = false
        for (let index = 0; index < apps.length; ++index) {
            const app = apps[index]
            const haystack = (String(app.name || "") + " "
                              + String(app.id || "") + " "
                              + String(app.sourceName || "")).toLowerCase()
            if (query !== "" && haystack.indexOf(query) < 0)
                continue
            if (rows.length >= root.visibleLimit) {
                more = true
                break
            }
            rows.push({
                "title": app.name || app.id,
                "subtitle": root.appSubtitle(app),
                "icon": root.sourceIcon(app.sourceId),
                "tone": root.sourceTone(app.sourceId),
                "trailingKind": root.sortMode === "size" && root.appTotalBytes(app) > 0 ? "status" : "navigation",
                "trailingText": root.formatBytes(root.appTotalBytes(app)),
                "app": app
            })
        }
        root.hasMoreApplications = more
        return rows
    }

    readonly property var appInfoRows: [
        {
            "title": qsTr("Package source"),
            "subtitle": selectedApp.sourceName || qsTr("Unknown"),
            "icon": root.sourceIcon(selectedApp.sourceId),
            "tone": root.sourceTone(selectedApp.sourceId),
            "trailingKind": "none",
            "interactive": false
        },
        {
            "title": qsTr("Version"),
            "subtitle": selectedApp.version || qsTr("Not reported"),
            "icon": "info",
            "tone": "neutral",
            "trailingKind": "none",
            "interactive": false
        },
        {
            "title": qsTr("Application ID"),
            "subtitle": selectedApp.id || qsTr("Unavailable"),
            "icon": "fingerprint",
            "tone": "neutral",
            "trailingKind": "none",
            "interactive": false
        },
        {
            "title": qsTr("Installed package files"),
            "subtitle": selectedApp.packageSizeBytes
                        ? root.formatBytes(selectedApp.packageSizeBytes)
                        : qsTr("Size not reported"),
            "icon": "inventory_2",
            "tone": "neutral",
            "trailingKind": "none",
            "interactive": false
        }
    ]

    readonly property var configurationPathRows: {
        const rows = []
        const storage = selectedApp.storage || []
        for (let index = 0; index < storage.length; ++index) {
            const item = storage[index]
            if (item.id !== "config")
                continue
            const paths = item.paths || []
            rows.push({
                "title": qsTr("Verified configuration"),
                "subtitle": paths.length > 0 ? paths.join(" · ") : qsTr("Application configuration location"),
                "icon": "folder",
                "tone": "primary",
                "trailingKind": "status",
                "trailingText": root.formatBytes(item.bytes),
                "interactive": false
            })
        }
        if (rows.length === 0) {
            rows.push({
                "title": qsTr("No verified .config location"),
                "subtitle": qsTr("Meo does not guess configuration paths. Trusted OmniStore metadata is required."),
                "icon": "folder_off",
                "tone": "neutral",
                "trailingKind": "none",
                "interactive": false
            })
        }
        return rows
    }

    readonly property var storageRows: {
        const rows = []
        const storage = selectedApp.storage || []
        for (let index = 0; index < storage.length; ++index) {
            const item = storage[index]
            if (item.id === "config")
                continue
            const paths = item.paths || []
            rows.push({
                "title": root.categoryTitle(item.id),
                "subtitle": paths.length > 0 ? paths.join(" · ") : qsTr("App-scoped location"),
                "icon": root.categoryIcon(item.id),
                "tone": item.id === "cache" ? "secondary" : "neutral",
                "trailingKind": "status",
                "trailingText": root.formatBytes(item.bytes),
                "interactive": false
            })
        }
        if (rows.length === 0) {
            rows.push({
                "title": qsTr("No additional app storage"),
                "subtitle": qsTr("No verified cache, data, or state location is registered for this application."),
                "icon": "folder_off",
                "tone": "neutral",
                "trailingKind": "none",
                "interactive": false
            })
        }
        return rows
    }

    function openApp(app) {
        selectedApp = app
        appDetails.open()
    }

    function appMatchesRequest(app) {
        const requestedId = requestedAppId.trim().toLowerCase()
        const requestedName = requestedAppName.trim().toLowerCase()
        const appId = String(app.id || "").toLowerCase()
        const appName = String(app.name || "").toLowerCase()
        if (requestedId !== "" && appId === requestedId)
            return true
        return requestedName !== "" && appName === requestedName
    }

    function handleRequestedApp() {
        if (!dataViewReady || requestHandled
                || (requestedAppId.trim() === "" && requestedAppName.trim() === ""))
            return
        const apps = OmniStoreAppsBackend.applications || []
        for (let index = 0; index < apps.length; ++index) {
            const app = apps[index]
            if (!appMatchesRequest(app))
                continue
            requestHandled = true
            openApp(app)
            return
        }
        searchBar.text = requestedAppName.trim() !== "" ? requestedAppName : requestedAppId
    }

    function requestAction(action) {
        pendingAction = action
        actionConfirmation.open()
    }

    function actionTitle(action) {
        switch (action) {
        case "clear-cache": return qsTr("Clear cache?")
        case "reset-settings": return qsTr("Reset app settings?")
        case "clear-data": return qsTr("Delete app data?")
        case "uninstall": return qsTr("Uninstall application?")
        default: return qsTr("Manage application?")
        }
    }

    function actionDescription(action) {
        switch (action) {
        case "clear-cache": return qsTr("Temporary files registered for %1 will be removed.").arg(selectedApp.name || "")
        case "reset-settings": return qsTr("Verified configuration for %1 will be reset.").arg(selectedApp.name || "")
        case "clear-data": return qsTr("Registered local app data for %1 will be removed. This can sign you out.").arg(selectedApp.name || "")
        case "uninstall": return qsTr("%1 will be removed through OmniStore. App data is kept unless removed separately.").arg(selectedApp.name || "")
        default: return ""
        }
    }

    function executePendingAction() {
        const id = String(selectedApp.id || "")
        const source = String(selectedApp.sourceId || "")
        switch (pendingAction) {
        case "clear-cache": OmniStoreAppsBackend.clearCache(id, source); break
        case "reset-settings": OmniStoreAppsBackend.resetSettings(id, source); break
        case "clear-data": OmniStoreAppsBackend.clearData(id, source); break
        case "uninstall": OmniStoreAppsBackend.uninstall(id, source); break
        }
    }

    function refreshSelectedApp() {
        if (!selectedApp.id)
            return
        const apps = OmniStoreAppsBackend.applications || []
        for (let index = 0; index < apps.length; ++index) {
            if (apps[index].id === selectedApp.id && apps[index].sourceId === selectedApp.sourceId) {
                selectedApp = apps[index]
                return
            }
        }
    }

    function maybeExtendWindow() {
        if (!dataViewReady || !hasMoreApplications)
            return
        const remaining = page.contentHeight - (page.contentY + page.height)
        if (remaining < Math.max(700 * MeoTheme.globalScale, page.height * 0.9))
            visibleLimit += pageStep
    }

    Connections {
        target: OmniStoreAppsBackend
        function onChanged() {
            root.refreshSelectedApp()
            root.handleRequestedApp()
        }
        function onActionFinished(action, appId, success) {
            if (success && action === "uninstall" && root.selectedApp.id === appId) {
                appDetails.close()
                root.selectedApp = ({})
            }
        }
    }

    // Let the shell/title/status card paint before application projection and
    // any exporter refresh work are requested. A slow backend now means a
    // loading card inside a responsive page rather than an unresponsive route.
    Timer {
        interval: 1
        repeat: false
        running: true
        onTriggered: {
            root.dataViewReady = true
            if (!OmniStoreAppsBackend.busy && (OmniStoreAppsBackend.applications || []).length === 0
                    && OmniStoreAppsBackend.exporterAvailable)
                OmniStoreAppsBackend.refresh()
            root.handleRequestedApp()
        }
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: MeoTheme.settingsContentMaxWidth
        title: root.isCompact ? "" : qsTr("Applications")
        subtitle: qsTr("Choose an app to see its storage, clear its cache or remove it. The gear opens update and cache settings for all apps.")
        onContentYChanged: root.maybeExtendWindow()
        onHeightChanged: root.maybeExtendWindow()

        // List first: search, sort and the app-wide settings sit in one row
        // above the list; maintenance details live behind the gear button.
        Item {
            width: parent.width
            height: searchBar.implicitHeight

            MeoSearchBar {
                id: searchBar
                anchors.left: parent.left
                anchors.right: toolbarButtons.left
                anchors.rightMargin: MeoTheme.space8
                anchors.verticalCenter: parent.verticalCenter
                placeholder: qsTr("Search installed apps")
                trailingIcon: ""
                visualStyle: "settings"
                Accessible.name: qsTr("Search installed apps")
                onTextChanged: root.visibleLimit = root.pageStep
            }

            Row {
                id: toolbarButtons
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: MeoTheme.space4

                MeoIconButton {
                    icon.name: "refresh"
                    type: "standard"
                    toolTipText: qsTr("Refresh")
                    enabled: !OmniStoreAppsBackend.busy
                    onClicked: OmniStoreAppsBackend.refresh()
                }
                MeoIconButton {
                    icon.name: "settings"
                    type: "tonal"
                    toolTipText: qsTr("App settings")
                    onClicked: root.openAppSettings()
                }
            }
        }

        Flow {
            width: parent.width
            spacing: MeoTheme.space8

            Repeater {
                model: [
                    { "id": "name", "label": qsTr("Name") },
                    { "id": "size", "label": qsTr("Size") },
                    { "id": "source", "label": qsTr("Source") }
                ]
                delegate: MeoChip {
                    required property var modelData
                    type: "filter"
                    label: modelData.label
                    selected: root.sortMode === modelData.id
                    onClicked: root.sortMode = modelData.id
                }
            }
        }

        MeoProgressBar {
            width: parent.width
            visible: OmniStoreAppsBackend.busy
            type: "linear"
            indeterminate: true
        }

        MeoText {
            width: parent.width
            text: OmniStoreAppsBackend.error !== ""
                  ? OmniStoreAppsBackend.error
                  : (OmniStoreAppsBackend.busy
                     ? qsTr("Loading installed applications in the background…")
                     : OmniStoreAppsBackend.summary)
            typeRole: "body"
            typeSize: "small"
            color: OmniStoreAppsBackend.error !== "" ? MeoTheme.error : MeoTheme.contentOnSurfaceVariant
            wrapMode: Text.WordWrap
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.dataViewReady && root.filteredApplications.length > 0
            title: qsTr("Installed applications")
            subtitle: root.hasMoreApplications
                      ? qsTr("Showing the first %1 matches. Scroll to load more.").arg(root.filteredApplications.length)
                      : qsTr("%n application(s) shown", "", root.filteredApplications.length)
            model: root.filteredApplications
            onRowActivated: (index, row) => root.openApp(row.app)
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.hasMoreApplications
            implicitHeight: moreColumn.implicitHeight + 24 * MeoTheme.globalScale
            Column {
                id: moreColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12 * MeoTheme.globalScale
                spacing: 8 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("More applications are available")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                }
                MeoButton {
                    text: qsTr("Load more")
                    type: "text"
                    onClicked: root.visibleLimit += root.pageStep
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 230 * MeoTheme.globalScale
            visible: root.dataViewReady && !OmniStoreAppsBackend.busy && root.filteredApplications.length === 0
            icon: searchBar.text.trim() === "" ? "apps" : "search_off"
            title: searchBar.text.trim() === "" ? qsTr("No applications reported") : qsTr("No matching applications")
            description: searchBar.text.trim() === ""
                         ? qsTr("Install or update OmniStore, then refresh this page.")
                         : qsTr("Try the application name, package ID, or source.")
        }
    }

    MeoSettingsTaskSheet {
        id: appDetails
        popupParent: Overlay.overlay
        title: root.selectedApp.name || qsTr("Application")
        subtitle: root.selectedApp.sourceName || qsTr("Managed application")
        rejectText: qsTr("Close")
        content: Component {
            Column {
                width: parent.width
                spacing: 16 * MeoTheme.globalScale

                MeoSettingsGroup {
                    width: parent.width
                    title: qsTr("App info")
                    model: root.appInfoRows
                }

                MeoSettingsGroup {
                    width: parent.width
                    title: qsTr("Configuration (.config)")
                    subtitle: qsTr("Only paths verified by OmniStore are exposed.")
                    model: root.configurationPathRows
                }

                MeoSettingsGroup {
                    width: parent.width
                    visible: root.selectedSettings.available === true
                    title: qsTr("App-provided settings")
                    model: [{
                        "title": root.selectedSettings.label || qsTr("Application configuration"),
                        "subtitle": qsTr("Provided by %1").arg(root.selectedSettings.provider || qsTr("OmniStore")),
                        "icon": "settings",
                        "tone": "primary",
                        "trailingKind": root.selectedSettings.route ? "navigation" : "none",
                        "route": root.selectedSettings.route || ""
                    }]
                    onRowActivated: (index, row) => {
                        if (row.route) {
                            appDetails.close()
                            root.navigateTo(row.route)
                        }
                    }
                }

                MeoSettingsGroup {
                    width: parent.width
                    title: qsTr("Storage & data")
                    subtitle: selectedApp.storageComplete === false
                              ? qsTr("Some bounded scans may be partial.")
                              : qsTr("Only verified app-scoped locations are shown.")
                    model: root.storageRows
                }

                Flow {
                    width: parent.width
                    spacing: 8 * MeoTheme.globalScale
                    MeoButton {
                        text: qsTr("Clear cache")
                        type: "tonal"
                        visible: root.selectedCapabilities.clearCache === true
                        enabled: !OmniStoreAppsBackend.busy
                        onClicked: root.requestAction("clear-cache")
                    }
                    MeoButton {
                        text: qsTr("Reset settings")
                        type: "outlined"
                        visible: root.selectedCapabilities.resetSettings === true
                        enabled: !OmniStoreAppsBackend.busy
                        onClicked: root.requestAction("reset-settings")
                    }
                    MeoButton {
                        text: qsTr("Delete app data")
                        type: "outlined"
                        visible: root.selectedCapabilities.clearData === true
                        enabled: !OmniStoreAppsBackend.busy
                        onClicked: root.requestAction("clear-data")
                    }
                    MeoButton {
                        text: qsTr("Uninstall")
                        type: "outlined"
                        visible: root.selectedCapabilities.uninstall === true
                        enabled: OmniStoreAppsBackend.managerAvailable && !OmniStoreAppsBackend.busy
                        onClicked: root.requestAction("uninstall")
                    }
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: appSettings
        popupParent: Overlay.overlay
        title: qsTr("App settings")
        subtitle: qsTr("Updates and storage for every app managed by OmniStore")
        rejectText: qsTr("Close")
        readonly property var scheduleChoices: UpdateScheduleBackend.choices
        readonly property var cacheChoices: (OmniStorePreferences.cacheLimitChoicesMb || []).map(function(value) {
            return { "label": root.cacheLimitLabel(value), "value": value }
        })
        function indexOf(options, value) {
            for (let index = 0; index < options.length; ++index)
                if (options[index].value === value)
                    return index
            return -1
        }
        content: Component {
            Column {
                width: parent.width
                spacing: 16 * MeoTheme.globalScale

                MeoText {
                    width: parent.width
                    visible: OmniStorePreferences.available && !OmniStorePreferences.supported
                             && !OmniStorePreferences.busy
                    text: qsTr("Update OmniStore to change automatic updates and cache cleanup here.")
                    typeRole: "body"; typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                MeoText {
                    width: parent.width
                    visible: OmniStorePreferences.error !== "" || UpdateScheduleBackend.error !== ""
                    text: OmniStorePreferences.error !== "" ? OmniStorePreferences.error : UpdateScheduleBackend.error
                    typeRole: "body"; typeSize: "small"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }

                MeoSettingsGroup {
                    width: parent.width
                    title: qsTr("Updates")
                    model: {
                        const rows = []
                        if (UpdateScheduleBackend.available)
                            rows.push({
                                "id": "frequency",
                                "title": qsTr("Check for updates"),
                                "subtitle": qsTr("How often OmniStore looks for new versions in the background"),
                                "icon": "update", "tone": "primary",
                                "trailingKind": "dropdown",
                                "options": appSettings.scheduleChoices,
                                "currentIndex": appSettings.indexOf(appSettings.scheduleChoices, UpdateScheduleBackend.intervalHours)
                            })
                        if (OmniStorePreferences.supported) {
                            rows.push({
                                "id": "autoUpdate",
                                "title": qsTr("Update apps automatically"),
                                "subtitle": qsTr("Flatpak apps update in the background. System packages always wait for you in OmniStore."),
                                "icon": "system_update_alt", "tone": "primary",
                                "trailingKind": "toggle",
                                "checked": OmniStorePreferences.autoUpdate,
                                "enabled": UpdateScheduleBackend.intervalHours !== 0
                            })
                            rows.push({
                                "id": "notify",
                                "title": qsTr("Notify me about updates"),
                                "icon": "notifications", "tone": "secondary",
                                "trailingKind": "toggle",
                                "checked": OmniStorePreferences.notifyUpdates,
                                "enabled": UpdateScheduleBackend.intervalHours !== 0
                            })
                        }
                        return rows
                    }
                    onRowDropdownSelected: (index, optionIndex, value, row) =>
                        UpdateScheduleBackend.setIntervalHours(appSettings.scheduleChoices[optionIndex].value)
                    onRowToggled: (index, checked, row) => {
                        if (row.id === "autoUpdate")
                            OmniStorePreferences.setAutoUpdate(checked)
                        else if (row.id === "notify")
                            OmniStorePreferences.setNotifyUpdates(checked)
                    }
                }

                MeoSettingsGroup {
                    width: parent.width
                    visible: OmniStorePreferences.supported
                    title: qsTr("Storage")
                    subtitle: qsTr("App caches currently use %1").arg(root.formatBytes(root.totalCacheBytes))
                    model: [
                        {
                            "id": "cacheLimit",
                            "title": qsTr("Clean app caches automatically"),
                            "subtitle": qsTr("When app caches together grow past the limit, the largest are cleared first. Settings and data are never removed."),
                            "icon": "cleaning_services", "tone": "tertiary",
                            "trailingKind": "dropdown",
                            "options": appSettings.cacheChoices,
                            "currentIndex": appSettings.indexOf(appSettings.cacheChoices, OmniStorePreferences.cacheLimitMb)
                        },
                        {
                            "id": "trimNow",
                            "title": qsTr("Clean up now"),
                            "subtitle": OmniStorePreferences.lastResult !== ""
                                        ? OmniStorePreferences.lastResult
                                        : qsTr("Bring app caches under the limit right away"),
                            "icon": "delete_sweep", "tone": "tertiary",
                            "trailingKind": "action",
                            "actionText": OmniStorePreferences.busy ? qsTr("Cleaning…") : qsTr("Clean"),
                            "enabled": OmniStorePreferences.cacheLimitMb > 0 && !OmniStorePreferences.busy
                        }
                    ]
                    onRowDropdownSelected: (index, optionIndex, value, row) =>
                        OmniStorePreferences.setCacheLimitMb(appSettings.cacheChoices[optionIndex].value)
                    onRowActionTriggered: (index, row) => {
                        if (row.id === "trimNow")
                            OmniStorePreferences.trimCachesNow()
                    }
                }

                MeoSettingsGroup {
                    width: parent.width
                    title: qsTr("OmniStore")
                    model: [{
                        "id": "open",
                        "title": qsTr("Open OmniStore"),
                        "subtitle": qsTr("Install apps, review updates and manage sources"),
                        "icon": "storefront", "tone": "primary",
                        "trailingKind": "chevron",
                        "enabled": OmniStoreAppsBackend.launcherAvailable
                    }]
                    onRowActivated: (index, row) => OmniStoreAppsBackend.openOmniStore()
                }
            }
        }
    }

    Connections {
        target: OmniStorePreferences
        function onCachesTrimmed(bytesFreed) {
            if (bytesFreed > 0)
                OmniStoreAppsBackend.refresh()
        }
    }

    MeoSettingsTaskSheet {
        id: actionConfirmation
        popupParent: Overlay.overlay
        title: root.actionTitle(root.pendingAction)
        subtitle: root.actionDescription(root.pendingAction)
        acceptText: root.pendingAction === "clear-data" || root.pendingAction === "uninstall"
                    ? qsTr("Continue") : qsTr("Confirm")
        rejectText: qsTr("Cancel")
        onAccepted: root.executePendingAction()
    }
}
