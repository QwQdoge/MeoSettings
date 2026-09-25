import QtQuick
import QtQuick.Controls
import MeoUI
import MeoKDE 1.0

ApplicationWindow {
    id: root

    width: 1280 * MeoTheme.globalScale
    height: 820 * MeoTheme.globalScale
    minimumWidth: 420 * MeoTheme.globalScale
    minimumHeight: 560 * MeoTheme.globalScale
    visible: true
    title: qsTr("Meo Settings")
    color: MeoTheme.surface

    property string currentRoute: "home"
    property string lastLoadedRoute: ""
    // A Settings session is required to consume the complete platform HCT
    // palette before it shows content.  Fixed MeoUI colors remain available
    // only for non-session previews, not as a silent product fallback.
    readonly property bool dynamicThemeReady: MeoShellTheme.ready
                                             && MeoTheme.colorSchemeMode === "dynamic"
                                             && MeoTheme.hasCompleteColorScheme(MeoTheme.dynamicColorScheme)
    readonly property string dynamicThemeMode: MeoTheme.colorSchemeMode
    // A page is ready for route-by-route validation only once its visual
    // handoff has committed. `readyPageKey` is intentionally earlier: it
    // acknowledges that a Loader constructed an item while the old page may
    // still be animating out. Advancing a smoke navigator at that point can
    // coalesce a new request into the active handoff and make the result
    // depend on frame timing rather than route correctness.
    readonly property bool pageContentReady: pageHost.initialized
                                            && pageHost.currentItem !== null
                                            && pageHost.currentPageKey === currentRoute
                                            && !pageHost.loading
                                            && !pageHost.transitioning
    readonly property bool usesDesktopSettingsIndex: rootMetrics.isExpandedWidth
                                                     || rootMetrics.isLargeWidth
                                                     || rootMetrics.isExtraLargeWidth
    signal pageReady(string route)

    MeoWindowMetrics {
        id: rootMetrics
        availableWidth: root.width
        availableHeight: root.height
    }

    function entryForRoute(route) {
        return SettingsRegistry.entry(route)
    }

    function categoryIdForRoute(route) {
        if (route.startsWith("category:"))
            return route.slice("category:".length)
        const entry = entryForRoute(route)
        return entry.categoryId || ""
    }

    function sidebarIndexForRoute(route) {
        if (route === "home")
            return 0
        if (route === "about")
            return SettingsRegistry.sidebarEntries.length - 1

        const categoryId = categoryIdForRoute(route)
        const entries = SettingsRegistry.sidebarEntries
        for (let index = 0; index < entries.length; ++index) {
            if (entries[index].route === "category:" + categoryId)
                return index
        }
        return 0
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

    function navigationRouteFor(route) {
        if (route === "home" || route === "about")
            return route
        const categoryId = categoryIdForRoute(route)
        return categoryId !== "" ? "category:" + categoryId : "home"
    }

    function sidebarCategoryRow(categoryId) {
        const category = SettingsRegistry.category(categoryId)
        return {
            "title": category.title || "",
            "subtitle": category.description || "",
            "leadingIcon": category.icon || "settings",
            "leadingTone": category.tone || "primary",
            "leadingStyle": "tonal",
            "route": category.route || "home",
            "trailingKind": "navigation"
        }
    }

    function sidebarEntryRow(entryId, titleOverride, subtitleOverride) {
        const entry = SettingsRegistry.entry(entryId)
        const route = String(entry.route || "")
        const kcmRoute = route.startsWith("kcm:")
        const available = !kcmRoute || KcmBridge.isAvailable(route.slice(4))
        return {
            "title": titleOverride || entry.title || "",
            "subtitle": available
                        ? (subtitleOverride || entry.description || "")
                        : qsTr("This system setting is not installed"),
            "leadingIcon": entry.icon || "settings",
            "leadingTone": entry.tone || "primary",
            "leadingStyle": "tonal",
            "route": route,
            "enabled": available,
            "trailingKind": kcmRoute ? "choice" : "navigation",
            "trailingText": kcmRoute ? qsTr("Advanced") : ""
        }
    }

    function desktopSelectionRoute(route) {
        for (let groupIndex = 0; groupIndex < desktopSidebarGroups.length; ++groupIndex) {
            const rows = desktopSidebarGroups[groupIndex].rows || []
            for (let rowIndex = 0; rowIndex < rows.length; ++rowIndex) {
                if (String(rows[rowIndex].route || "") === route)
                    return route
            }
        }
        return navigationRouteFor(route)
    }

    // Desktop follows Caelestia Nexus' search-first, connected-group rhythm:
    // a small set of high-signal destinations stays visible, while the full
    // registry remains available through category pages and search. Nothing is
    // removed from SettingsRegistry; this is only the primary information
    // architecture for the wide layout.
    readonly property var desktopSidebarGroups: [
        {
            "title": "",
            "rows": [{
                "title": qsTr("Overview"),
                "subtitle": qsTr("Account, connected devices, and system status"),
                "leadingIcon": "home",
                "leadingTone": "primary",
                "leadingStyle": "tonal",
                "route": "home",
                "trailingKind": "navigation"
            }]
        },
        {
            "title": "",
            "rows": [
                sidebarCategoryRow("personalization")
            ]
        },
        {
            "title": "",
            "rows": [
                sidebarCategoryRow("network"),
                sidebarCategoryRow("devices"),
                sidebarEntryRow("sound", qsTr("Audio"), qsTr("App volumes, sound devices, and microphone")),
                sidebarEntryRow("display", qsTr("Displays"), qsTr("Brightness, Night Light, layout, HDR, and scaling"))
            ]
        },
        {
            "title": "",
            "rows": [
                sidebarEntryRow("power"),
                sidebarEntryRow("updates"),
                sidebarCategoryRow("storage"),
                sidebarCategoryRow("privacy"),
                sidebarCategoryRow("accessibility")
            ]
        },
        {
            "title": "",
            "rows": [
                sidebarEntryRow("shell", qsTr("Desktop & shell"),
                                qsTr("Shelf, Launcher, panels, notification surfaces, and Time Center")),
                sidebarEntryRow("control-center", qsTr("Quick Settings"),
                                qsTr("Top-bar status and Quick Settings tile layout")),
                sidebarCategoryRow("apps"),
                sidebarCategoryRow("accounts"),
                sidebarEntryRow("background-services", qsTr("Services"),
                                qsTr("Background services and session components")),
                sidebarEntryRow("language", qsTr("Language & region"),
                                qsTr("UI language, locale, formats, and regional settings"))
            ]
        },
        {
            "title": "",
            "rows": [{
                "title": qsTr("About"),
                "subtitle": qsTr("MeoArch, hardware, and runtime information"),
                "leadingIcon": "info",
                "leadingTone": "neutral",
                "leadingStyle": "tonal",
                "route": "about",
                "trailingKind": "navigation"
            }]
        }
    ]

    readonly property var desktopSearchRows: {
        if (!desktopSidebar || desktopSidebar.searchText.trim() === "")
            return []
        const rows = []
        const results = SettingsRegistry.search(desktopSidebar.searchText)
        for (let index = 0; index < results.length; ++index) {
            const entry = results[index]
            if (!root.capabilityAvailable(entry.capability))
                continue
            const kcmRoute = String(entry.route || "").startsWith("kcm:")
            const available = !kcmRoute || KcmBridge.isAvailable(String(entry.route).slice(4))
            rows.push({
                "title": entry.title,
                "subtitle": available ? entry.category + " · " + entry.description
                                      : qsTr("Advanced system tool is not installed"),
                "leadingIcon": entry.icon,
                "leadingTone": entry.tone || "primary",
                "leadingStyle": "tonal",
                "route": entry.route,
                "enabled": available,
                "trailingKind": kcmRoute ? "choice" : "navigation",
                "trailingText": kcmRoute ? qsTr("Advanced") : ""
            })
        }
        return rows
    }

    function pageSource(route) {
        if (route === "category:privacy" || route === "privacy")
            return Qt.resolvedUrl("pages/PrivacyPage.qml")
        if (route.startsWith("category:"))
            return Qt.resolvedUrl("pages/CategoryPage.qml")
        switch (route) {
        case "home": return Qt.resolvedUrl("pages/HomePage.qml")
        case "wifi": return Qt.resolvedUrl("pages/WifiPage.qml")
        case "bluetooth": return Qt.resolvedUrl("pages/BluetoothPage.qml")
        case "sound": return Qt.resolvedUrl("pages/SoundPage.qml")
        case "display": return Qt.resolvedUrl("pages/DisplayPage.qml")
        case "power": return Qt.resolvedUrl("pages/PowerPage.qml")
        case "language-region": return Qt.resolvedUrl("pages/LanguageRegionPage.qml")
        case "appearance": return Qt.resolvedUrl("pages/AppearancePage.qml")
        case "notifications": return Qt.resolvedUrl("pages/NotificationsPage.qml")
        case "applications": return Qt.resolvedUrl("pages/ApplicationsPage.qml")
        case "accounts": return Qt.resolvedUrl("pages/AccountsPage.qml")
        case "session-entry": return Qt.resolvedUrl("pages/SessionEntryPage.qml")
        case "control-center": return Qt.resolvedUrl("pages/ControlCenterPage.qml")
        case "shell": return Qt.resolvedUrl("pages/ShellPage.qml")
        case "desktop-integration": return Qt.resolvedUrl("pages/DesktopIntegrationPage.qml")
        case "storage": return Qt.resolvedUrl("pages/StoragePage.qml")
        case "updates": return Qt.resolvedUrl("pages/UpdatesPage.qml")
        case "hardware": return Qt.resolvedUrl("pages/HardwarePage.qml")
        case "recovery": return Qt.resolvedUrl("pages/RecoveryPage.qml")
        case "system-center": return Qt.resolvedUrl("pages/SystemCenterPage.qml")
        case "about": return Qt.resolvedUrl("pages/AboutPage.qml")
        default:
            return route.startsWith("kcm:")
                   ? Qt.resolvedUrl("pages/KcmPage.qml")
                   : Qt.resolvedUrl("pages/HomePage.qml")
        }
    }

    function pageProperties(route) {
        const common = {
            "navigateTo": root.navigate,
            "rootMetrics": rootMetrics
        }
        if (route === "applications") {
            common.requestedAppId = requestedApplicationId
            common.requestedAppName = requestedApplicationName
            common.requestedSection = requestedApplicationSection
        } else if (route.startsWith("category:") && route !== "category:privacy") {
            common.categoryId = route.slice("category:".length)
        } else if (route.startsWith("kcm:")) {
            const entry = entryForRoute(route)
            common.moduleId = route.slice(4)
            common.pageTitle = entry.title || common.moduleId
            common.pageDescription = entry.description || qsTr("Open this KDE settings module")
        }
        return common
    }

    function titleForRoute(route) {
        if (route.startsWith("category:")) {
            const category = SettingsRegistry.category(route.slice("category:".length))
            return category.title || qsTr("Settings")
        }
        const entry = entryForRoute(route)
        return entry.title || qsTr("Meo Settings")
    }

    function parentRouteFor(route) {
        if (route === "home")
            return ""
        // Privacy is a top-level category page, not a second copy of itself.
        // Its direct route is used by search and the sidebar alike.
        if (route === "privacy")
            return "home"
        if (route === "about")
            return "home"
        if (route.startsWith("category:"))
            return "home"
        const entry = entryForRoute(route)
        return entry.categoryId ? "category:" + entry.categoryId : "home"
    }

    function navigateBack() {
        const parentRoute = parentRouteFor(currentRoute)
        if (parentRoute)
            navigate(parentRoute)
    }

    function navigate(route) {
        if (!route)
            return
        const targetRoute = pageSource(route) === Qt.resolvedUrl("pages/HomePage.qml") && route !== "home"
                            ? "home" : route
        if (targetRoute === currentRoute && pageHost.initialized)
            return

        const previousIndex = sidebarIndexForRoute(currentRoute)
        const nextIndex = sidebarIndexForRoute(targetRoute)
        currentRoute = targetRoute
        pageHost.showPage(pageSource(targetRoute), pageProperties(targetRoute),
                          nextIndex >= previousIndex ? 1 : -1, targetRoute)
    }

    // Settings pages share a predictable title-and-groups frame. Declaring
    // those positions lets MeoPageHost acknowledge navigation immediately
    // without showing an empty viewport while an asynchronous page compiles.
    Component {
        id: settingsPageLoadingPlaceholder

        Rectangle {
            color: MeoTheme.surface

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: rootMetrics.isCompactWidth ? MeoTheme.space24 : MeoTheme.space48
                anchors.rightMargin: anchors.leftMargin
                anchors.topMargin: MeoTheme.space40
                spacing: MeoTheme.space16

                MeoSkeleton { type: "text"; width: Math.min(parent.width * 0.42, 280 * MeoTheme.globalScale); height: 28 * MeoTheme.globalScale }
                MeoSkeleton { type: "text"; width: Math.min(parent.width * 0.72, 520 * MeoTheme.globalScale) }
                Item { width: 1; height: MeoTheme.space8 }
                MeoSkeleton { type: "card"; width: parent.width; height: 104 * MeoTheme.globalScale; radius: MeoTheme.shapeExtraLarge }
                MeoSkeleton { type: "card"; width: parent.width; height: 168 * MeoTheme.globalScale; radius: MeoTheme.shapeExtraLarge }
                MeoSkeleton { type: "card"; width: parent.width; height: 104 * MeoTheme.globalScale; radius: MeoTheme.shapeExtraLarge }
            }
        }
    }

    MeoNavigationSuite {
        id: navigation
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.usesDesktopSettingsIndex ? 0 : (isCompact ? parent.width : implicitWidth)
        height: parent.height
        visible: !root.usesDesktopSettingsIndex
        availableWidth: root.width
        model: SettingsRegistry.sidebarEntries
        currentIndex: root.sidebarIndexForRoute(root.currentRoute)
        compactNavigationLimit: 5
        compactPresentation: "drawer"
        preferPersistentDrawer: true
        navigationVisualStyle: "settings"
        // A medium-width rail is deliberately icon-first. It avoids squeezing
        // category labels such as “Privacy & security” into an 80 dp column;
        // the full persistent drawer returns at the large window class.
        labelType: rootMetrics.isMediumWidth ? "none" : "always"
        onClicked: (index) => root.navigate(SettingsRegistry.sidebarEntries[index].route)
    }

    MeoSettingsSidebar {
        id: desktopSidebar
        objectName: "meoSettingsDesktopSidebar"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.usesDesktopSettingsIndex ? MeoTheme.settingsSidebarWidth : 0
        visible: root.usesDesktopSettingsIndex
        showTitle: false
        groups: root.desktopSidebarGroups
        searchResults: root.desktopSearchRows
        selectedRoute: root.desktopSelectionRoute(root.currentRoute)
        onRouteActivated: (route, row) => {
            root.navigate(route)
            if (searching)
                searchText = ""
        }
    }

    Item {
        id: contentHost
        anchors.left: parent.left
        anchors.leftMargin: root.usesDesktopSettingsIndex ? desktopSidebar.width
                                                         : (navigation.isCompact ? 0 : navigation.width)
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: navigation.compactNavigationHeight
        clip: true

        MeoTopAppBar {
            id: compactTopBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: navigation.isCompact
            title: root.currentRoute === "home" ? qsTr("Settings")
                  : (root.currentRoute === "display" ? qsTr("Display & touch")
                                                      : root.titleForRoute(root.currentRoute))
            type: "small"
            navigationIcon: Component {
                MeoIconButton {
                    icon.name: root.currentRoute === "home" ? "menu" : "arrow_back"
                    Accessible.name: root.currentRoute === "home"
                                     ? qsTr("Open settings categories")
                                     : qsTr("Back to settings category")
                    onClicked: {
                        if (root.currentRoute === "home")
                            navigation.openOverflow()
                        else
                            root.navigateBack()
                    }
                }
            }
        }

        MeoPageHost {
            id: pageHost
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: navigation.isCompact ? compactTopBar.bottom : parent.top
            anchors.bottom: parent.bottom
            transitionDistance: 32 * MeoTheme.globalScale
            loadingPlaceholder: settingsPageLoadingPlaceholder
            onPageLoaded: (item) => {
                // Rapid navigation can change currentRoute while the previous
                // asynchronous page is still completing. Acknowledge the
                // exact route constructed by MeoPageHost instead of labelling
                // that page with the newest sidebar selection.
                root.lastLoadedRoute = pageHost.readyPageKey
                root.pageReady(pageHost.readyPageKey)
            }
        }
    }

    Component.onCompleted: {
        // The bridge only reads the active KDE theme; it never applies or
        // rewrites desktop colors from Settings.
        MeoShellTheme.sync()
        navigate("home")
    }
}
