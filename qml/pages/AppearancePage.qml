import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
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

    function applicationIconShapeTitle(shape) {
        switch (shape) {
        case "circle": return qsTr("Circle")
        case "squircle": return qsTr("Squircle")
        case "rounded": return qsTr("Rounded square")
        default: return qsTr("Pixel flower")
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

    function configureApplicationIcons() {
        if (!ApplicationIconBackend.available || ApplicationIconBackend.busy)
            return
        applicationIconStudio.selectedStyle = ApplicationIconBackend.style
        applicationIconStudio.selectedShape = ApplicationIconBackend.shape
        applicationIconStudio.promptText = ApplicationIconBackend.prompt
        applicationIconStudio.applyScope = "all"
        applicationIconStudio.selectedApplicationIds = []
        applicationIconStudio.selectedCredentialId = ""
        applicationIconStudio.aiModel = ""
        if (AccountBackend.signedIn)
            AccountBackend.refreshAiCredentials()
        applicationIconStudio.open()
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

    readonly property var meoConfigurationRows: [
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
        {
            "id": "application-icons",
            "title": qsTr("Application icon style"),
            "subtitle": ApplicationIconBackend.busy
                        ? qsTr("Updating application icons without changing system status icons…")
                        : (ApplicationIconBackend.available
                           ? qsTr("%1 · %2 · active Material light/dark palette")
                                 .arg(root.applicationIconShapeTitle(ApplicationIconBackend.shape))
                                 .arg(ApplicationIconBackend.style === "monet" || ApplicationIconBackend.style === "pure"
                                      ? qsTr("Monet")
                                      : (ApplicationIconBackend.style === "mono" ? qsTr("Black & white") : qsTr("Original")))
                           : qsTr("Install the Meo application icon studio to create per-app Pixel-style icons")),
            "icon": "apps",
            "tone": "primary",
            "enabled": ApplicationIconBackend.available && !ApplicationIconBackend.busy,
            "trailingKind": "choice",
            "trailingText": ApplicationIconBackend.busy ? qsTr("Updating…") : qsTr("Customize")
        }
    ]

    // Keep every supported Plasma 6 appearance surface discoverable here.
    // Meo owns the shared dynamic palette and application-icon workflow;
    // specialized KDE modules retain authority for their mature theme editors.
    readonly property var kdeAppearanceRows: [
        root.moduleRow("kcm_lookandfeel", qsTr("Global theme"), qsTr("Apply a complete KDE look-and-feel package"), "palette"),
        root.moduleRow("kcm_colors", qsTr("Color scheme & contrast"), qsTr("Inspect installed color schemes and compatibility colors"), "contrast"),
        root.moduleRow("kcm_wallpaper", qsTr("Wallpaper"), qsTr("Desktop backgrounds, positioning, slideshows, and plugins"), "wallpaper"),
        root.moduleRow("kcm_style", qsTr("Application style"), qsTr("Widget style, toolbar labels, and application behavior"), "web_asset"),
        root.moduleRow("kcm_desktoptheme", qsTr("Plasma style"), qsTr("Panel, widget, popup, and notification appearance"), "dashboard"),
        root.moduleRow("kcm_icons", qsTr("System icon theme"), qsTr("Installed KDE icon themes for apps and the workspace"), "apps"),
        root.moduleRow("kcm_cursortheme", qsTr("Cursors"), qsTr("Pointer theme, size, and animation"), "mouse"),
        root.moduleRow("kcm_fonts", qsTr("Fonts"), qsTr("Font families, rendering, hinting, and system-wide sizing"), "format_size"),
        root.moduleRow("kcm_kwindecoration", qsTr("Window decorations"), qsTr("Title bars, borders, buttons, and decoration themes"), "select_window"),
        root.moduleRow("kcm_splashscreen", qsTr("Welcome screen"), qsTr("Plasma session startup animation"), "animation"),
        root.moduleRow("kcm_soundtheme", qsTr("Sound theme"), qsTr("Notification and desktop event sounds"), "music_note")
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
            model: root.meoConfigurationRows
            onRowActivated: (index, row) => {
                if (row.id === "dynamic-color-source")
                    root.chooseDynamicColorSource()
                else if (row.id === "apply-dynamic-color")
                    root.applyDynamicColor()
                else if (row.id === "application-icons")
                    root.configureApplicationIcons()
                else if (row.enabled && row.route)
                    root.navigateTo(row.route)
            }
            onRowActionTriggered: (index, row) => {
                if (row.id === "apply-dynamic-color")
                    root.applyDynamicColor()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("KDE desktop details")
            subtitle: qsTr("Every installed Plasma 6 appearance editor stays connected to Meo Settings. Changes made there are read back by the shared Meo dynamic-color bridge.")
            model: root.kdeAppearanceRows
            onRowActivated: (index, row) => {
                if (row.enabled && row.route)
                    root.navigateTo(row.route)
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
        id: applicationIconStudio
        popupParent: Overlay.overlay
        property string selectedStyle: "monet"
        property string selectedShape: "circle"
        property string promptText: ""
        property string applyScope: "all"
        property var selectedApplicationIds: []
        property string selectedCredentialId: ""
        property string aiModel: ""
        property string shownConsentRequest: ""
        property var aiQueue: []
        property int aiQueueIndex: -1
        property string stagedAiDesktopId: ""
        readonly property string previewIconSource:
            ApplicationIconBackend.applications.length > 0
            ? String(ApplicationIconBackend.applications[0].icon || "applications-all-symbolic")
            : "applications-all-symbolic"

        function isApplicationSelected(desktopId) {
            return selectedApplicationIds.indexOf(desktopId) !== -1
        }

        function toggleApplication(desktopId) {
            const next = selectedApplicationIds.slice()
            const position = next.indexOf(desktopId)
            if (position === -1)
                next.push(desktopId)
            else
                next.splice(position, 1)
            selectedApplicationIds = next
        }

        function aiApplications() {
            const applications = ApplicationIconBackend.applications || []
            if (applyScope === "all")
                return applications.slice()
            const selected = []
            for (let index = 0; index < applications.length; ++index) {
                if (selectedApplicationIds.indexOf(applications[index].desktopId) !== -1)
                    selected.push(applications[index])
            }
            return selected
        }

        function startAiQueue() {
            const applications = aiApplications()
            if (applications.length === 0)
                return
            if (!ApplicationIconBackend.beginAiBatch())
                return
            aiQueue = applications
            aiQueueIndex = 0
            stagedAiDesktopId = ""
            AccountBackend.prepareIconImageBatch(
                applications, selectedCredentialId, aiModel, promptText)
        }

        function stopAiQueue(cancelStaging) {
            aiQueue = []
            aiQueueIndex = -1
            stagedAiDesktopId = ""
            if (cancelStaging)
                ApplicationIconBackend.cancelAiBatch()
        }
        closeOnAccept: false
        title: qsTr("Application Icon Studio")
        subtitle: qsTr("Creates unique, user-local icons for applications only. They follow Meo's active light/dark dynamic palette. Wi‑Fi, microphone, volume, and every other system icon keep the current KDE theme.")
        acceptText: ApplicationIconBackend.busy ? qsTr("Applying…") : qsTr("Apply to applications")
        rejectText: qsTr("Close")
        acceptEnabled: ApplicationIconBackend.available && !ApplicationIconBackend.busy
                       && promptText.trim().length > 0
                       && (applyScope === "all" || selectedApplicationIds.length > 0)
        onAccepted: {
            if (applyScope === "selected")
                ApplicationIconBackend.applyToApplications(
                    selectedApplicationIds, selectedStyle, selectedShape, promptText)
            else
                ApplicationIconBackend.apply(selectedStyle, selectedShape, promptText)
            close()
        }

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: applicationIconContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: applicationIconContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 16 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        text: qsTr("Style")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }

                    Row {
                        width: parent.width
                        spacing: 8 * MeoTheme.globalScale

                        MeoButton {
                            text: qsTr("Monet")
                            type: "outlined"
                            selected: applicationIconStudio.selectedStyle === "monet"
                            onClicked: applicationIconStudio.selectedStyle = "monet"
                        }
                        MeoButton {
                            text: qsTr("Original")
                            type: "outlined"
                            selected: applicationIconStudio.selectedStyle === "original"
                            onClicked: applicationIconStudio.selectedStyle = "original"
                        }
                        MeoButton {
                            text: qsTr("Black & white")
                            type: "outlined"
                            selected: applicationIconStudio.selectedStyle === "mono"
                            onClicked: applicationIconStudio.selectedStyle = "mono"
                        }
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"

                        MeoText {
                            width: parent.width
                            text: applicationIconStudio.selectedStyle === "monet"
                                  ? qsTr("Default Pixel style: keeps the recognizable silhouette and internal cuts, then recolors them with three wallpaper-derived Material tones inside one container.")
                                  : (applicationIconStudio.selectedStyle === "mono"
                                     ? qsTr("Uses a high-contrast black or white container while retaining the original mark where possible.")
                                     : qsTr("Preserves the original application artwork and colors inside the selected single container."))
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }

                    MeoText {
                        width: parent.width
                        text: qsTr("Icon shape")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }

                    Flow {
                        width: parent.width
                        spacing: 12 * MeoTheme.globalScale

                        Repeater {
                            model: [
                                { "id": "circle", "label": qsTr("Circle") },
                                { "id": "pixel", "label": qsTr("Pixel flower") },
                                { "id": "squircle", "label": qsTr("Squircle") },
                                { "id": "rounded", "label": qsTr("Rounded square") }
                            ]

                            delegate: Item {
                                required property var modelData
                                readonly property bool selected: applicationIconStudio.selectedShape === modelData.id
                                width: 132 * MeoTheme.globalScale
                                height: 116 * MeoTheme.globalScale

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 16 * MeoTheme.globalScale
                                    color: selected ? MeoTheme.primaryContainer : MeoTheme.surfaceContainerLow
                                    border.width: selected ? 2 * MeoTheme.globalScale : 1 * MeoTheme.globalScale
                                    border.color: selected ? MeoTheme.primary : MeoTheme.outlineVariant

                                    Rectangle {
                                        id: shapeCore
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        y: 16 * MeoTheme.globalScale
                                        width: 58 * MeoTheme.globalScale
                                        height: width
                                        radius: modelData.id === "circle" || modelData.id === "pixel"
                                                ? width / 2
                                                : (modelData.id === "squircle" ? width * 0.31 : width * 0.19)
                                        color: selected ? MeoTheme.primary : MeoTheme.surface

                                        Repeater {
                                            model: modelData.id === "pixel" ? 8 : 0
                                            delegate: Rectangle {
                                                required property int index
                                                readonly property real angle: index * Math.PI / 4
                                                width: 22 * MeoTheme.globalScale
                                                height: width
                                                radius: width / 2
                                                x: shapeCore.width / 2 + Math.cos(angle) * 18 * MeoTheme.globalScale - width / 2
                                                y: shapeCore.height / 2 + Math.sin(angle) * 18 * MeoTheme.globalScale - height / 2
                                                color: shapeCore.color
                                            }
                                        }

                                        Kirigami.Icon {
                                            anchors.centerIn: parent
                                            width: 30 * MeoTheme.globalScale
                                            height: width
                                            source: applicationIconStudio.previewIconSource
                                        }
                                    }

                                    MeoText {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 12 * MeoTheme.globalScale
                                        horizontalAlignment: Text.AlignHCenter
                                        text: modelData.label
                                        typeRole: "label"
                                        typeSize: "small"
                                        emphasized: selected
                                        color: MeoTheme.contentOnSurface
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: applicationIconStudio.selectedShape = modelData.id
                                }
                            }
                        }
                    }

                    MeoText {
                        width: parent.width
                        text: qsTr("The app’s own mark remains visible inside every shape. The Dock does not add a second opaque plate; Circle, Pixel flower, Squircle, and Rounded square are controlled here.")
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }

                    MeoText {
                        width: parent.width
                        text: qsTr("Applications")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }

                    Row {
                        spacing: 8 * MeoTheme.globalScale

                        MeoButton {
                            text: qsTr("All applications")
                            type: "outlined"
                            selected: applicationIconStudio.applyScope === "all"
                            onClicked: applicationIconStudio.applyScope = "all"
                        }
                        MeoButton {
                            text: qsTr("Choose applications")
                            type: "outlined"
                            selected: applicationIconStudio.applyScope === "selected"
                            onClicked: applicationIconStudio.applyScope = "selected"
                        }
                    }

                    MeoCard {
                        width: parent.width
                        height: applicationIconStudio.applyScope === "selected"
                                ? 248 * MeoTheme.globalScale : 0
                        visible: applicationIconStudio.applyScope === "selected"
                        type: "outlined"

                        ListView {
                            anchors.fill: parent
                            clip: true
                            spacing: 4 * MeoTheme.globalScale
                            model: ApplicationIconBackend.applications

                            delegate: Item {
                                id: appChoice
                                required property var modelData
                                readonly property bool selected:
                                    applicationIconStudio.isApplicationSelected(modelData.desktopId)
                                width: ListView.view.width
                                height: 52 * MeoTheme.globalScale

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 14 * MeoTheme.globalScale
                                    color: appChoice.selected
                                           ? MeoTheme.secondaryContainer
                                           : "transparent"

                                    Kirigami.Icon {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10 * MeoTheme.globalScale
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 34 * MeoTheme.globalScale
                                        height: width
                                        source: appChoice.modelData.icon
                                    }

                                    MeoText {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 56 * MeoTheme.globalScale
                                        anchors.right: selectionMark.left
                                        anchors.rightMargin: 8 * MeoTheme.globalScale
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: appChoice.modelData.name
                                        typeRole: "body"
                                        typeSize: "medium"
                                        emphasized: appChoice.selected
                                        elide: Text.ElideRight
                                        color: appChoice.selected
                                               ? MeoTheme.onSecondaryContainer
                                               : MeoTheme.contentOnSurface
                                    }

                                    MeoIcon {
                                        id: selectionMark
                                        anchors.right: parent.right
                                        anchors.rightMargin: 12 * MeoTheme.globalScale
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: appChoice.selected ? "check_circle" : "circle"
                                        size: 22 * MeoTheme.globalScale
                                        color: appChoice.selected
                                               ? MeoTheme.primary
                                               : MeoTheme.outline
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: applicationIconStudio.toggleApplication(
                                        appChoice.modelData.desktopId)
                                }
                            }
                        }
                    }

                    MeoTextField {
                        width: parent.width
                        label: qsTr("AI style prompt")
                        placeholder: qsTr("Add an optional visual requirement")
                        helperText: qsTr("Stored locally for deterministic styles. AI generation sends it only after the separate Account provider/model/data consent sheet is confirmed.")
                        showClearButton: true
                        maxLength: 4000
                        text: applicationIconStudio.promptText
                        onTextEdited: applicationIconStudio.promptText = text
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"

                        Column {
                            width: parent.width
                            spacing: 10 * MeoTheme.globalScale

                            MeoText {
                                width: parent.width
                                text: qsTr("AI icon generation")
                                typeRole: "title"
                                typeSize: "small"
                                emphasized: true
                                color: MeoTheme.contentOnSurface
                            }
                            MeoText {
                                width: parent.width
                                text: qsTr("AI credentials stay encrypted in Meo Account. Meo prepares payload-bound permission for every selected app, shows one batch confirmation, then stages the complete Easel/Monet pack for preview. Nothing is replaced until you apply the pack.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                            ComboBox {
                                width: parent.width
                                visible: AccountBackend.aiCredentials.length > 0
                                model: AccountBackend.aiCredentials
                                textRole: "displayName"
                                valueRole: "id"
                                onCurrentValueChanged: {
                                    applicationIconStudio.selectedCredentialId = String(currentValue || "")
                                    const credentials = AccountBackend.aiCredentials || []
                                    if (currentIndex >= 0 && currentIndex < credentials.length)
                                        applicationIconStudio.aiModel = credentials[currentIndex].defaultModel || ""
                                }
                            }
                            MeoTextField {
                                width: parent.width
                                visible: AccountBackend.aiCredentials.length > 0
                                label: qsTr("Image model")
                                placeholder: qsTr("Model configured for this connection")
                                text: applicationIconStudio.aiModel
                                onTextEdited: applicationIconStudio.aiModel = text
                            }
                            Flow {
                                width: parent.width
                                spacing: 10 * MeoTheme.globalScale
                                visible: ApplicationIconBackend.aiBatchPreviews.length > 0

                                Repeater {
                                    model: ApplicationIconBackend.aiBatchPreviews

                                    delegate: Column {
                                        required property var modelData
                                        width: 104 * MeoTheme.globalScale
                                        spacing: 4 * MeoTheme.globalScale

                                        Image {
                                            width: 88 * MeoTheme.globalScale
                                            height: width
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            source: modelData.preview
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                        }
                                        MeoText {
                                            width: parent.width
                                            text: modelData.name
                                            typeRole: "label"
                                            typeSize: "small"
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                            color: MeoTheme.contentOnSurface
                                        }
                                    }
                                }
                            }
                            MeoButton {
                                text: AccountBackend.aiBusy
                                      ? qsTr("Generating…")
                                      : (applicationIconStudio.applyScope === "all"
                                         ? qsTr("Generate all with AI")
                                         : qsTr("Generate selected with AI"))
                                type: "filled"
                                enabled: !AccountBackend.aiBusy
                                         && AccountBackend.signedIn
                                         && AccountBackend.aiCredentials.length > 0
                                         && applicationIconStudio.aiApplications().length > 0
                                         && applicationIconStudio.selectedCredentialId !== ""
                                         && applicationIconStudio.aiModel.trim() !== ""
                                         && applicationIconStudio.promptText.trim() !== ""
                                onClicked: {
                                    applicationIconStudio.startAiQueue()
                                }
                            }
                            MeoText {
                                width: parent.width
                                visible: applicationIconStudio.aiQueueIndex >= 0
                                text: qsTr("AI pack %1 of %2 staged · one batch confirmation")
                                      .arg(ApplicationIconBackend.aiBatchPreviews.length)
                                      .arg(applicationIconStudio.aiQueue.length)
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                            }
                            MeoButton {
                                text: ApplicationIconBackend.busy
                                      ? qsTr("Applying complete pack…")
                                      : qsTr("Apply complete AI icon pack")
                                type: "tonal"
                                visible: AccountBackend.aiState === "batch_ready"
                                         && ApplicationIconBackend.aiBatchPreviews.length > 0
                                enabled: !ApplicationIconBackend.busy
                                onClicked: {
                                    ApplicationIconBackend.applyAiBatch()
                                    AccountBackend.clearGeneratedIconImage()
                                    applicationIconStudio.aiQueue = []
                                    applicationIconStudio.aiQueueIndex = -1
                                }
                            }
                            MeoButton {
                                text: qsTr("Discard staged AI pack")
                                type: "text"
                                visible: ApplicationIconBackend.aiBatchPreviews.length > 0
                                         && !ApplicationIconBackend.busy
                                onClicked: {
                                    AccountBackend.clearGeneratedIconImage()
                                    applicationIconStudio.stopAiQueue(true)
                                }
                            }
                            MeoButton {
                                text: qsTr("Manage AI connections")
                                type: "tonal"
                                enabled: AccountBackend.available
                                onClicked: AccountBackend.openHostedAction("ai_providers")
                            }
                        }
                    }

                    MeoButton {
                        text: qsTr("Restore original application icons")
                        type: "text"
                        enabled: ApplicationIconBackend.available && !ApplicationIconBackend.busy
                        onClicked: {
                            ApplicationIconBackend.reset()
                            applicationIconStudio.close()
                        }
                    }

                    MeoText {
                        width: parent.width
                        text: ApplicationIconBackend.error
                        visible: text !== ""
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.error
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Connections {
        target: AccountBackend

        function onChanged() {
            const consent = AccountBackend.aiConsent || {}
            const requestId = String(consent.requestId || "")
            if ((AccountBackend.aiState === "consent_ready"
                    || AccountBackend.aiState === "batch_consent_ready") && requestId !== ""
                    && requestId !== applicationIconStudio.shownConsentRequest) {
                applicationIconStudio.shownConsentRequest = requestId
                aiIconConsent.open()
            }
            if ((AccountBackend.aiState === "denied"
                    || AccountBackend.aiState === "failed")
                    && applicationIconStudio.aiQueueIndex >= 0) {
                applicationIconStudio.stopAiQueue(true)
                if (!AccountBackend.aiBusy)
                    AccountBackend.clearGeneratedIconImage()
            }
            if (AccountBackend.aiState === "batch_image_ready"
                    && AccountBackend.aiImageSource !== ""
                    && AccountBackend.aiTargetDesktopId !== applicationIconStudio.stagedAiDesktopId) {
                applicationIconStudio.stagedAiDesktopId = AccountBackend.aiTargetDesktopId
                const staged = ApplicationIconBackend.stageAiImage(
                    AccountBackend.aiTargetDesktopId,
                    AccountBackend.aiTargetApplicationName,
                    AccountBackend.aiImageSource,
                    applicationIconStudio.selectedShape,
                    applicationIconStudio.promptText)
                if (staged) {
                    applicationIconStudio.aiQueueIndex = ApplicationIconBackend.aiBatchPreviews.length
                    AccountBackend.continuePreparedIconImageBatch()
                } else {
                    applicationIconStudio.stopAiQueue(true)
                    AccountBackend.clearGeneratedIconImage()
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: aiIconConsent
        popupParent: Overlay.overlay
        title: AccountBackend.aiState === "batch_consent_ready"
               ? qsTr("Generate this application icon pack?")
               : qsTr("Generate one application icon?")
        subtitle: qsTr("Confirm the exact Account-owned request once. Each image remains payload-bound and the provider key never enters Meo Settings.")
        acceptText: qsTr("Confirm and generate pack")
        rejectText: qsTr("Deny")
        acceptEnabled: !AccountBackend.aiBusy
                       && String(AccountBackend.aiConsent.requestId || "") !== ""
        onAccepted: {
            if (Number(AccountBackend.aiConsent.itemCount || 0) > 0)
                AccountBackend.generatePreparedIconImageBatch()
            else
                AccountBackend.generatePreparedIconImage()
        }
        onRejected: {
            if (Number(AccountBackend.aiConsent.itemCount || 0) > 0)
                AccountBackend.denyPreparedIconImageBatch()
            else
                AccountBackend.denyPreparedIconImage()
            applicationIconStudio.stopAiQueue(true)
        }

        content: Component {
            Column {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale

                readonly property var consent: AccountBackend.aiConsent || ({})

                MeoText {
                    width: parent.width
                    text: qsTr("Provider: %1\nModel: %2\nDestination: %3\nPurpose: %4\nApplications: %5\nData: %6\nTotal prompt: %7 characters")
                          .arg(parent.consent.providerName || parent.consent.provider || qsTr("Unknown"))
                          .arg(parent.consent.model || qsTr("Unknown"))
                          .arg(parent.consent.destination || qsTr("Unknown"))
                          .arg(parent.consent.purpose || qsTr("Unknown"))
                          .arg(parent.consent.itemCount || 1)
                          .arg((parent.consent.dataCategories || []).join(", "))
                          .arg(parent.consent.promptCharacters || 0)
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurface
                    wrapMode: Text.WordWrap
                }
                MeoText {
                    width: parent.width
                    text: qsTr("The locked Easel/Monet constraints apply to every icon. Generated images are staged locally for whole-pack preview and are not installed until Apply is pressed.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
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
