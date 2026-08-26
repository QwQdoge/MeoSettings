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

    Component.onCompleted: loadDraft()

    Connections {
        target: ControlCenterBackend
        function onChanged() {
            if (!root.hasDraftChanges)
                root.loadDraft()
        }
        function onLayoutSaved() { root.loadDraft() }
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

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: ControlCenterBackend.error !== ""
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: ControlCenterBackend.error
                    typeRole: "body"; typeSize: "medium"; color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: ControlCenterBackend.available
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

        MeoQuickSettingsEditor {
            id: tileEditor
            width: parent.width
            visible: ControlCenterBackend.available
            title: qsTr("Edit tiles")
            subtitle: qsTr("Select tiles to rearrange and resize")
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
                    text: qsTr("Density changes only the Meo Quick Settings tiles. It does not change text scale or the rest of Plasma.")
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
                    text: qsTr("Direct Meo configuration")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Changes are written to the active Meo top-bar applet through Plasma Shell and reloaded immediately. This page never edits Plasma configuration files directly or redirects this task to a KDE settings module.")
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
            description: qsTr("Keep the Meo top-bar applet in the active Plasma layout, then refresh this page. No fallback writes are made to an arbitrary Plasma configuration file.")
            actionText: qsTr("Refresh")
            onActionClicked: ControlCenterBackend.refresh()
        }
    }
}
