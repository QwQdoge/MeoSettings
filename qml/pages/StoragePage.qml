import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedVolume: ({})
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

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

    function volumeSubtitle(volume) {
        const status = volume.readOnly ? qsTr("Read-only") : qsTr("Read/write")
        const external = volume.external ? qsTr("External") : qsTr("Internal")
        const mounts = volume.mountCount > 1
                       ? qsTr(" · %n mounted path(s)", "", volume.mountCount)
                       : ""
        return qsTr("%1% used · %2 free · %3 · %4%5")
            .arg(volume.usedPercent)
            .arg(formatBytes(volume.freeBytes))
            .arg(external)
            .arg(status)
            .arg(mounts)
    }

    function volumeTitle(volume) {
        if (volume.mountPoint === "/")
            return qsTr("System volume")
        return volume.displayName || volume.mountPoint
    }

    function formatPercent(value) {
        return Math.round(Math.max(0, Math.min(100, Number(value) || 0))) + "%"
    }

    function sourceIcon(sourceId) {
        switch (sourceId) {
        case "flatpak": return "deployed_code"
        case "appimage": return "rocket_launch"
        case "pacman": return "terminal"
        case "aur": return "extension"
        default: return "apps"
        }
    }

    function sourceTone(sourceId) {
        switch (sourceId) {
        case "flatpak": return "secondary"
        case "appimage": return "tertiary"
        case "pacman": return "primary"
        case "aur": return "error"
        default: return "neutral"
        }
    }

    function categoryIcon(categoryId) {
        switch (categoryId) {
        case "images": return "image"
        case "videos": return "movie"
        case "documents": return "description"
        case "audio": return "music_note"
        case "ai": return "auto_awesome"
        default: return "folder"
        }
    }

    function categoryTone(categoryId) {
        switch (categoryId) {
        case "images": return "tertiary"
        case "videos": return "secondary"
        case "documents": return "primary"
        case "audio": return "secondary"
        case "ai": return "primary"
        default: return "neutral"
        }
    }

    function categoryStateText(category) {
        switch (category.state) {
        case "complete": return qsTr("Checked")
        case "partial": return qsTr("Partial")
        case "canceled": return qsTr("Canceled")
        case "not-present": return qsTr("Not found")
        case "permission-denied": return qsTr("No access")
        case "unsafe-root": return qsTr("Skipped safely")
        case "not-scanned": return qsTr("Not scanned")
        default: return qsTr("Unknown")
        }
    }

    function categorySubtitle(category) {
        const state = category.state || "not-scanned"
        if (state === "complete")
            return qsTr("%1 recognised file(s) in %2 selected folder(s)")
                    .arg(category.fileCount || 0)
                    .arg(category.scannedRootCount || 0)
        if (state === "partial" || state === "canceled")
            return qsTr("%1 recognised file(s); protected, unsafe, or remaining paths were not counted")
                    .arg(category.fileCount || 0)
        if (state === "not-present")
            return qsTr("No selected folder is present on this device")
        if (state === "permission-denied")
            return qsTr("The selected folder could not be read, so its size is unknown")
        if (state === "unsafe-root")
            return qsTr("A folder outside Home or behind a symlink was skipped")
        return qsTr("Run the explicit category scan to inspect selected folders")
    }

    function isPacmanSource(source) {
        const id = String(source.id || "").toLowerCase()
        const name = String(source.name || "").toLowerCase()
        return id === "pacman" || name === "pacman" || name === "native"
    }

    function isAurSource(source) {
        const id = String(source.id || "").toLowerCase()
        const name = String(source.name || "").toLowerCase()
        return id === "aur" || name === "aur"
    }

    function sourceFor(kind) {
        const sources = OmniStoreAppsBackend.sources
        for (let index = 0; index < sources.length; ++index) {
            const source = sources[index]
            if ((kind === "pacman" && root.isPacmanSource(source))
                    || (kind === "aur" && root.isAurSource(source)))
                return source
        }
        return null
    }

    readonly property var volumeRows: {
        const rows = []
        const volumes = StorageBackend.volumes
        for (let index = 0; index < volumes.length; ++index) {
            const volume = volumes[index]
            rows.push({
                "title": root.volumeTitle(volume),
                "subtitle": root.volumeSubtitle(volume),
                "icon": volume.external ? "usb" : "storage",
                "tone": volume.external ? "secondary" : "tertiary",
                "trailingKind": "navigation",
                "volume": volume
            })
        }
        return rows
    }

    readonly property var selectedVolumeRows: [
        { "title": qsTr("Mount point"), "subtitle": selectedVolume.mountPoint || qsTr("Unavailable"), "icon": "folder", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("Mounted paths"), "subtitle": selectedVolume.mountPoints ? selectedVolume.mountPoints.join(", ") : qsTr("Unavailable"), "icon": "account_tree", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("Device"), "subtitle": selectedVolume.device || qsTr("Unavailable"), "icon": "hard_drive", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("File system"), "subtitle": selectedVolume.filesystem || qsTr("Unavailable"), "icon": "account_tree", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("Capacity"), "subtitle": root.formatBytes(selectedVolume.totalBytes), "icon": "data_usage", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("Available to this user"), "subtitle": root.formatBytes(selectedVolume.freeBytes), "icon": "space_dashboard", "tone": "neutral", "trailingKind": "none", "interactive": false },
        { "title": qsTr("Access"), "subtitle": selectedVolume.readOnly ? qsTr("Read-only") : qsTr("Read/write"), "icon": selectedVolume.readOnly ? "lock" : "lock_open", "tone": "neutral", "trailingKind": "none", "interactive": false }
    ]

    readonly property var systemVolume: {
        const volumes = StorageBackend.volumes
        for (let index = 0; index < volumes.length; ++index) {
            if (volumes[index].mountPoint === "/")
                return volumes[index]
        }
        return ({})
    }

    readonly property var categoryUsageRows: {
        const rows = []
        const categories = StorageBackend.categoryUsage
        for (let index = 0; index < categories.length; ++index) {
            const category = categories[index]
            const complete = category.state === "complete"
            const partial = category.state === "partial" || category.state === "canceled"
            rows.push({
                "title": category.label || category.id,
                "subtitle": root.categorySubtitle(category),
                "icon": root.categoryIcon(category.id),
                "tone": root.categoryTone(category.id),
                "trailingKind": complete || partial ? "status" : "choice",
                "trailingText": complete || partial
                                ? root.formatBytes(category.bytes)
                                : root.categoryStateText(category),
                "interactive": false
            })
        }
        return rows
    }

    readonly property var packageDatabaseRows: {
        const rows = []
        const groups = PackageInventoryBackend.groups || []
        for (let index = 0; index < groups.length; ++index) {
            const group = groups[index]
            const foreign = group.id === "foreign"
            const count = Number(group.packageCount) || 0
            const knownCount = Number(group.sizeKnownCount) || 0
            const sizeDetail = knownCount === count
                    ? root.formatBytes(group.knownSizeBytes)
                    : qsTr("%1 known package file size(s)").arg(root.formatBytes(group.knownSizeBytes))
            rows.push({
                "title": foreign ? qsTr("Foreign / AUR candidates")
                                 : qsTr("Pacman repository packages"),
                "subtitle": foreign
                            ? qsTr("%1 foreign package(s) · %2. Local manually built packages can also be foreign, so this is not a claim that every entry is from AUR.")
                                  .arg(count).arg(sizeDetail)
                            : qsTr("%1 locally installed repository package(s) · %2 installed package files")
                                  .arg(count).arg(sizeDetail),
                "icon": foreign ? "extension" : "terminal",
                "tone": foreign ? "tertiary" : "primary",
                "trailingKind": "status",
                "trailingText": root.formatBytes(group.knownSizeBytes),
                "interactive": false
            })
        }
        return rows
    }

    readonly property var packageInventoryRows: {
        const rows = []
        const pacman = root.sourceFor("pacman")
        const aur = root.sourceFor("aur")
        const addPackageSource = function(title, icon, tone, source, unavailableText) {
            if (source) {
                rows.push({
                    "title": title,
                    "subtitle": qsTr("%1 OmniStore application record(s) · %2 known application size")
                                    .arg(source.applicationCount)
                                    .arg(root.formatBytes(source.knownSizeBytes)),
                    "icon": icon,
                    "tone": tone,
                    "trailingKind": "status",
                    "trailingText": root.formatPercent(source.sharePercent),
                    "interactive": false
                })
            } else {
                rows.push({
                    "title": title,
                    "subtitle": unavailableText,
                    "icon": icon,
                    "tone": tone,
                    "trailingKind": "choice",
                    "trailingText": qsTr("Not reported"),
                    "interactive": false
                })
            }
        }
        addPackageSource(qsTr("Pacman applications"), "terminal", "primary", pacman,
                         qsTr("OmniStore did not report a Pacman source in its current app snapshot"))
        addPackageSource(qsTr("AUR applications"), "extension", "error", aur,
                         qsTr("OmniStore did not report an AUR source in its current app snapshot"))
        return rows
    }

    readonly property var omniStoreSourceRows: {
        const rows = []
        const sources = OmniStoreAppsBackend.sources
        for (let index = 0; index < sources.length; ++index) {
            const source = sources[index]
            if (root.isPacmanSource(source) || root.isAurSource(source))
                continue
            rows.push({
                "title": source.name,
                "subtitle": qsTr("%1 app(s) · %2 known app size")
                                .arg(source.applicationCount)
                                .arg(root.formatBytes(source.knownSizeBytes)),
                "icon": root.sourceIcon(source.id),
                "tone": root.sourceTone(source.id),
                "trailingKind": "progress",
                "progress": Math.max(0, Math.min(1, Number(source.sharePercent) / 100)),
                "progressText": root.formatPercent(source.sharePercent),
                "showProgressLabel": true,
                "interactive": false
            })
        }
        return rows
    }

    readonly property var omniStoreApplicationRows: {
        const rows = []
        const applications = OmniStoreAppsBackend.topApplications
        for (let index = 0; index < applications.length; ++index) {
            const application = applications[index]
            const known = application.sizeKind !== "unknown"
            rows.push({
                "title": application.name,
                "subtitle": known
                                ? qsTr("%1 · %2 size metadata")
                                      .arg(application.sourceName)
                                      .arg(application.sizeKind === "exact" ? qsTr("Exact") : qsTr("Reported"))
                                : qsTr("%1 · Size metadata unavailable").arg(application.sourceName),
                "icon": "apps",
                "tone": root.sourceTone(application.sourceId),
                "trailingKind": "status",
                "trailingText": known ? root.formatBytes(application.sizeBytes) : qsTr("Unknown"),
                "interactive": false
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
        title: root.isCompact ? "" : qsTr("Storage & applications")
        subtitle: qsTr("Mounted capacity stays separate from optional, bounded personal-folder categories so totals are never invented or double-counted.")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Mounted volumes")
            subtitle: StorageBackend.summary
            visible: root.volumeRows.length > 0
            model: root.volumeRows
            onRowActivated: (index, row) => {
                root.selectedVolume = row.volume || ({})
                volumeDetails.open()
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 220 * MeoTheme.globalScale
            visible: root.volumeRows.length === 0
            icon: "storage"
            title: qsTr("No mounted user storage is visible")
            description: qsTr("Pseudo and memory-backed filesystems are intentionally excluded from this overview.")
        }

        MeoCard {
            width: parent.width
            type: "filled"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale

                    MeoIcon {
                        icon: "folder_open"
                        size: 24
                        color: MeoTheme.primary
                    }

                    Column {
                        width: parent.width - 36 * MeoTheme.globalScale
                        spacing: 2 * MeoTheme.globalScale

                        MeoText {
                            width: parent.width
                            text: qsTr("Personal storage categories")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }

                        MeoText {
                            width: parent.width
                            text: qsTr("Images, videos, documents, audio, and known local AI model/cache folders are scanned only when you ask. The scan stays inside selected Home folders, skips symlinks, and stops at a fixed safety limit.")
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                MeoProgressBar {
                    width: parent.width
                    visible: StorageBackend.usageScanActive
                    type: "linear"
                    indeterminate: true
                }

                MeoText {
                    width: parent.width
                    text: StorageBackend.usageScanSummary
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoText {
                    width: parent.width
                    visible: StorageBackend.usageScanError !== ""
                    text: StorageBackend.usageScanError
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8 * MeoTheme.globalScale

                    MeoButton {
                        text: qsTr("Scan selected folders")
                        type: "tonal"
                        visible: !StorageBackend.usageScanActive
                        onClicked: StorageBackend.startUsageScan()
                    }

                    MeoButton {
                        text: qsTr("Cancel scan")
                        type: "outlined"
                        visible: StorageBackend.usageScanActive
                        onClicked: StorageBackend.cancelUsageScan()
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.categoryUsageRows.length > 0
            title: qsTr("Personal data by category")
            subtitle: qsTr("Only recognised files in the explicitly selected folders are listed. Category values are not added to mounted-volume usage, because they already live on those volumes.")
            model: root.categoryUsageRows
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.systemVolume.mountPoint === "/"

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                MeoIcon {
                    icon: "memory"
                    size: 24
                    color: MeoTheme.contentOnSurfaceVariant
                }

                Column {
                    width: parent.width - 36 * MeoTheme.globalScale
                    spacing: 2 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        text: qsTr("System & other data")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }

                    MeoText {
                        width: parent.width
                        text: qsTr("%1 used on the system volume. This exact volume figure includes system files, packages, settings, logs, and anything outside the selected personal categories; it is intentionally not presented as a category total.")
                              .arg(root.formatBytes(root.systemVolume.usedBytes))
                        typeRole: "body"
                        typeSize: "medium"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale

                    MeoIcon {
                        icon: "terminal"
                        size: 24
                        color: MeoTheme.primary
                    }

                    Column {
                        width: parent.width - 36 * MeoTheme.globalScale
                        spacing: 2 * MeoTheme.globalScale

                        MeoText {
                            width: parent.width
                            text: qsTr("Installed software")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }

                        MeoText {
                            width: parent.width
                            text: PackageInventoryBackend.summary
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                MeoProgressBar {
                    width: parent.width
                    visible: PackageInventoryBackend.busy
                    type: "linear"
                    indeterminate: true
                }

                MeoText {
                    width: parent.width
                    visible: PackageInventoryBackend.error !== ""
                    text: PackageInventoryBackend.error
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }

                MeoButton {
                    text: PackageInventoryBackend.busy
                          ? qsTr("Inspecting packages…")
                          : (PackageInventoryBackend.snapshotAvailable
                             ? qsTr("Refresh package inventory")
                             : qsTr("Inspect installed packages"))
                    type: "tonal"
                    enabled: PackageInventoryBackend.pacmanAvailable && !PackageInventoryBackend.busy
                    loading: PackageInventoryBackend.busy
                    onClicked: PackageInventoryBackend.refresh()
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: PackageInventoryBackend.snapshotAvailable && root.packageDatabaseRows.length > 0
            title: qsTr("Pacman package storage")
            subtitle: qsTr("Installed package file sizes come directly from the local Pacman database. They are separate from OmniStore's managed-application size mix and are not added to the system-volume total.")
            model: root.packageDatabaseRows
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: OmniStoreAppsBackend.exporterAvailable

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale

                    MeoIcon {
                        icon: "apps"
                        size: 24
                        color: MeoTheme.primary
                    }

                    Column {
                        width: parent.width - 36 * MeoTheme.globalScale
                        spacing: 2 * MeoTheme.globalScale

                        MeoText {
                            width: parent.width
                            text: qsTr("Applications managed by OmniStore")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }

                        MeoText {
                            width: parent.width
                            text: OmniStoreAppsBackend.summary
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
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
                    visible: OmniStoreAppsBackend.available && !OmniStoreAppsBackend.busy
                    text: qsTr("%1 known app size · %2 exact, %3 reported · %4")
                              .arg(root.formatBytes(OmniStoreAppsBackend.knownSizeBytes))
                              .arg(OmniStoreAppsBackend.exactSizeCount)
                              .arg(OmniStoreAppsBackend.reportedSizeCount)
                              .arg(qsTr("%n app(s) without size metadata", "", OmniStoreAppsBackend.unknownSizeCount))
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8 * MeoTheme.globalScale

                    MeoButton {
                        text: qsTr("Refresh apps")
                        type: "tonal"
                        enabled: !OmniStoreAppsBackend.busy
                        loading: OmniStoreAppsBackend.busy
                        onClicked: OmniStoreAppsBackend.refresh()
                    }

                    MeoButton {
                        text: qsTr("Open OmniStore")
                        type: "outlined"
                        visible: OmniStoreAppsBackend.launcherAvailable
                        onClicked: OmniStoreAppsBackend.openOmniStore()
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: OmniStoreAppsBackend.available
            title: qsTr("Pacman & AUR application inventory")
            subtitle: qsTr("These are only the source-tagged application records OmniStore reports. A missing AUR row is not treated as zero installed packages, and local/foreign packages are never guessed to be AUR.")
            model: root.packageInventoryRows
        }

        MeoSettingsGroup {
            width: parent.width
            visible: OmniStoreAppsBackend.available && root.omniStoreSourceRows.length > 0
            title: qsTr("Other application sources")
            subtitle: qsTr("Each percentage is that source's share of known OmniStore application-size metadata. It does not include dependencies, caches, or personal data.")
            model: root.omniStoreSourceRows
        }

        MeoSettingsGroup {
            width: parent.width
            visible: OmniStoreAppsBackend.available && root.omniStoreApplicationRows.length > 0
            title: qsTr("Largest reported applications")
            subtitle: qsTr("The list is limited to the largest application records OmniStore can describe; reported values are not a filesystem scan.")
            model: root.omniStoreApplicationRows
        }

        MeoEmptyState {
            width: parent.width
            height: 180 * MeoTheme.globalScale
            visible: OmniStoreAppsBackend.available
                     && !OmniStoreAppsBackend.busy
                     && OmniStoreAppsBackend.applicationCount === 0
            icon: "apps"
            title: qsTr("No OmniStore applications are reported")
            description: qsTr("This result reflects the enabled local sources in OmniStore. It does not claim that the system has no software installed.")
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !OmniStoreAppsBackend.exporterAvailable

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon { icon: "apps"; size: 24; color: MeoTheme.contentOnSurfaceVariant }
                    MeoText {
                        width: parent.width - 36 * MeoTheme.globalScale
                        text: qsTr("Install OmniStore's app overview exporter to see its managed-application size mix here. Meo Settings will continue to show mounted volumes without it.")
                        typeRole: "body"
                        typeSize: "medium"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }

                MeoButton {
                    text: qsTr("Open OmniStore")
                    type: "outlined"
                    visible: OmniStoreAppsBackend.launcherAvailable
                    onClicked: OmniStoreAppsBackend.openOmniStore()
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: OmniStoreAppsBackend.error !== "" && OmniStoreAppsBackend.exporterAvailable

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: OmniStoreAppsBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoButton {
            text: qsTr("Refresh storage")
            type: "tonal"
            onClicked: StorageBackend.refresh()
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Disk management")
            subtitle: qsTr("Configuration and destructive operations remain in maintained KDE tools with their own confirmation and recovery flows.")
            model: [
                {
                    "id": "automount",
                    "title": qsTr("Removable devices"),
                    "subtitle": KcmBridge.isAvailable("kcm_device_automounter")
                                ? qsTr("Set how removable media is handled")
                                : qsTr("The KDE removable-device module is not installed"),
                    "icon": "usb", "tone": "secondary",
                    "route": "kcm:kcm_device_automounter",
                    "enabled": KcmBridge.isAvailable("kcm_device_automounter"),
                    "trailingKind": "choice", "trailingText": qsTr("Advanced")
                },
                {
                    "id": "disk-health",
                    "title": qsTr("Disk health"),
                    "subtitle": KcmBridge.isAvailable("kcm_disks")
                                ? qsTr("Inspect the installed KDE disk information module")
                                : qsTr("The KDE disk information module is not installed"),
                    "icon": "hard_drive", "tone": "tertiary",
                    "route": "kcm:kcm_disks",
                    "enabled": KcmBridge.isAvailable("kcm_disks"),
                    "trailingKind": "choice", "trailingText": qsTr("Advanced")
                },
                {
                    "id": "partition-manager",
                    "title": qsTr("Partitions & filesystems"),
                    "subtitle": KcmBridge.partitionManagerAvailable
                                ? qsTr("Open KDE Partition Manager for partition, format, and filesystem work")
                                : qsTr("KDE Partition Manager is not installed"),
                    "icon": "storage", "tone": "error",
                    "enabled": KcmBridge.partitionManagerAvailable,
                    "trailingKind": "action", "actionText": qsTr("Open")
                }
            ]
            onRowActivated: (index, row) => {
                if (row.route)
                    root.navigateTo(row.route)
            }
            onRowActionTriggered: (index, row) => {
                if (row.id === "partition-manager")
                    KcmBridge.openPartitionManager()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Backup & recovery")
            subtitle: qsTr("Long-running recovery work is not a transient drawer and must have a verified backend before it becomes a Meo Settings control.")
            model: [{
                "title": qsTr("Backup provider"),
                "subtitle": qsTr("No verified backup or restore backend is available in this build."),
                "icon": "backup",
                "tone": "neutral",
                "trailingKind": "status",
                "trailingText": qsTr("Not configured"),
                "interactive": false
            }]
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: KcmBridge.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: KcmBridge.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: volumeDetails
        popupParent: Overlay.overlay
        title: root.selectedVolume.displayName || qsTr("Volume details")
        subtitle: qsTr("Read-only details for the selected mounted volume. Closing this sheet returns to Storage & backup.")
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: volumeInformation.implicitHeight + 40 * MeoTheme.globalScale

                MeoSettingsGroup {
                    id: volumeInformation
                    width: parent.width - 40 * MeoTheme.globalScale
                    x: 20 * MeoTheme.globalScale
                    y: 20 * MeoTheme.globalScale
                    title: qsTr("Volume information")
                    model: root.selectedVolumeRows
                }
            }
        }
    }
}
