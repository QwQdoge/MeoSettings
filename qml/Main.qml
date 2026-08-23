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
    readonly property bool pageContentReady: pageHost.initialized
                                            && pageHost.currentItem !== null
                                            && lastLoadedRoute === currentRoute
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
        case "appearance": return Qt.resolvedUrl("pages/AppearancePage.qml")
        case "notifications": return Qt.resolvedUrl("pages/NotificationsPage.qml")
        case "accounts": return Qt.resolvedUrl("pages/AccountsPage.qml")
        case "control-center": return Qt.resolvedUrl("pages/ControlCenterPage.qml")
        case "storage": return Qt.resolvedUrl("pages/StoragePage.qml")
        case "updates": return Qt.resolvedUrl("pages/UpdatesPage.qml")
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
        if (route.startsWith("category:") && route !== "category:privacy") {
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

    MeoNavigationSuite {
        id: navigation
        anchors.left: parent.left
        anchors.top: parent.top
        width: isCompact ? parent.width : implicitWidth
        height: parent.height
        availableWidth: root.width
        model: SettingsRegistry.sidebarEntries
        currentIndex: root.sidebarIndexForRoute(root.currentRoute)
        compactNavigationLimit: 5
        compactPresentation: "drawer"
        // A medium-width rail is deliberately icon-first. It avoids squeezing
        // category labels such as “Privacy & security” into an 80 dp column;
        // the full persistent drawer returns at the large window class.
        labelType: rootMetrics.isMediumWidth ? "none" : "always"
        onClicked: (index) => root.navigate(SettingsRegistry.sidebarEntries[index].route)
    }

    Item {
        id: contentHost
        anchors.left: parent.left
        anchors.leftMargin: navigation.isCompact ? 0 : navigation.width
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
                                               : root.titleForRoute(root.currentRoute)
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
            onPageLoaded: (item) => {
                root.lastLoadedRoute = root.currentRoute
                root.pageReady(root.currentRoute)
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
