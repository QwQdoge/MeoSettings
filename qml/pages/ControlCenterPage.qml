import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var draftTiles: []
    property string draftDensity: "comfortable"
    property bool hasDraftChanges: false
    property var draftTopBar: ({})
    property bool hasTopBarDraftChanges: false
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property int visibleDraftCount: {
        let count = 0
        for (const tile of draftTiles) {
            if (tile.visible)
                ++count
        }
        return count
    }

    // The editor receives presentation-only copies. The source of truth stays
    // `draftTiles`, which is eventually validated and written by
    // ControlCenterBackend through the active Meo top-bar applet.
    readonly property var visibleEditorTiles: {
        const tiles = []
        for (let index = 0; index < draftTiles.length; ++index) {
            const tile = draftTiles[index]
            if (!tile.visible)
                continue
            tiles.push({
                "id": tile.id,
                "sourceIndex": index,
                "title": root.tileTitle(tile.id),
                "supportingText": root.tileDescription(tile),
                "iconName": root.tileIcon(tile.id),
                "span": tile.span,
                "removable": root.visibleDraftCount > 1,
                "resizable": true
            })
        }
        return tiles
    }

    readonly property var availableEditorTiles: {
        const tiles = []
        for (let index = 0; index < draftTiles.length; ++index) {
            const tile = draftTiles[index]
            if (tile.visible)
                continue
            tiles.push({
                "id": tile.id,
                "sourceIndex": index,
                "title": root.tileTitle(tile.id),
                "iconName": root.tileIcon(tile.id),
                "span": tile.span,
                "resizable": true
            })
        }
        return tiles
    }

    function loadDraft() {
        if (!ControlCenterBackend.available)
            return
        const nextTiles = []
        for (const tile of ControlCenterBackend.tiles) {
            nextTiles.push({ "id": tile.id, "span": Number(tile.span) === 1 ? 1 : 2,
                             "visible": Boolean(tile.visible) })
        }
        draftTiles = nextTiles
        draftDensity = ControlCenterBackend.density
        hasDraftChanges = false
    }

    function loadTopBarDraft() {
        if (!ControlCenterBackend.available)
            return

        const source = ControlCenterBackend.topBar || ({})
        draftTopBar = {
            "textScalePercent": Number(source.textScalePercent === undefined ? 100 : source.textScalePercent),
            "density": String(source.density || "comfortable"),
            "surfaceStyle": String(source.surfaceStyle || "theme"),
            "surfaceOpacityPercent": Number(source.surfaceOpacityPercent === undefined ? 100 : source.surfaceOpacityPercent),
            "motionProfile": String(source.motionProfile || "pixel"),
            "showUnreadBadge": source.showUnreadBadge === undefined ? true : Boolean(source.showUnreadBadge),
            "showJobs": source.showJobs === undefined ? true : Boolean(source.showJobs),
            "showNetwork": source.showNetwork === undefined ? true : Boolean(source.showNetwork),
            "showBluetooth": source.showBluetooth === undefined ? true : Boolean(source.showBluetooth),
            "showVolume": source.showVolume === undefined ? true : Boolean(source.showVolume),
            "batteryDisplay": Number(source.batteryDisplay === undefined ? 2 : source.batteryDisplay),
            "showDate": source.showDate === undefined ? true : Boolean(source.showDate),
            "showNotifications": source.showNotifications === undefined ? true : Boolean(source.showNotifications),
            "use24HourClock": source.use24HourClock === undefined ? true : Boolean(source.use24HourClock)
        }
        hasTopBarDraftChanges = false
    }

    function topBarValue(key, fallbackValue) {
        if (!draftTopBar || draftTopBar[key] === undefined)
            return fallbackValue
        return draftTopBar[key]
    }

    function setTopBarValue(key, value) {
        const next = ({})
        const source = draftTopBar || ({})
        for (const currentKey in source)
            next[currentKey] = source[currentKey]
        next[key] = value
        draftTopBar = next
        hasTopBarDraftChanges = true
    }

    function optionIndex(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (String(options[index].value) === String(value))
                return index
        }
        return 0
    }

    function updateTile(index, key, value) {
        const nextTiles = []
        for (let tileIndex = 0; tileIndex < draftTiles.length; ++tileIndex) {
            const current = draftTiles[tileIndex]
            const next = { "id": current.id, "span": current.span, "visible": current.visible }
            if (tileIndex === index)
                next[key] = value
            nextTiles.push(next)
        }
        draftTiles = nextTiles
        hasDraftChanges = true
    }

    function moveTile(index, offset) {
        const destination = index + offset
        if (destination < 0 || destination >= draftTiles.length)
            return
        const nextTiles = draftTiles.slice()
        const moved = nextTiles.splice(index, 1)[0]
        nextTiles.splice(destination, 0, moved)
        draftTiles = nextTiles
        hasDraftChanges = true
    }

    function moveVisibleTile(fromVisibleIndex, toVisibleIndex) {
        const sourceIndexes = []
        for (let index = 0; index < draftTiles.length; ++index) {
            if (draftTiles[index].visible)
                sourceIndexes.push(index)
        }
        if (fromVisibleIndex < 0 || toVisibleIndex < 0
                || fromVisibleIndex >= sourceIndexes.length
                || toVisibleIndex >= sourceIndexes.length
                || fromVisibleIndex === toVisibleIndex)
            return

        const nextTiles = draftTiles.slice()
        const moved = nextTiles.splice(sourceIndexes[fromVisibleIndex], 1)[0]
        nextTiles.splice(sourceIndexes[toVisibleIndex], 0, moved)
        draftTiles = nextTiles
        hasDraftChanges = true
    }

    function tileTitle(id) {
        switch (id) {
        case "wifi": return qsTr("Wi-Fi")
        case "bluetooth": return qsTr("Bluetooth")
        case "focus": return qsTr("Focus")
        case "nightLight": return qsTr("Night Light")
        case "keepAwake": return qsTr("Keep Awake")
        case "powerMode": return qsTr("Power Mode")
        case "microphone": return qsTr("Microphone")
        case "audioDevices": return qsTr("Sound")
        case "display": return qsTr("Displays")
        default: return qsTr("Screenshot")
        }
    }

    function tileIcon(id) {
        switch (id) {
        case "wifi": return "wifi"
        case "bluetooth": return "bluetooth"
        case "focus": return "do_not_disturb_on"
        case "nightLight": return "dark_mode"
        case "keepAwake": return "coffee"
        case "powerMode": return "battery_saver"
        case "microphone": return "mic"
        case "audioDevices": return "headphones"
        case "display": return "desktop_windows"
        default: return "screenshot_monitor"
        }
    }

    function tileDescription(tile) {
        return (tile.visible ? qsTr("Visible") : qsTr("Hidden"))
               + qsTr(" · ") + (tile.span === 2 ? qsTr("Wide") : qsTr("Compact"))
    }

    readonly property var densityOptions: [
        { "label": qsTr("Compact"), "value": "compact" },
        { "label": qsTr("Comfortable"), "value": "comfortable" }
    ]
    readonly property var surfaceOptions: [
        { "label": qsTr("Follow theme"), "value": "theme" },
        { "label": qsTr("Flat"), "value": "flat" },
        { "label": qsTr("Tonal"), "value": "tonal" },
        { "label": qsTr("Translucent"), "value": "translucent" }
    ]
    readonly property var motionOptions: [
        { "label": qsTr("Calm"), "value": "calm" },
        { "label": qsTr("Pixel"), "value": "pixel" },
        { "label": qsTr("Playful"), "value": "playful" }
    ]
    readonly property var batteryOptions: [
        { "label": qsTr("Hidden"), "value": 0 },
        { "label": qsTr("Icon only"), "value": 1 },
        { "label": qsTr("Icon and percentage"), "value": 2 },
        { "label": qsTr("Detailed state"), "value": 3 }
    ]

    readonly property var topBarAppearanceRows: [
        {
            "id": "textScalePercent",
            "title": qsTr("Text size"),
            "subtitle": qsTr("Scale text inside the Meo top bar without changing the rest of the desktop"),
            "icon": "text_fields",
            "tone": "primary",
            "trailingKind": "slider",
            "from": 75,
            "to": 150,
            "value": Number(root.topBarValue("textScalePercent", 100)),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "density",
            "title": qsTr("Density"),
            "subtitle": qsTr("Choose the spacing used by compact top-bar content"),
            "icon": "density_medium",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.densityOptions,
            "currentIndex": root.optionIndex(root.densityOptions, root.topBarValue("density", "comfortable"))
        },
        {
            "id": "surfaceStyle",
            "title": qsTr("Surface style"),
            "subtitle": qsTr("Follow the theme or use a flat, tonal, or translucent top-bar surface"),
            "icon": "layers",
            "tone": "tertiary",
            "trailingKind": "dropdown",
            "options": root.surfaceOptions,
            "currentIndex": root.optionIndex(root.surfaceOptions, root.topBarValue("surfaceStyle", "theme"))
        },
        {
            "id": "surfaceOpacityPercent",
            "title": qsTr("Surface opacity"),
            "subtitle": qsTr("Adjust the opacity used by translucent top-bar surfaces"),
            "icon": "opacity",
            "tone": "tertiary",
            "trailingKind": "slider",
            "from": 70,
            "to": 100,
            "value": Number(root.topBarValue("surfaceOpacityPercent", 100)),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "motionProfile",
            "title": qsTr("Motion"),
            "subtitle": qsTr("Choose the top-bar animation profile"),
            "icon": "animation",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.motionOptions,
            "currentIndex": root.optionIndex(root.motionOptions, root.topBarValue("motionProfile", "pixel"))
        }
    ]

    readonly property var topBarStatusRows: [
        {
            "id": "showNetwork",
            "title": qsTr("Network status"),
            "subtitle": qsTr("Show the current network state in the compact top bar"),
            "icon": "wifi",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showNetwork", true))
        },
        {
            "id": "showBluetooth",
            "title": qsTr("Bluetooth status"),
            "subtitle": qsTr("Show Bluetooth state when the service is available"),
            "icon": "bluetooth",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showBluetooth", true))
        },
        {
            "id": "showVolume",
            "title": qsTr("Volume status"),
            "subtitle": qsTr("Show the current audio state in the compact top bar"),
            "icon": "volume_up",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showVolume", true))
        },
        {
            "id": "batteryDisplay",
            "title": qsTr("Battery"),
            "subtitle": qsTr("Choose how much battery information appears in the top bar"),
            "icon": "battery_full",
            "tone": "primary",
            "trailingKind": "dropdown",
            "options": root.batteryOptions,
            "currentIndex": root.optionIndex(root.batteryOptions, root.topBarValue("batteryDisplay", 2))
        },
        {
            "id": "showDate",
            "title": qsTr("Date"),
            "subtitle": qsTr("Show the date alongside the clock"),
            "icon": "calendar_today",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showDate", true))
        },
        {
            "id": "use24HourClock",
            "title": qsTr("24-hour clock"),
            "subtitle": qsTr("Use a 24-hour time format in the Meo top bar"),
            "icon": "schedule",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("use24HourClock", true))
        },
        {
            "id": "showNotifications",
            "title": qsTr("Notification indicator"),
            "subtitle": qsTr("Show the notification entry point in the top bar"),
            "icon": "notifications",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showNotifications", true))
        },
        {
            "id": "showUnreadBadge",
            "title": qsTr("Unread badge"),
            "subtitle": qsTr("Show the unread notification count when notifications are visible"),
            "icon": "mark_email_unread",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showUnreadBadge", true))
        },
        {
            "id": "showJobs",
            "title": qsTr("Background tasks"),
            "subtitle": qsTr("Show active background jobs and progress"),
            "icon": "sync",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(root.topBarValue("showJobs", true))
        }
    ]

    Component.onCompleted: {
        loadDraft()
        loadTopBarDraft()
    }

    Connections {
        target: ControlCenterBackend
        function onChanged() {
            if (!root.hasDraftChanges)
                root.loadDraft()
            if (!root.hasTopBarDraftChanges)
                root.loadTopBarDraft()
        }
        function onLayoutSaved() { root.loadDraft() }
        function onTopBarSaved() { root.loadTopBarDraft() }
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Control Center")
        subtitle: qsTr("Choose which Meo Quick Settings tiles appear, how they are arranged, and how dense the surface feels.")

        Column {
            width: parent.width
            visible: ControlCenterBackend.error !== ""
            spacing: MeoTheme.space4
            MeoBanner {
                width: parent.width
                title: qsTr("Control Center needs attention")
                text: qsTr("Try saving the layout again. Your current setup remains unchanged.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(ControlCenterBackend.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: ControlCenterBackend.available
            Accessible.role: Accessible.StatusBar
            Accessible.name: qsTr("Meo Control Center status")
            Accessible.description: ControlCenterBackend.summary
            Column {
                width: parent.width
                spacing: 4 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Control Center")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: ControlCenterBackend.summary
                    typeRole: "body"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: ControlCenterBackend.available
            title: qsTr("Top bar appearance")
            subtitle: qsTr("These controls write the real org.meo.topbar Appearance configuration.")
            model: root.topBarAppearanceRows
            onRowSliderMoved: (index, value, row) => root.setTopBarValue(row.id, value)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setTopBarValue(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const option = row.options && optionIndex >= 0 ? row.options[optionIndex] : null
                if (option && option.value !== undefined)
                    root.setTopBarValue(row.id, option.value)
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: ControlCenterBackend.available
            title: qsTr("Top bar status")
            subtitle: ""
            model: root.topBarStatusRows
            onRowToggled: (index, checked, row) => root.setTopBarValue(row.id, checked)
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const option = row.options && optionIndex >= 0 ? row.options[optionIndex] : null
                if (option && option.value !== undefined)
                    root.setTopBarValue(row.id, option.value)
            }
        }

        Flow {
            width: parent.width
            visible: ControlCenterBackend.available
            spacing: MeoTheme.space8

            MeoButton {
                text: ControlCenterBackend.busy ? qsTr("Applying…") : qsTr("Apply top bar")
                type: "filled"
                enabled: root.hasTopBarDraftChanges && !ControlCenterBackend.busy
                onClicked: ControlCenterBackend.saveTopBar(root.draftTopBar)
            }
            MeoButton {
                text: qsTr("Restore top-bar defaults")
                type: "tonal"
                enabled: !ControlCenterBackend.busy
                onClicked: ControlCenterBackend.resetTopBar()
            }
        }

        MeoDivider {
            width: parent.width
        }

        MeoText {
            width: parent.width
            text: qsTr("Quick Settings")
            typeRole: "title"
            typeSize: "medium"
            emphasized: true
            color: MeoTheme.contentOnSurface
        }

        MeoQuickSettingsEditor {
            id: tileEditor
            width: parent.width
            visible: ControlCenterBackend.available
            title: qsTr("Edit tiles")
            subtitle: qsTr("Choose which tiles appear, then rearrange or resize them.")
            tiles: root.visibleEditorTiles
            availableTiles: root.availableEditorTiles
            columns: 4
            editingEnabled: !ControlCenterBackend.busy
            undoEnabled: root.hasDraftChanges && !ControlCenterBackend.busy
            Accessible.name: qsTr("Edit Meo Quick Settings tiles")
            onBackRequested: root.navigateTo("home")
            onUndoRequested: root.loadDraft()
            onTileMoved: (from, to) => root.moveVisibleTile(from, to)
            onTileResizeRequested: (index, span) => {
                const tile = root.visibleEditorTiles[index]
                if (tile)
                    root.updateTile(tile.sourceIndex, "span", span)
            }
            onTileRemoveRequested: (index, tile) => {
                if (tile && root.visibleDraftCount > 1)
                    root.updateTile(tile.sourceIndex, "visible", false)
            }
            onTileAddRequested: (index, tile) => {
                if (tile)
                    root.updateTile(tile.sourceIndex, "visible", true)
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: ControlCenterBackend.available
            Column {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("Tile density")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Choose how much space Quick Settings tiles use. It does not change text size elsewhere.")
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                Flow {
                    width: parent.width
                    spacing: 8 * MeoTheme.globalScale
                    MeoButton {
                        text: qsTr("Compact")
                        type: root.draftDensity === "compact" ? "filled" : "tonal"
                        enabled: !ControlCenterBackend.busy
                        onClicked: { root.draftDensity = "compact"; root.hasDraftChanges = true }
                    }
                    MeoButton {
                        text: qsTr("Comfortable")
                        type: root.draftDensity === "comfortable" ? "filled" : "tonal"
                        enabled: !ControlCenterBackend.busy
                        onClicked: { root.draftDensity = "comfortable"; root.hasDraftChanges = true }
                    }
                    MeoButton {
                        text: qsTr("Spacious")
                        type: root.draftDensity === "spacious" ? "filled" : "tonal"
                        enabled: !ControlCenterBackend.busy
                        onClicked: { root.draftDensity = "spacious"; root.hasDraftChanges = true }
                    }
                }
            }
        }

        Flow {
            width: parent.width
            visible: ControlCenterBackend.available
            spacing: 8 * MeoTheme.globalScale
            MeoButton {
                text: ControlCenterBackend.busy ? qsTr("Applying…") : qsTr("Apply changes")
                type: "filled"
                enabled: root.hasDraftChanges && !ControlCenterBackend.busy
                onClicked: ControlCenterBackend.saveLayout(root.draftTiles, root.draftDensity)
            }
            MeoButton {
                text: qsTr("Restore defaults")
                type: "tonal"; enabled: !ControlCenterBackend.busy
                onClicked: ControlCenterBackend.resetLayout()
            }
            MeoButton {
                text: qsTr("Refresh")
                type: "text"; enabled: !ControlCenterBackend.busy
                onClicked: ControlCenterBackend.refresh()
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: ControlCenterBackend.available
            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("Your Control Center")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Changes are saved to the Meo top-bar and take effect right away. Meo does not edit Plasma configuration files directly.")
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !ControlCenterBackend.available && !ControlCenterBackend.busy
            icon: "settings"
            title: qsTr("Meo Control Center is unavailable")
            description: qsTr("Add the Meo top-bar to the active Plasma layout, then refresh this page. Meo will not write to a different Plasma configuration file.")
            actionText: qsTr("Refresh")
            onActionClicked: ControlCenterBackend.refresh()
        }
    }
}
