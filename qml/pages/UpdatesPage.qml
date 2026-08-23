import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedUpdate: ({})
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function familyTitle(family) {
        switch (family) {
        case "meo": return qsTr("Meo")
        case "kde": return qsTr("KDE & Plasma")
        case "custom": return qsTr("Custom repositories")
        case "aur": return qsTr("AUR")
        default: return qsTr("System packages")
        }
    }

    function familyIcon(family) {
        switch (family) {
        case "meo": return "auto_awesome"
        case "kde": return "desktop_windows"
        case "custom": return "source"
        case "aur": return "extension"
        default: return "memory"
        }
    }

    function familyTone(family) {
        switch (family) {
        case "meo": return "tertiary"
        case "kde": return "secondary"
        case "custom": return "primary"
        case "aur": return "secondary"
        default: return "neutral"
        }
    }

    function updateRowsFor(family, updates) {
        const rows = []
        for (let index = 0; index < updates.length; ++index) {
            const update = updates[index]
            if (update.family !== family)
                continue
            rows.push({
                "title": update.name,
                "subtitle": qsTr("%1 → %2").arg(update.installedVersion).arg(update.availableVersion),
                "icon": root.familyIcon(family),
                "tone": root.familyTone(family),
                "trailingKind": "status",
                "trailingText": update.repository || update.source || qsTr("Unknown source"),
                "update": update
            })
        }
        return rows
    }

    readonly property var meoRows: root.updateRowsFor("meo", UpdatesBackend.updates)
    readonly property var kdeRows: root.updateRowsFor("kde", UpdatesBackend.updates)
    readonly property var customRows: root.updateRowsFor("custom", UpdatesBackend.updates)
    readonly property var systemRows: root.updateRowsFor("system", UpdatesBackend.updates)
    readonly property var nativeAurRows: root.updateRowsFor("aur", UpdatesBackend.updates)
    readonly property var aurRows: root.updateRowsFor("aur", UpdatesBackend.aurUpdates)

    readonly property var repositoryRows: {
        const rows = []
        const repositories = UpdatesBackend.configuredRepositories
        for (let index = 0; index < repositories.length; ++index) {
            const repository = repositories[index]
            if (!repository.custom && repository.kind !== "meo")
                continue
            rows.push({
                "title": repository.name,
                "subtitle": repository.description,
                "icon": repository.kind === "meo" ? "auto_awesome" : "source",
                "tone": repository.kind === "meo" ? "tertiary" : "primary",
                "trailingKind": "status",
                "trailingText": repository.kind === "meo" ? qsTr("Meo") : qsTr("Custom"),
                "interactive": false
            })
        }
        return rows
    }

    function openDetails(row) {
        root.selectedUpdate = row.update || ({})
        updateDetails.open()
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Updates")
        subtitle: qsTr("Meo, KDE, system, and configured-repository packages are shown independently from OmniStore. This page never installs an update.")

        MeoCard {
            width: parent.width
            type: "filled"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon { icon: "system_update"; size: 26; color: MeoTheme.primary }
                    Column {
                        width: parent.width - 38 * MeoTheme.globalScale
                        spacing: 2 * MeoTheme.globalScale
                        MeoText {
                            width: parent.width
                            text: qsTr("System update overview")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }
                        MeoText {
                            width: parent.width
                            text: UpdatesBackend.summary
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                MeoProgressBar {
                    width: parent.width
                    visible: UpdatesBackend.busy && !UpdatesBackend.checkingAur
                    type: "linear"
                    indeterminate: true
                }

                MeoText {
                    width: parent.width
                    visible: UpdatesBackend.cachedMetadataTimestamp !== ""
                    text: qsTr("Based on locally cached repository metadata from %1. Refreshing here does not download metadata or run pacman -Sy/-Syu.")
                          .arg(UpdatesBackend.cachedMetadataTimestamp)
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8 * MeoTheme.globalScale
                    MeoButton {
                        text: qsTr("Refresh local metadata")
                        type: "tonal"
                        enabled: UpdatesBackend.pacmanAvailable && !UpdatesBackend.busy
                        loading: UpdatesBackend.busy && !UpdatesBackend.checkingAur
                        onClicked: UpdatesBackend.refresh()
                    }
                    MeoButton {
                        text: qsTr("Open system updater")
                        type: "outlined"
                        visible: KcmBridge.isAvailable("kcm_updates")
                        enabled: !UpdatesBackend.busy
                        onClicked: root.navigateTo("kcm:kcm_updates")
                    }
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 190 * MeoTheme.globalScale
            visible: UpdatesBackend.pacmanAvailable && !UpdatesBackend.busy
                     && UpdatesBackend.updateCount === 0 && UpdatesBackend.error === ""
            icon: "check_circle"
            title: qsTr("No cached system updates")
            description: qsTr("This only reflects your currently downloaded pacman repository metadata. Use the system updater to refresh repositories and install updates.")
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.meoRows.length > 0
            title: qsTr("Meo updates")
            subtitle: qsTr("MeoArch components from the configured pacman metadata")
            model: root.meoRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.kdeRows.length > 0
            title: qsTr("KDE & Plasma updates")
            subtitle: qsTr("Desktop components classified by their installed package names")
            model: root.kdeRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.customRows.length > 0
            title: qsTr("Custom repository updates")
            subtitle: qsTr("Packages whose current repository is configured outside the distribution defaults")
            model: root.customRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.systemRows.length > 0
            title: qsTr("System package updates")
            subtitle: qsTr("Remaining native packages from cached pacman metadata")
            model: root.systemRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.nativeAurRows.length > 0
            title: qsTr("Foreign package updates")
            subtitle: qsTr("Foreign/local packages surfaced by pacman’s cached update view")
            model: root.nativeAurRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("AUR update check")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: UpdatesBackend.aurSummary
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                MeoProgressBar {
                    width: parent.width
                    visible: UpdatesBackend.checkingAur
                    type: "linear"
                    indeterminate: true
                }
                MeoButton {
                    text: qsTr("Check AUR updates")
                    type: "outlined"
                    visible: UpdatesBackend.aurHelperAvailable
                    enabled: !UpdatesBackend.busy
                    onClicked: aurConfirmation.open()
                }
                MeoText {
                    width: parent.width
                    visible: !UpdatesBackend.aurHelperAvailable
                    text: qsTr("Install paru or yay to make an explicit AUR check available. Settings never runs an AUR install or upgrade itself.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.aurRows.length > 0
            title: qsTr("AUR updates")
            subtitle: qsTr("Reported only after you explicitly ask your configured AUR helper to check")
            model: root.aurRows
            onRowActivated: (index, row) => root.openDetails(row)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.repositoryRows.length > 0
            title: qsTr("Configured Meo & custom repositories")
            subtitle: qsTr("Repository names read from pacman.conf. This list does not modify repository configuration.")
            model: root.repositoryRows
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: UpdatesBackend.error !== ""
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: UpdatesBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: aurConfirmation
        popupParent: Overlay.overlay
        title: qsTr("Check AUR updates?")
        subtitle: qsTr("This asks your configured paru or yay helper to query its remote AUR metadata. It does not install, upgrade, remove, or refresh pacman packages.")
        acceptText: qsTr("Check")
        rejectText: qsTr("Cancel")
        onAccepted: UpdatesBackend.refreshAurUpdates()
    }

    MeoSettingsTaskSheet {
        id: updateDetails
        popupParent: Overlay.overlay
        title: root.selectedUpdate.name || qsTr("Package update")
        subtitle: qsTr("Read-only package metadata. Close this sheet to return to Updates.")
        rejectText: qsTr("Close")
        content: Component {
            MeoSettingsGroup {
                width: parent.width
                title: qsTr("Package information")
                model: [
                    { "title": qsTr("Installed version"), "subtitle": root.selectedUpdate.installedVersion || qsTr("Unavailable"), "icon": "history", "tone": "neutral", "trailingKind": "none", "interactive": false },
                    { "title": qsTr("Available version"), "subtitle": root.selectedUpdate.availableVersion || qsTr("Unavailable"), "icon": "system_update", "tone": root.familyTone(root.selectedUpdate.family), "trailingKind": "none", "interactive": false },
                    { "title": qsTr("Update family"), "subtitle": root.familyTitle(root.selectedUpdate.family), "icon": root.familyIcon(root.selectedUpdate.family), "tone": root.familyTone(root.selectedUpdate.family), "trailingKind": "none", "interactive": false },
                    { "title": qsTr("Repository"), "subtitle": root.selectedUpdate.repository || root.selectedUpdate.source || qsTr("Not available from local metadata"), "icon": "source", "tone": "neutral", "trailingKind": "none", "interactive": false }
                ]
            }
        }
    }
}
