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
        default: return qsTr("System accent")
        }
    }

    function dynamicSourceDescription(source) {
        switch (source) {
        case "wallpaper":
            return qsTr("Use the configured desktop wallpaper to create one shared color scheme")
        case "manual":
            return qsTr("Use a color you choose to create one shared color scheme")
        default:
            return qsTr("Use the current system accent to create one shared color scheme")
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
            "subtitle": available ? subtitle : qsTr("This advanced system setting is not available"),
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
        applicationIconStudio.resetAiPackForOpen()
        if (AccountBackend.signedIn)
            AccountBackend.refreshAiIconStyles()
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
                        ? qsTr("Colors come from %1 in this desktop session")
                              .arg(root.dynamicSourceTitle(MeoTheme.dynamicColorSourceId))
                        : qsTr("A complete Meo color scheme is not available in this session"),
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
            "subtitle": qsTr("The current desktop is using %1 mode").arg(root.appearanceMode),
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
                        ? qsTr("Applying your color scheme…")
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
            "title": qsTr("Reapply colors"),
            "subtitle": DynamicColorBackend.busy
                        ? qsTr("Applying the selected colors…")
                        : (DynamicColorBackend.available
                           ? qsTr("Apply a fresh color scheme from %1")
                                 .arg(root.dynamicSourceTitle(DynamicColorBackend.sourceMode))
                           : qsTr("Install the Meo color service to apply a desktop color scheme")),
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
                           ? qsTr("%1 · %2 · colors match the active light or dark mode")
                                 .arg(root.applicationIconShapeTitle(ApplicationIconBackend.shape))
                                 .arg(ApplicationIconBackend.style === "monet" || ApplicationIconBackend.style === "pure"
                                      ? qsTr("Meo Color")
                                      : (ApplicationIconBackend.style === "mono" ? qsTr("Monochrome") : qsTr("Original")))
                           : qsTr("Install Meo Icon Studio to customize icons for your apps")),
            "icon": "apps",
            "tone": "primary",
            "enabled": ApplicationIconBackend.available && !ApplicationIconBackend.busy,
            "trailingKind": "choice",
            "trailingText": ApplicationIconBackend.busy ? qsTr("Updating…") : qsTr("Customize")
        }
    ]

    // Meo owns the daily appearance workflow.  Specialized, platform-owned
    // editors remain available as an explicit compatibility escape hatch.
    readonly property var kdeAppearanceRows: [
        root.moduleRow("kcm_lookandfeel", qsTr("Global theme"), qsTr("Apply a complete platform look-and-feel package"), "palette"),
        root.moduleRow("kcm_colors", qsTr("Color scheme & contrast"), qsTr("Inspect installed platform color schemes and compatibility colors"), "contrast"),
        root.moduleRow("kcm_wallpaper", qsTr("Wallpaper"), qsTr("Desktop backgrounds, positioning, slideshows, and plugins"), "wallpaper"),
        root.moduleRow("kcm_style", qsTr("Application style"), qsTr("Widget style, toolbar labels, and application behavior"), "web_asset"),
        root.moduleRow("kcm_desktoptheme", qsTr("Plasma style"), qsTr("Panel, widget, popup, and notification appearance"), "dashboard"),
        root.moduleRow("kcm_icons", qsTr("System icon theme"), qsTr("Installed platform icon themes for apps and the workspace"), "apps"),
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
        subtitle: qsTr("Choose a wallpaper, system accent, or color. Meo uses one consistent color scheme across its apps.")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Current appearance")
            subtitle: qsTr("These are the colors, text, and motion settings active in this session.")
            model: root.currentRows
            onRowActivated: (index, row) => {
                if (row.id === "dynamic-color")
                    dynamicColorDetails.open()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Meo appearance")
            subtitle: qsTr("Choose where Meo gets its colors. The change is confirmed before it is applied across Meo.")
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
            title: qsTr("Advanced compatibility")
            subtitle: qsTr("Open a specialized system setting when it is not available here. Meo reflects the result in its color theme.")
            model: root.kdeAppearanceRows
            onRowActivated: (index, row) => {
                if (row.enabled && row.route)
                    root.navigateTo(row.route)
            }
        }

        Column {
            width: parent.width
            visible: DynamicColorBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Appearance needs attention")
                text: qsTr("Choose a wallpaper or color again, then try updating the appearance.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(DynamicColorBackend.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            Accessible.role: Accessible.StatusBar
            Accessible.name: qsTr("Color theme overview")
            Accessible.description: qsTr("Meo uses one trusted color source across its apps")

            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale

                MeoText {
                    width: parent.width
                    text: qsTr("One color scheme across Meo")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo uses one trusted source—your system accent, desktop wallpaper, or a color you choose—to create the colors it uses. Every Meo app uses the same color scheme.")
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
        subtitle: qsTr("View the color settings active in this session. The KDE platform service creates the color scheme, and Meo uses it.")
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
                            "title": qsTr("Color scheme"),
                            "subtitle": MeoTheme.hasCompleteColorScheme(MeoTheme.dynamicColorScheme)
                                        ? qsTr("A complete color scheme is ready")
                                        : qsTr("No complete color scheme is ready"),
                            "icon": "check_circle", "tone": "primary",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("Last update"),
                            "subtitle": qsTr("Session revision %1").arg(MeoTheme.colorSchemeRevision),
                            "icon": "refresh", "tone": "secondary",
                            "trailingKind": "none", "interactive": false
                        },
                        {
                            "title": qsTr("When colors are unavailable"),
                            "subtitle": qsTr("Meo waits for a complete color scheme instead of mixing old and new colors."),
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
        readonly property bool originalStyle: selectedStyle === "original"
        readonly property bool aiStyle: selectedStyle === "ai"
        property string promptText: ""
        property string applyScope: "all"
        property var selectedApplicationIds: []
        property string selectedAiStyleId: ""
        property string aiPackPhase: "idle"
        property string activeAiPackJobId: ""
        property string activeAiPackManifestKey: ""
        property string shownAiPackConsentRequest: ""
        property var activeAiApplicationIds: []
        property var appliedAiApplicationIds: []
        property string aiPackSelectionMessage: ""
        property bool aiPackCleanupStarted: false
        property bool releaseAfterAiPackCommit: false
        property bool undoingAiPack: false
        readonly property bool aiPackBusy: AccountBackend.aiIconPackBusy
                                           || ApplicationIconBackend.aiPackDescribing
                                           || ApplicationIconBackend.aiPackPreviewing
                                           || (aiPackPhase === "applying"
                                               && ApplicationIconBackend.busy)
                                           || (undoingAiPack
                                               && ApplicationIconBackend.busy)
        // Once identity descriptors leave Studio, scope, shape, and style are
        // immutable until the Account-owned job is released or discarded. A
        // terminal failure stays frozen too, so recovery cannot silently
        // change the request that the user approved.
        readonly property bool aiPackActive: aiStyle && (aiPackPhase !== "idle"
                                                          && aiPackPhase !== "styles_ready"
                                                          && aiPackPhase !== "applied"
                                                          || undoingAiPack)
        readonly property bool aiInputsFrozen: aiPackActive
        readonly property string iconStudioError: aiStyle && AccountBackend.error !== ""
                                                 ? AccountBackend.error
                                                 : ApplicationIconBackend.error
        readonly property string previewIconSource:
            ApplicationIconBackend.applications.length > 0
            ? String(ApplicationIconBackend.applications[0].icon || "applications-all-symbolic")
            : "applications-all-symbolic"

        function isApplicationSelected(desktopId) {
            return selectedApplicationIds.indexOf(desktopId) !== -1
        }

        function toggleApplication(desktopId) {
            if (aiInputsFrozen)
                return
            const next = selectedApplicationIds.slice()
            const position = next.indexOf(desktopId)
            if (position === -1) {
                if (aiStyle && next.length >= 128) {
                    aiPackSelectionMessage = qsTr("Choose no more than 128 applications for one AI icon pack.")
                    return
                }
                next.push(desktopId)
            } else {
                next.splice(position, 1)
            }
            aiPackSelectionMessage = ""
            selectedApplicationIds = next
        }

        function selectedAiApplicationIds() {
            const applications = ApplicationIconBackend.applications || []
            const ids = []
            for (let index = 0; index < applications.length; ++index) {
                const desktopId = String(applications[index].desktopId || "")
                if (desktopId === "" || ids.indexOf(desktopId) !== -1)
                    continue
                if (applyScope === "all"
                        || selectedApplicationIds.indexOf(desktopId) !== -1)
                    ids.push(desktopId)
            }
            return ids
        }

        function chooseDefaultAiStyle() {
            const styles = AccountBackend.aiIconStyles || []
            for (let index = 0; index < styles.length; ++index) {
                if (styles[index].available !== false
                        && String(styles[index].id || "") === selectedAiStyleId)
                    return
            }
            selectedAiStyleId = ""
            for (let index = 0; index < styles.length; ++index) {
                if (styles[index].available !== false) {
                    selectedAiStyleId = String(styles[index].id || "")
                    return
                }
            }
        }

        function startFormalAiPack() {
            const ids = selectedAiApplicationIds()
            if (ids.length < 1 || ids.length > 128) {
                aiPackSelectionMessage = qsTr("Choose from 1 to 128 applications for one AI icon pack.")
                return
            }
            if (selectedAiStyleId === "") {
                aiPackSelectionMessage = qsTr("Choose an approved AI icon style first.")
                return
            }
            aiPackSelectionMessage = ""
            ApplicationIconBackend.discardAttestedAiPack()
            activeAiApplicationIds = ids
            activeAiPackJobId = ""
            activeAiPackManifestKey = ""
            shownAiPackConsentRequest = ""
            aiPackCleanupStarted = false
            releaseAfterAiPackCommit = false
            appliedAiApplicationIds = []
            aiPackPhase = "describing"
            ApplicationIconBackend.describeAiPackApplications(ids)
        }

        function prepareFormalAiPackFromDescriptors() {
            if (aiPackPhase !== "describing" || ApplicationIconBackend.aiPackDescribing)
                return
            const descriptors = ApplicationIconBackend.aiPackDescriptors || []
            if (descriptors.length !== activeAiApplicationIds.length || descriptors.length < 1) {
                aiPackPhase = "failed"
                return
            }
            const items = []
            for (let index = 0; index < descriptors.length; ++index) {
                const descriptor = descriptors[index] || {}
                items.push({
                    "desktopId": String(descriptor.desktopId || ""),
                    "sourceIconHash": String(descriptor.canonicalIdentityHash || "")
                })
            }
            aiPackPhase = "preparing"
            AccountBackend.prepareAiIconMaterialPack({
                "schema": "org.meo.ai-icon-pack-request/v1",
                "contractVersion": 1,
                "styleId": selectedAiStyleId,
                "shape": selectedShape,
                "items": items
            })
        }

        function startAttestedAiPackPreview() {
            const jobId = activeAiPackJobId
            const manifestPath = String(AccountBackend.aiIconPackManifestPath || "")
            const manifestSha256 = String(AccountBackend.aiIconPackManifestSha256 || "")
            const manifestKey = jobId + ":" + manifestSha256
            // Account can emit several changed() notifications for one
            // ready pack. Preview is deliberately one shot per attested
            // job/manifest hash; recovery is a new staged pack, not a second
            // renderer pass over the same private material.
            if (jobId === "" || manifestPath === "" || manifestSha256 === ""
                    || activeAiPackManifestKey === manifestKey)
                return
            activeAiPackManifestKey = manifestKey
            aiPackPhase = "previewing"
            ApplicationIconBackend.previewAttestedAiPack(manifestPath)
        }

        function discardFormalAiPack() {
            if (aiPackCleanupStarted || aiPackPhase === "applying")
                return
            const jobId = activeAiPackJobId || String(AccountBackend.aiIconPackJobId || "")
            ApplicationIconBackend.discardAttestedAiPack()
            activeAiPackManifestKey = ""
            if (jobId === "") {
                activeAiApplicationIds = []
                aiPackPhase = "idle"
                return
            }
            aiPackCleanupStarted = true
            if (AccountBackend.aiIconPackState === "ready" && !AccountBackend.aiIconPackBusy) {
                aiPackPhase = "releasing"
                AccountBackend.releaseAiIconMaterialPack()
            } else {
                aiPackPhase = "cancelling"
                AccountBackend.cancelAiIconMaterialPack()
            }
        }

        function finishFormalAiPackCleanup(terminalState) {
            const wasCommitted = releaseAfterAiPackCommit && terminalState === "released"
            ApplicationIconBackend.discardAttestedAiPack()
            activeAiPackJobId = ""
            activeAiPackManifestKey = ""
            activeAiApplicationIds = []
            shownAiPackConsentRequest = ""
            aiPackCleanupStarted = false
            releaseAfterAiPackCommit = false
            aiPackPhase = wasCommitted ? "applied" : "idle"
        }

        function resetAiPackForOpen() {
            aiPackSelectionMessage = ""
            shownAiPackConsentRequest = ""
            if (!aiPackBusy && activeAiPackJobId === ""
                    && String(AccountBackend.aiIconPackJobId || "") === "") {
                ApplicationIconBackend.discardAttestedAiPack()
                if (appliedAiApplicationIds.length === 0)
                    aiPackPhase = "idle"
            }
        }

        function undoAppliedAiPack() {
            if (appliedAiApplicationIds.length === 0
                    || !ApplicationIconBackend.available
                    || ApplicationIconBackend.busy
                    || typeof ApplicationIconBackend.resetApplications !== "function")
                return
            undoingAiPack = true
            ApplicationIconBackend.resetApplications(appliedAiApplicationIds)
        }

        function aiPackStatusText() {
            switch (aiPackPhase) {
            case "describing": return qsTr("Checking the selected application identities…")
            case "preparing": return qsTr("Preparing the Account confirmation…")
            case "consent_ready": return qsTr("Waiting for your confirmation…")
            case "generating":
            case "staging": return qsTr("Generating one shared AI material…")
            case "previewing": return qsTr("Rendering the final icon pack preview…")
            case "applying": return qsTr("Applying the complete AI icon pack…")
            case "releasing": return qsTr("Finishing the private AI staging…")
            case "cancelling": return qsTr("Discarding the private AI staging…")
            case "denying": return qsTr("Denying the Account request…")
            case "preview_failed": return qsTr("The staged preview could not be rendered. Discard it and generate a new pack.")
            case "failed": return qsTr("The staged AI icon pack needs attention. Discard it before changing the request.")
            case "expired": return qsTr("The staged AI icon pack expired. Discard this state before creating a new pack.")
            default: return qsTr("AI icon pack needs attention.")
            }
        }
        closeOnAccept: false
        title: qsTr("Application Icon Studio")
        subtitle: qsTr("Changes application identity only. Wi‑Fi, microphone, volume, battery, and other live KDE system icons always keep their own semantic theme.")
        acceptText: applicationIconStudio.aiStyle ? ""
                   : (ApplicationIconBackend.busy ? qsTr("Applying…") : qsTr("Apply to applications"))
        rejectText: qsTr("Close")
        rejectEnabled: !applicationIconStudio.aiPackBusy
                       && applicationIconStudio.aiPackPhase !== "applying"
                       && applicationIconStudio.aiPackPhase !== "consent_ready"
                       && applicationIconStudio.aiPackPhase !== "denying"
        dismissible: rejectEnabled
        acceptEnabled: ApplicationIconBackend.available && !ApplicationIconBackend.busy
                       && !applicationIconStudio.aiStyle
                       && (applicationIconStudio.originalStyle || promptText.trim().length > 0)
                       && (applyScope === "all" || selectedApplicationIds.length > 0)
        onAccepted: {
            if (applyScope === "selected")
                ApplicationIconBackend.applyToApplications(
                    selectedApplicationIds, selectedStyle, selectedShape, promptText)
            else
                ApplicationIconBackend.apply(selectedStyle, selectedShape, promptText)
            close()
        }
        onRejected: {
            if (applicationIconStudio.aiStyle)
                applicationIconStudio.discardFormalAiPack()
        }
        onClosed: {
            if (applicationIconStudio.aiStyle && applicationIconStudio.aiPackPhase !== "applied")
                applicationIconStudio.discardFormalAiPack()
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

                    Flow {
                        width: parent.width
                        spacing: 8 * MeoTheme.globalScale

                        MeoButton {
                            text: qsTr("Meo Color")
                            type: "outlined"
                            enabled: !applicationIconStudio.aiInputsFrozen
                            selected: applicationIconStudio.selectedStyle === "monet"
                            onClicked: applicationIconStudio.selectedStyle = "monet"
                        }
                        MeoButton {
                            text: qsTr("Original")
                            type: "outlined"
                            enabled: !applicationIconStudio.aiInputsFrozen
                            selected: applicationIconStudio.selectedStyle === "original"
                            onClicked: applicationIconStudio.selectedStyle = "original"
                        }
                        MeoButton {
                            text: qsTr("Monochrome")
                            type: "outlined"
                            enabled: !applicationIconStudio.aiInputsFrozen
                            selected: applicationIconStudio.selectedStyle === "mono"
                            onClicked: applicationIconStudio.selectedStyle = "mono"
                        }
                        MeoButton {
                            text: qsTr("Create with AI")
                            type: "outlined"
                            enabled: !applicationIconStudio.aiInputsFrozen
                            selected: applicationIconStudio.aiStyle
                            onClicked: {
                                applicationIconStudio.selectedStyle = "ai"
                                applicationIconStudio.chooseDefaultAiStyle()
                            }
                        }
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"

                        MeoText {
                            width: parent.width
                            text: applicationIconStudio.selectedStyle === "monet"
                                  ? qsTr("Default Meo style: keeps the recognizable silhouette and internal cuts, then applies the current dynamic Material palette inside one container.")
                                  : (applicationIconStudio.selectedStyle === "mono"
                                     ? qsTr("Uses a human-reviewed mono glyph with the current Meo theme color.")
                                     : (applicationIconStudio.originalStyle
                                        ? qsTr("Removes the Meo application override and lets the current upstream icon resolve normally. Shape follows the original artwork.")
                                        : qsTr("Select an approved AI style, preview the complete pack, then apply it atomically.")))
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }

                    MeoText {
                        width: parent.width
                        text: applicationIconStudio.originalStyle
                              ? qsTr("Shape: Follow original") : qsTr("Icon shape")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }

                    Flow {
                        width: parent.width
                        spacing: 12 * MeoTheme.globalScale
                        enabled: !applicationIconStudio.originalStyle
                                 && !applicationIconStudio.aiInputsFrozen
                        opacity: applicationIconStudio.originalStyle
                                 || applicationIconStudio.aiInputsFrozen ? 0.48 : 1.0

                        Repeater {
                            model: [
                                { "id": "circle", "label": qsTr("Circle") },
                                { "id": "pixel", "label": qsTr("Pixel flower") },
                                { "id": "squircle", "label": qsTr("Squircle") },
                                { "id": "rounded", "label": qsTr("Rounded square") }
                            ]

                            delegate: MeoCard {
                                id: shapeChoice
                                required property int index
                                required property var modelData
                                objectName: "applicationIconShapeChoice_" + index
                                width: 132 * MeoTheme.globalScale
                                height: 116 * MeoTheme.globalScale
                                padding: 0
                                radius: MeoTheme.shapeLarge
                                type: "outlined"
                                interactive: !applicationIconStudio.originalStyle
                                             && !applicationIconStudio.aiInputsFrozen
                                selected: applicationIconStudio.selectedShape === modelData.id
                                onClicked: {
                                    if (!applicationIconStudio.originalStyle
                                            && !applicationIconStudio.aiInputsFrozen)
                                        applicationIconStudio.selectedShape = modelData.id
                                }

                                Rectangle {
                                    id: shapeCore
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: MeoTheme.space16
                                    width: 58 * MeoTheme.globalScale
                                    height: width
                                    radius: modelData.id === "circle" || modelData.id === "pixel"
                                            ? width / 2
                                            : (modelData.id === "squircle" ? width * 0.31 : width * 0.19)
                                    color: shapeChoice.selected ? MeoTheme.primary : MeoTheme.surface

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
                                    anchors.bottomMargin: MeoTheme.space12
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.label
                                    typeRole: "label"
                                    typeSize: "small"
                                    emphasized: shapeChoice.selected
                                    color: shapeChoice.selected
                                           ? MeoTheme.contentOnPrimaryContainer
                                           : MeoTheme.contentOnSurface
                                }
                            }
                        }
                    }

                    MeoText {
                        width: parent.width
                        text: applicationIconStudio.originalStyle
                              ? qsTr("Original does not redraw, mask, or put the application inside a Meo container.")
                              : qsTr("The app’s own mark remains visible inside every shape. Circle, Pixel flower, Squircle, and Rounded square are controlled here.")
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
                            enabled: !applicationIconStudio.aiInputsFrozen
                            selected: applicationIconStudio.applyScope === "all"
                            onClicked: applicationIconStudio.applyScope = "all"
                        }
                        MeoButton {
                            text: qsTr("Choose applications")
                            type: "outlined"
                            enabled: !applicationIconStudio.aiInputsFrozen
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
                            enabled: !applicationIconStudio.aiInputsFrozen
                            visible: !ApplicationIconBackend.applicationsLoading
                            spacing: MeoTheme.space4
                            model: ApplicationIconBackend.applications

                            delegate: MeoListItem {
                                id: appChoice
                                required property int index
                                required property var modelData
                                objectName: "applicationIconAppChoice_" + index
                                width: ListView.view.width
                                isDense: true
                                isSegmented: true
                                roundingStrategy: "all"
                                outerCornerRadius: MeoTheme.shapeLarge
                                selected: applicationIconStudio.isApplicationSelected(modelData.desktopId)
                                headline: modelData.name
                                leadingComponentSize: 36
                                leadingComponent: Component {
                                    Kirigami.Icon {
                                        width: 36 * MeoTheme.globalScale
                                        height: width
                                        source: appChoice.modelData.icon
                                    }
                                }
                                trailingComponent: Component {
                                    MeoIcon {
                                        icon: appChoice.selected ? "check_circle" : "circle"
                                        size: 22
                                        color: appChoice.selected
                                               ? MeoTheme.primary
                                               : MeoTheme.outline
                                    }
                                }
                                onClicked: applicationIconStudio.toggleApplication(modelData.desktopId)
                            }
                        }

                        MeoLoadingFeedback {
                            anchors.fill: parent
                            active: ApplicationIconBackend.applicationsLoading
                            minimumVisibleDuration: 0
                            accessibleName: qsTr("Loading applications")
                            placeholder: Component {
                                Column {
                                    anchors.fill: parent
                                    spacing: MeoTheme.space4

                                    Repeater {
                                        model: 4
                                        delegate: Row {
                                            required property int index
                                            width: parent.width
                                            height: 56 * MeoTheme.globalScale
                                            spacing: MeoTheme.space12

                                            MeoSkeleton {
                                                type: "avatar"
                                                width: 36 * MeoTheme.globalScale
                                                height: width
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            MeoSkeleton {
                                                type: "text"
                                                width: Math.max(80 * MeoTheme.globalScale,
                                                                parent.width * (0.52 + (index % 2) * 0.16))
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    MeoCard {
                        width: parent.width
                        visible: applicationIconStudio.aiStyle
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
                                text: qsTr("Choose an approved material style. Meo Account generates one shared texture only; Meo Icon Studio preserves each reviewed application identity and previews the complete pack before anything changes.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }

                            MeoButton {
                                width: parent.width
                                visible: !AccountBackend.signedIn
                                text: qsTr("Connect Meo Account")
                                type: "tonal"
                                enabled: AccountBackend.available
                                onClicked: AccountBackend.openHostedAction("ai_providers")
                            }

                            MeoText {
                                width: parent.width
                                visible: AccountBackend.signedIn
                                         && (AccountBackend.aiIconPackState === "loading_styles"
                                             || AccountBackend.aiIconPackState === "contacting")
                                text: qsTr("Loading approved AI styles…")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                            }

                            MeoText {
                                width: parent.width
                                visible: AccountBackend.signedIn
                                         && (AccountBackend.aiIconStyles || []).length === 0
                                         && !AccountBackend.aiIconPackBusy
                                text: qsTr("No approved AI icon style is available for this Account connection yet.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }

                            Flow {
                                width: parent.width
                                spacing: 8 * MeoTheme.globalScale
                                visible: AccountBackend.signedIn
                                         && (AccountBackend.aiIconStyles || []).length > 0

                                Repeater {
                                    model: AccountBackend.aiIconStyles

                                    delegate: MeoCard {
                                        required property var modelData
                                        width: Math.min(parent.width,
                                                        214 * MeoTheme.globalScale)
                                        type: "outlined"
                                        interactive: modelData.available !== false
                                                     && !applicationIconStudio.aiInputsFrozen
                                        selected: applicationIconStudio.selectedAiStyleId
                                                  === String(modelData.id || "")
                                        opacity: modelData.available !== false ? 1.0 : 0.56
                                        onClicked: {
                                            if (modelData.available !== false
                                                    && !applicationIconStudio.aiInputsFrozen) {
                                                applicationIconStudio.selectedAiStyleId = String(modelData.id || "")
                                                applicationIconStudio.aiPackSelectionMessage = ""
                                            }
                                        }

                                        Column {
                                            width: parent.width
                                            spacing: MeoTheme.space4

                                            MeoText {
                                                width: parent.width
                                                text: String(modelData.displayName || modelData.id || "")
                                                typeRole: "title"
                                                typeSize: "small"
                                                emphasized: true
                                                color: MeoTheme.contentOnSurface
                                                wrapMode: Text.WordWrap
                                            }
                                            MeoText {
                                                width: parent.width
                                                text: modelData.available !== false
                                                      ? String(modelData.description || "")
                                                      : String(modelData.unavailableReason
                                                               || qsTr("Unavailable for this Account connection."))
                                                typeRole: "body"
                                                typeSize: "small"
                                                color: MeoTheme.contentOnSurfaceVariant
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }

                            MeoText {
                                width: parent.width
                                visible: applicationIconStudio.aiStyle
                                         && applicationIconStudio.selectedAiApplicationIds().length > 128
                                text: qsTr("This selection has %1 applications. Choose up to 128 for one AI icon pack.")
                                      .arg(applicationIconStudio.selectedAiApplicationIds().length)
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.error
                                wrapMode: Text.WordWrap
                            }

                            MeoText {
                                width: parent.width
                                visible: applicationIconStudio.aiPackSelectionMessage !== ""
                                text: applicationIconStudio.aiPackSelectionMessage
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.error
                                wrapMode: Text.WordWrap
                            }

                            MeoText {
                                width: parent.width
                                visible: applicationIconStudio.aiPackPhase !== "idle"
                                         && applicationIconStudio.aiPackPhase !== "styles_ready"
                                         && applicationIconStudio.aiPackPhase !== "preview_ready"
                                         && applicationIconStudio.aiPackPhase !== "applied"
                                text: applicationIconStudio.aiPackStatusText()
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }

                            Flow {
                                width: parent.width
                                spacing: 10 * MeoTheme.globalScale
                                visible: ApplicationIconBackend.aiPackPreviews.length > 0

                                Repeater {
                                    model: ApplicationIconBackend.aiPackPreviews

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
                                text: applicationIconStudio.aiPackPhase === "applied"
                                      ? qsTr("Generate another AI icon pack")
                                      : (applicationIconStudio.applyScope === "all"
                                         ? qsTr("Generate all with AI")
                                         : qsTr("Generate selected with AI"))
                                type: "filled"
                                visible: applicationIconStudio.aiPackPhase === "idle"
                                         || applicationIconStudio.aiPackPhase === "styles_ready"
                                         || applicationIconStudio.aiPackPhase === "applied"
                                enabled: AccountBackend.signedIn
                                         && !applicationIconStudio.aiPackBusy
                                         && String(AccountBackend.aiIconPackJobId || "") === ""
                                         && applicationIconStudio.selectedAiStyleId !== ""
                                         && applicationIconStudio.selectedAiApplicationIds().length >= 1
                                         && applicationIconStudio.selectedAiApplicationIds().length <= 128
                                onClicked: applicationIconStudio.startFormalAiPack()
                            }
                            MeoButton {
                                text: ApplicationIconBackend.busy
                                      ? qsTr("Applying complete pack…")
                                      : qsTr("Apply complete AI icon pack")
                                type: "tonal"
                                visible: applicationIconStudio.aiPackPhase === "preview_ready"
                                         && ApplicationIconBackend.aiPackPreviewReady
                                enabled: !ApplicationIconBackend.busy
                                         && !applicationIconStudio.aiPackBusy
                                onClicked: {
                                    applicationIconStudio.aiPackPhase = "applying"
                                    ApplicationIconBackend.applyAttestedAiPack()
                                }
                            }
                            MeoButton {
                                text: qsTr("Discard staged AI pack")
                                type: "text"
                                visible: applicationIconStudio.aiPackPhase !== "idle"
                                         && applicationIconStudio.aiPackPhase !== "styles_ready"
                                         && applicationIconStudio.aiPackPhase !== "applied"
                                         && applicationIconStudio.aiPackPhase !== "applying"
                                enabled: applicationIconStudio.aiPackPhase !== "releasing"
                                onClicked: applicationIconStudio.discardFormalAiPack()
                            }
                            MeoButton {
                                text: qsTr("Undo this AI icon pack")
                                type: "text"
                                visible: applicationIconStudio.aiPackPhase === "applied"
                                         && applicationIconStudio.appliedAiApplicationIds.length > 0
                                         && typeof ApplicationIconBackend.resetApplications === "function"
                                enabled: ApplicationIconBackend.available
                                         && !ApplicationIconBackend.busy
                                         && !applicationIconStudio.undoingAiPack
                                onClicked: applicationIconStudio.undoAppliedAiPack()
                            }
                            MeoButton {
                                text: qsTr("Refresh AI styles")
                                type: "text"
                                visible: AccountBackend.signedIn
                                         && !applicationIconStudio.aiInputsFrozen
                                enabled: !applicationIconStudio.aiPackBusy
                                onClicked: AccountBackend.refreshAiIconStyles()
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
                                 && !applicationIconStudio.aiInputsFrozen
                        onClicked: {
                            ApplicationIconBackend.reset()
                            applicationIconStudio.close()
                        }
                    }

                    Column {
                        width: parent.width
                        visible: applicationIconStudio.iconStudioError !== ""
                        spacing: MeoTheme.space4
                        MeoBanner {
                            width: parent.width
                            title: applicationIconStudio.aiStyle
                                   ? qsTr("AI icon pack needs attention")
                                   : qsTr("Application icons need attention")
                            text: qsTr("Check the selected icon, then try again.")
                            icon: "error"
                            tone: "error"
                        }
                        MeoText {
                            width: parent.width
                            text: qsTr("Technical details: %1").arg(applicationIconStudio.iconStudioError)
                            Accessible.name: text
                            typeRole: "label"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: AccountBackend

        function onChanged() {
            const state = String(AccountBackend.aiIconPackState || "")
            const returnedJobId = String(AccountBackend.aiIconPackJobId || "")

            if (state === "styles_ready") {
                applicationIconStudio.chooseDefaultAiStyle()
                if (applicationIconStudio.aiPackPhase === "idle")
                    applicationIconStudio.aiPackPhase = "styles_ready"
                return
            }

            // A local validation failure can happen before Account allocates
            // a job ID. It still belongs to the request currently being
            // prepared and must not leave the sheet stuck in "preparing".
            if (applicationIconStudio.aiPackPhase === "preparing"
                    && applicationIconStudio.activeAiPackJobId === ""
                    && returnedJobId === "" && state === "failed") {
                applicationIconStudio.aiPackPhase = "failed"
                return
            }

            if (applicationIconStudio.aiPackPhase === "preparing"
                    && applicationIconStudio.activeAiPackJobId === ""
                    && returnedJobId !== "") {
                applicationIconStudio.activeAiPackJobId = returnedJobId
            }

            const matchesActiveJob = applicationIconStudio.activeAiPackJobId !== ""
                                     && (returnedJobId === ""
                                         || returnedJobId === applicationIconStudio.activeAiPackJobId)
            if (!matchesActiveJob)
                return

            if (state === "consent_ready") {
                const consent = AccountBackend.aiIconPackConsent || {}
                const requestId = String(consent.requestId || "")
                applicationIconStudio.aiPackPhase = "consent_ready"
                if (requestId !== ""
                        && requestId !== applicationIconStudio.shownAiPackConsentRequest) {
                    applicationIconStudio.shownAiPackConsentRequest = requestId
                    aiIconConsent.open()
                }
                return
            }

            if (state === "generating" || state === "staging") {
                applicationIconStudio.aiPackPhase = state
                return
            }

            if (state === "ready") {
                if (applicationIconStudio.aiPackPhase !== "preview_ready"
                        && applicationIconStudio.aiPackPhase !== "applying"
                        && applicationIconStudio.aiPackPhase !== "releasing") {
                    applicationIconStudio.startAttestedAiPackPreview()
                }
                return
            }

            if (state === "released" || state === "cancelled" || state === "denied") {
                applicationIconStudio.finishFormalAiPackCleanup(state)
                return
            }

            if (state === "expired") {
                ApplicationIconBackend.discardAttestedAiPack()
                applicationIconStudio.activeAiPackJobId = ""
                applicationIconStudio.activeAiPackManifestKey = ""
                applicationIconStudio.activeAiApplicationIds = []
                applicationIconStudio.aiPackCleanupStarted = false
                applicationIconStudio.aiPackPhase = "expired"
                return
            }

            if (state === "failed") {
                if (applicationIconStudio.aiPackPhase !== "applying")
                    applicationIconStudio.aiPackPhase = "failed"
            }
        }
    }

    Connections {
        target: ApplicationIconBackend

        function onChanged() {
            if (applicationIconStudio.aiPackPhase === "describing"
                    && !ApplicationIconBackend.aiPackDescribing) {
                if ((ApplicationIconBackend.aiPackDescriptors || []).length > 0)
                    applicationIconStudio.prepareFormalAiPackFromDescriptors()
                else if (ApplicationIconBackend.error !== "")
                    applicationIconStudio.aiPackPhase = "failed"
            }

            if (applicationIconStudio.aiPackPhase === "previewing") {
                if (ApplicationIconBackend.aiPackPreviewReady) {
                    applicationIconStudio.aiPackPhase = "preview_ready"
                } else if (!ApplicationIconBackend.aiPackPreviewing
                           && ApplicationIconBackend.error !== "") {
                    applicationIconStudio.aiPackPhase = "preview_failed"
                }
            }

            if (applicationIconStudio.aiPackPhase === "applying") {
                if (ApplicationIconBackend.aiPackCommitted) {
                    applicationIconStudio.appliedAiApplicationIds
                        = applicationIconStudio.activeAiApplicationIds.slice()
                    applicationIconStudio.releaseAfterAiPackCommit = true
                    applicationIconStudio.aiPackPhase = "releasing"
                    AccountBackend.releaseAiIconMaterialPack()
                } else if (!ApplicationIconBackend.busy
                           && ApplicationIconBackend.error !== "") {
                    applicationIconStudio.aiPackPhase = "preview_ready"
                }
            }

            if (applicationIconStudio.undoingAiPack && !ApplicationIconBackend.busy) {
                if (ApplicationIconBackend.error === "") {
                    applicationIconStudio.appliedAiApplicationIds = []
                    applicationIconStudio.aiPackPhase = "idle"
                }
                applicationIconStudio.undoingAiPack = false
            }
        }
    }

    MeoSettingsTaskSheet {
        id: aiIconConsent
        popupParent: Overlay.overlay
        title: qsTr("Generate this application icon pack?")
        subtitle: qsTr("Confirm this exact Account-owned request once. Meo Settings never receives a provider key, provider image, or Account token.")
        showCloseButton: false
        dismissible: false
        closeOnAccept: true
        closeOnReject: true
        acceptText: qsTr("Confirm and generate pack")
        rejectText: qsTr("Deny")
        acceptEnabled: !AccountBackend.aiIconPackBusy
                       && String(AccountBackend.aiIconPackConsent.requestId || "") !== ""
        onAccepted: {
            applicationIconStudio.aiPackPhase = "generating"
            AccountBackend.generatePreparedAiIconMaterialPack()
        }
        onRejected: {
            applicationIconStudio.aiPackPhase = "denying"
            AccountBackend.denyPreparedAiIconMaterialPack()
        }

        content: Component {
            Column {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale

                readonly property var consent: AccountBackend.aiIconPackConsent || ({})

                MeoText {
                    width: parent.width
                    text: qsTr("Provider: %1\nModel: %2\nDestination: %3\nPurpose: %4\nStyle: %5\nShape: %6\nApplications: %7\nData: %8")
                          .arg(parent.consent.providerName || parent.consent.provider || qsTr("Unknown"))
                          .arg(parent.consent.model || qsTr("Unknown"))
                          .arg(parent.consent.destination || qsTr("Unknown"))
                          .arg(parent.consent.purpose || qsTr("Unknown"))
                          .arg(parent.consent.styleId || qsTr("Unknown"))
                          .arg(parent.consent.shape || qsTr("Unknown"))
                          .arg(parent.consent.itemCount || 1)
                          .arg((parent.consent.dataCategories || []).join(", "))
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurface
                    wrapMode: Text.WordWrap
                }
                MeoText {
                    width: parent.width
                    text: qsTr("One approved material is generated for this pack. Meo Icon Studio keeps the canonical application identities local, renders a full preview, and does not install anything until Apply is pressed.")
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
