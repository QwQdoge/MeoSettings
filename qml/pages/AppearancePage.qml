import QtQuick
import QtQuick.Controls
import MeoUI
import MeoKDE 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function dynamicSourceTitle(source) {
        switch (source) {
        case "wallpaper": return qsTr("Wallpaper")
        case "manual": return qsTr("Manual color")
        default: return qsTr("KDE accent")
        }
    }

    function dynamicSourceDescription(source) {
        switch (source) {
        case "wallpaper":
            return qsTr("Sample the configured local desktop image, then generate one complete MD3 scheme")
        case "manual":
            return qsTr("Use a selected seed color, then generate one complete MD3 scheme")
        default:
            return qsTr("Use KDE’s active accent, then generate one complete MD3 scheme")
        }
    }

    function moduleRow(id, title, subtitle, icon) {
        const available = KcmBridge.isAvailable(id)
        return {
            "title": title,
            "subtitle": available ? subtitle : qsTr("This advanced system tool is not installed"),
            "icon": icon,
            "tone": "tertiary",
            "route": "kcm:" + id,
            "enabled": available,
            "trailingKind": "choice",
            "trailingText": qsTr("Advanced")
        }
    }

    function applyDynamicColor() {
        if (!DynamicColorBackend.available || DynamicColorBackend.busy)
            return
        dynamicColorApply.open()
    }

    function chooseDynamicColorSource() {
        if (!DynamicColorBackend.available || DynamicColorBackend.busy)
            return
        dynamicColorSource.selectedSource = DynamicColorBackend.sourceMode || "accent"
        dynamicColorSource.open()
    }

    readonly property string appearanceMode: MeoTheme.isDarkMode ? qsTr("Dark") : qsTr("Light")
    readonly property string motionSummary: MeoTheme.reduceMotion
                                          ? qsTr("Reduced motion")
                                          : qsTr("System motion ×%1").arg(Number(MeoTheme.motionScale).toFixed(1))
    readonly property string typeSummary: qsTr("%1 · %2%")
                                            .arg(MeoTheme.fontFamily || qsTr("System font"))
                                            .arg(Math.round(MeoTheme.fontScale * 100))

    readonly property var currentRows: [
        {
            "id": "dynamic-color",
            "title": qsTr("Dynamic color"),
            "subtitle": MeoShellTheme.ready
                        ? qsTr("Complete Material colors are supplied by the active KDE session from %1")
                              .arg(root.dynamicSourceTitle(MeoTheme.dynamicColorSourceId))
                        : qsTr("The active KDE session has not supplied a complete dynamic color scheme"),
            "icon": "colors",
            "tone": "tertiary",
            "trailingKind": "choice",
            "trailingText": MeoShellTheme.ready
                            ? root.dynamicSourceTitle(MeoTheme.dynamicColorSourceId)
                            : qsTr("Unavailable"),
            "enabled": MeoShellTheme.ready
        },
        {
            "title": qsTr("System appearance"),
            "subtitle": qsTr("%1 mode from the KDE session").arg(root.appearanceMode),
            "icon": MeoTheme.isDarkMode ? "dark_mode" : "light_mode",
            "tone": "primary",
            "trailingKind": "status",
            "trailingText": root.appearanceMode,
            "interactive": false
        },
        {
            "title": qsTr("Interface text"),
            "subtitle": root.typeSummary,
            "icon": "format_size",
            "tone": "secondary",
            "trailingKind": "status",
            "trailingText": qsTr("System"),
            "interactive": false
        },
        {
            "title": qsTr("Motion"),
            "subtitle": root.motionSummary,
            "icon": "animation",
            "tone": "secondary",
            "trailingKind": "status",
            "trailingText": MeoTheme.reduceMotion ? qsTr("Reduced") : qsTr("Enabled"),
            "interactive": false
        }
    ]

    readonly property var configurationRows: [
        {
            "id": "dynamic-color-source",
            "title": qsTr("Dynamic color source"),
            "subtitle": DynamicColorBackend.busy
                        ? qsTr("Applying a complete Material color scheme…")
                        : root.dynamicSourceDescription(DynamicColorBackend.sourceMode),
            "icon": DynamicColorBackend.sourceMode === "wallpaper" ? "wallpaper"
                    : (DynamicColorBackend.sourceMode === "manual" ? "colorize" : "palette"),
            "tone": "tertiary",
            "enabled": DynamicColorBackend.available && !DynamicColorBackend.busy,
            "trailingKind": "choice",
            "trailingText": root.dynamicSourceTitle(DynamicColorBackend.sourceMode)
        },
        {
            "id": "apply-dynamic-color",
            "title": qsTr("Reapply dynamic color"),
            "subtitle": DynamicColorBackend.busy
                        ? qsTr("Applying the selected source and appearance mode…")
                        : (DynamicColorBackend.available
                           ? qsTr("Regenerate the complete Meo HCT/Material scheme from %1")
                                 .arg(root.dynamicSourceTitle(DynamicColorBackend.sourceMode))
                           : qsTr("Install the Meo dynamic-color generator to apply a desktop scheme")),
            "icon": "auto_awesome",
            "tone": "tertiary",
            "enabled": DynamicColorBackend.available && !DynamicColorBackend.busy,
            "trailingKind": "action",
            "actionText": DynamicColorBackend.busy ? qsTr("Applying…") : qsTr("Apply")
        },
        root.moduleRow("kcm_lookandfeel", qsTr("Global theme"), qsTr("Advanced workspace appearance and compatibility settings"), "palette"),
        root.moduleRow("kcm_colors", qsTr("Color scheme & contrast"), qsTr("Advanced color-scheme and contrast settings"), "contrast"),
        root.moduleRow("kcm_wallpaper", qsTr("Wallpaper"), qsTr("Desktop backgrounds and wallpaper behavior"), "wallpaper"),
        root.moduleRow("kcm_icons", qsTr("Icons"), qsTr("Installed KDE icon themes"), "apps"),
        root.moduleRow("kcm_fonts", qsTr("Fonts"), qsTr("Font rendering and system-wide type configuration"), "format_size")
    ]

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Appearance")
        subtitle: qsTr("Choose a wallpaper, KDE accent, or manual seed once; Meo generates one shared HCT/Material scheme for every Meo surface.")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Current appearance")
            subtitle: qsTr("These are real session values. MeoUI and Meo shell surfaces share the same generated role table.")
            model: root.currentRows
            onRowActivated: (index, row) => {
                if (row.id === "dynamic-color")
                    dynamicColorDetails.open()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Meo appearance")
            subtitle: qsTr("A source selection is explicit and confirmation-gated. Broader KDE compatibility controls remain available when their workflow is more complex.")
            model: root.configurationRows
            onRowActivated: (index, row) => {
                if (row.id === "dynamic-color-source")
                    root.chooseDynamicColorSource()
                else if (row.id === "apply-dynamic-color")
                    root.applyDynamicColor()
                else if (row.enabled && row.route)
                    root.navigateTo(row.route)
            }
            onRowActionTriggered: (index, row) => {
                if (row.id === "apply-dynamic-color")
                    root.applyDynamicColor()
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: DynamicColorBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: DynamicColorBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale

                MeoText {
                    width: parent.width
                    text: qsTr("One dynamic scheme, not a parallel palette")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo does not interpolate colors in QML or copy a theme file. The installed MeoKDE generator resolves one approved source—KDE accent, the configured local wallpaper, or a manual seed—then produces the complete Material role table with HCT/CAM16 and applies it as one KDE scheme update. Every Meo application consumes that same table.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: dynamicColorDetails
        popupParent: Overlay.overlay
        title: qsTr("Dynamic color")
        subtitle: qsTr("Read-only session diagnostics. Color generation remains in the KDE platform bridge, while MeoUI consumes the complete Material role table.")
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: colorFacts.implicitHeight + 32 * MeoTheme.globalScale

                MeoSettingsGroup {
                    id: colorFacts
                    width: parent.width
                    y: 16 * MeoTheme.globalScale
                    title: qsTr("Session facts")
                    model: [
                        {
                            "title": qsTr("Color source"),
                            "subtitle": MeoTheme.dynamicColorSourceId || qsTr("Unavailable"),
                            "icon": "account_tree", "tone": "neutral",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Color table"),
                            "subtitle": MeoTheme.hasCompleteColorScheme(MeoTheme.dynamicColorScheme)
                                        ? qsTr("Complete Material role table")
                                        : qsTr("No complete table is installed"),
                            "icon": "check_circle", "tone": "primary",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Revision"),
                            "subtitle": qsTr("Session revision %1").arg(MeoTheme.colorSchemeRevision),
                            "icon": "refresh", "tone": "secondary",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Fallback policy"),
                            "subtitle": qsTr("Incomplete color tables are rejected instead of mixing dynamic and fixed colors."),
                            "icon": "shield", "tone": "tertiary",
                            "trailingKind": "none", "interactive": false
                        }
                    ]
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: dynamicColorSource
        popupParent: Overlay.overlay
        property string selectedSource: "accent"
        property var manualField: null
        closeOnAccept: false
        title: qsTr("Choose dynamic color source")
        subtitle: qsTr("The source selects one seed only. MeoKDE will generate the complete MD3/HCT role table and notify all Meo surfaces; it does not restart Plasma or KWin.")
        acceptText: DynamicColorBackend.busy ? qsTr("Applying…") : qsTr("Apply")
        rejectText: qsTr("Cancel")
        acceptEnabled: DynamicColorBackend.available && !DynamicColorBackend.busy
        onAccepted: {
            let manualSeed = ""
            if (selectedSource === "manual") {
                if (!manualField || !manualField.commit())
                    return
                manualSeed = manualField.color.toString()
            }
            DynamicColorBackend.applySource(selectedSource, manualSeed)
            close()
        }

        readonly property var sourceRows: [
            {
                "id": "accent",
                "title": qsTr("KDE accent"),
                "subtitle": qsTr("Follow the current KDE accent color"),
                "icon": "palette", "tone": "tertiary",
                "trailingKind": "radio", "checked": selectedSource === "accent",
                "selected": selectedSource === "accent"
            },
            {
                "id": "wallpaper",
                "title": qsTr("Wallpaper"),
                "subtitle": qsTr("Sample the currently configured local desktop image"),
                "icon": "wallpaper", "tone": "primary",
                "trailingKind": "radio", "checked": selectedSource === "wallpaper",
                "selected": selectedSource === "wallpaper"
            },
            {
                "id": "manual",
                "title": qsTr("Manual color"),
                "subtitle": qsTr("Choose an exact color seed"),
                "icon": "colorize", "tone": "secondary",
                "trailingKind": "radio", "checked": selectedSource === "manual",
                "selected": selectedSource === "manual"
            }
        ]

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: sourceContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: sourceContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 16 * MeoTheme.globalScale

                    MeoSettingsGroup {
                        width: parent.width
                        title: qsTr("Source")
                        subtitle: qsTr("Only one source is active at a time")
                        model: dynamicColorSource.sourceRows
                        onRowToggled: (index, checked, row) => {
                            if (checked)
                                dynamicColorSource.selectedSource = row.id
                        }
                    }

                    MeoColorField {
                        id: manualColorField
                        width: parent.width
                        visible: dynamicColorSource.selectedSource === "manual"
                        label: qsTr("Manual seed")
                        helperText: qsTr("This is a seed, not a separate app palette")
                        color: DynamicColorBackend.manualColor || MeoShellTheme.accentColor
                        Component.onCompleted: dynamicColorSource.manualField = manualColorField
                        Component.onDestruction: {
                            if (dynamicColorSource.manualField === manualColorField)
                                dynamicColorSource.manualField = null
                        }
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"
                        visible: dynamicColorSource.selectedSource === "wallpaper"

                        MeoText {
                            width: parent.width
                            text: qsTr("Wallpaper mode uses the configured local org.kde.image wallpaper. If Plasma is using a slideshow, remote source, or missing file, Meo will show an error instead of silently using a different color.")
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: dynamicColorApply
        popupParent: Overlay.overlay
        title: qsTr("Reapply dynamic color")
        subtitle: qsTr("This will regenerate and select the Meo HCT/Material color scheme from the remembered %1 source and current light/dark mode. It changes the desktop palette, but does not restart Plasma or KWin.")
                  .arg(root.dynamicSourceTitle(DynamicColorBackend.sourceMode))
        acceptText: qsTr("Apply")
        rejectText: qsTr("Cancel")
        acceptEnabled: DynamicColorBackend.available && !DynamicColorBackend.busy
        onAccepted: DynamicColorBackend.applySource(DynamicColorBackend.sourceMode,
                                                     DynamicColorBackend.manualColor)

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: applyFacts.implicitHeight + 32 * MeoTheme.globalScale

                MeoSettingsGroup {
                    id: applyFacts
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    title: qsTr("What will be used")
                    model: [
                        {
                            "title": qsTr("Color source"),
                            "subtitle": root.dynamicSourceTitle(DynamicColorBackend.sourceMode),
                            "icon": DynamicColorBackend.sourceMode === "wallpaper" ? "wallpaper"
                                    : (DynamicColorBackend.sourceMode === "manual" ? "colorize" : "palette"),
                            "tone": "tertiary",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Appearance mode"),
                            "subtitle": root.appearanceMode,
                            "icon": MeoTheme.isDarkMode ? "dark_mode" : "light_mode",
                            "tone": "primary", "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Scope"),
                            "subtitle": qsTr("All Meo Material roles and the active KDE color scheme; all Meo applications consume the same role table"),
                            "icon": "palette", "tone": "secondary",
                            "trailingKind": "none", "interactive": false
                        }
                    ]
                }
            }
        }
    }

}
