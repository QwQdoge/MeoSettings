import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property string categoryId: ""
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property var categoryInfo: SettingsRegistry.category(categoryId)
    readonly property var categoryEntries: SettingsRegistry.entriesForCategory(categoryId)

    function isKcmRoute(route) {
        return String(route).startsWith("kcm:")
    }

    function routeIsAvailable(route) {
        return !isKcmRoute(route) || KcmBridge.isAvailable(String(route).slice(4))
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

    readonly property var categoryRows: {
        const rows = []
        for (let index = 0; index < categoryEntries.length; ++index) {
            const entry = categoryEntries[index]
            if (!root.capabilityAvailable(entry.capability))
                continue
            const handoff = entry.presentation === "external"
            const available = routeIsAvailable(entry.route)
            rows.push({
                "title": entry.title,
                "subtitle": handoff && !available
                            ? qsTr("This advanced system tool is not installed")
                            : entry.description,
                "icon": entry.icon,
                "tone": entry.tone || "primary",
                "route": entry.route,
                "enabled": available,
                "trailingKind": handoff ? "choice" : "navigation",
                "trailingText": handoff ? qsTr("Advanced") : ""
            })
        }
        return rows
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : (root.categoryInfo.title || qsTr("Settings"))
        subtitle: root.categoryInfo.description || qsTr("Browse available settings")

        MeoSettingsGroup {
            width: parent.width
            visible: root.categoryRows.length > 0
            title: qsTr("Settings")
            subtitle: qsTr("Daily controls stay in Meo Settings. Advanced tools appear only where their workflow needs extra system protection.")
            model: root.categoryRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: root.categoryRows.length === 0
            icon: "settings"
            title: qsTr("No settings in this category")
            description: qsTr("The settings registry does not currently expose an entry here.")
            actionText: qsTr("Back to Home")
            onActionClicked: root.navigateTo("home")
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale

                MeoText {
                    width: parent.width
                    text: qsTr("System ownership")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings is the primary place for everyday system changes. A protected advanced tool appears only for workflows that require special privileges, recovery, or an authentication flow that Meo has not safely implemented yet.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
