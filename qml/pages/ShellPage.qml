import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    property var shelfDraft: ({})
    property var notificationsDraft: ({})
    property var timeCenterDraft: ({})
    property var topTasksDraft: ({})
    property bool shelfDirty: false
    property bool notificationsDirty: false
    property bool timeCenterDirty: false
    property bool topTasksDirty: false

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
    readonly property var launcherPageOptions: [
        { "label": qsTr("Home"), "value": "home" },
        { "label": qsTr("All apps"), "value": "apps" }
    ]
    readonly property var launcherWidthOptions: [
        { "label": qsTr("Compact"), "value": "compact" },
        { "label": qsTr("Standard"), "value": "standard" },
        { "label": qsTr("Wide"), "value": "wide" }
    ]
    readonly property var notificationViewOptions: [
        { "label": qsTr("Cards"), "value": "cards" },
        { "label": qsTr("Compact list"), "value": "compact" }
    ]
    readonly property var previewOptions: [
        { "label": qsTr("Full"), "value": "full" },
        { "label": qsTr("Summary"), "value": "summary" },
        { "label": qsTr("Hidden"), "value": "hidden" }
    ]
    readonly property var clockOptions: [
        { "label": qsTr("System default"), "value": "system" },
        { "label": qsTr("24-hour"), "value": "24h" },
        { "label": qsTr("12-hour"), "value": "12h" }
    ]
    readonly property var popupOptions: [
        { "label": qsTr("Standard"), "value": "standard" },
        { "label": qsTr("Wide"), "value": "wide" }
    ]
    readonly property var defaultPageOptions: [
        { "label": qsTr("Notifications"), "value": "notifications" },
        { "label": qsTr("Calendar"), "value": "calendar" }
    ]

    function cloneMap(source) {
        const next = ({})
        if (!source)
            return next
        for (const key in source) {
            if (key !== "available" && key !== "count")
                next[key] = source[key]
        }
        return next
    }

    function optionIndex(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (String(options[index].value) === String(value))
                return index
        }
        return 0
    }

    function setMapValue(source, key, value) {
        const next = cloneMap(source)
        next[key] = value
        return next
    }

    function loadShelf() {
        shelfDraft = cloneMap(ShellSettingsBackend.shelf)
        shelfDirty = false
    }
    function loadNotifications() {
        notificationsDraft = cloneMap(ShellSettingsBackend.notifications)
        notificationsDirty = false
    }
    function loadTimeCenter() {
        timeCenterDraft = cloneMap(ShellSettingsBackend.timeCenter)
        timeCenterDirty = false
    }
    function loadTopTasks() {
        topTasksDraft = cloneMap(ShellSettingsBackend.topTasks)
        topTasksDirty = false
    }

    function setShelf(key, value) {
        shelfDraft = setMapValue(shelfDraft, key, value)
        shelfDirty = true
    }
    function setNotifications(key, value) {
        notificationsDraft = setMapValue(notificationsDraft, key, value)
        notificationsDirty = true
    }
    function setTimeCenter(key, value) {
        timeCenterDraft = setMapValue(timeCenterDraft, key, value)
        timeCenterDirty = true
    }
    function setTopTasks(key, value) {
        topTasksDraft = setMapValue(topTasksDraft, key, value)
        topTasksDirty = true
    }

    readonly property var componentRows: [
        {
            "title": qsTr("Shelf & Launcher"),
            "subtitle": ShellSettingsBackend.shelf.available
                        ? qsTr("Connected to the active Meo Shelf")
                        : qsTr("Not present in the active Plasma layout"),
            "icon": "dock_to_bottom",
            "tone": "primary",
            "trailingKind": "status",
            "trailingText": ShellSettingsBackend.shelf.available ? qsTr("Ready") : qsTr("Unavailable"),
            "statusTone": ShellSettingsBackend.shelf.available ? "primary" : "neutral",
            "interactive": false
        },
        {
            "title": qsTr("Notification Center"),
            "subtitle": ShellSettingsBackend.notifications.available
                        ? qsTr("Meo notification surface is configurable")
                        : qsTr("Separate notification applet is not present"),
            "icon": "notifications",
            "tone": "secondary",
            "trailingKind": "status",
            "trailingText": ShellSettingsBackend.notifications.available ? qsTr("Ready") : qsTr("Unavailable"),
            "statusTone": ShellSettingsBackend.notifications.available ? "primary" : "neutral",
            "interactive": false
        },
        {
            "title": qsTr("Time Center"),
            "subtitle": ShellSettingsBackend.timeCenter.available
                        ? qsTr("Clock, calendar, and combined notification surface")
                        : qsTr("Time Center is not present in the active layout"),
            "icon": "calendar_month",
            "tone": "tertiary",
            "trailingKind": "status",
            "trailingText": ShellSettingsBackend.timeCenter.available ? qsTr("Ready") : qsTr("Unavailable"),
            "statusTone": ShellSettingsBackend.timeCenter.available ? "primary" : "neutral",
            "interactive": false
        },
        {
            "title": qsTr("Top tasks"),
            "subtitle": ShellSettingsBackend.topTasks.available
                        ? qsTr("Top-panel application strip is configurable")
                        : qsTr("Top tasks is not present in the active layout"),
            "icon": "view_week",
            "tone": "secondary",
            "trailingKind": "status",
            "trailingText": ShellSettingsBackend.topTasks.available ? qsTr("Ready") : qsTr("Unavailable"),
            "statusTone": ShellSettingsBackend.topTasks.available ? "primary" : "neutral",
            "interactive": false
        },
        {
            "title": qsTr("On-screen display"),
            "subtitle": qsTr("Volume and brightness OSD still use the Plasma-owned surface; Meo Settings does not expose fake controls for it."),
            "icon": "picture_in_picture_alt",
            "tone": "neutral",
            "trailingKind": "status",
            "trailingText": qsTr("Plasma"),
            "interactive": false
        }
    ]

    readonly property var shelfRows: [
        {
            "id": "showLauncherButton",
            "title": qsTr("Launcher button"),
            "subtitle": qsTr("Show the Meo application launcher at the start of the Shelf"),
            "icon": "apps",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": Boolean(shelfDraft.showLauncherButton)
        },
        {
            "id": "filterTasksByVirtualDesktop",
            "title": qsTr("Current desktop only"),
            "subtitle": qsTr("Only show applications from the current virtual desktop"),
            "icon": "view_carousel",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(shelfDraft.filterTasksByVirtualDesktop)
        },
        {
            "id": "showRunningIndicators",
            "title": qsTr("Running indicators"),
            "subtitle": qsTr("Show active and running state below application icons"),
            "icon": "more_horiz",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": shelfDraft.showRunningIndicators === undefined ? true : Boolean(shelfDraft.showRunningIndicators)
        },
        {
            "id": "showTooltips",
            "title": qsTr("App tooltips"),
            "subtitle": qsTr("Show app and window information when hovering Shelf items"),
            "icon": "info",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": shelfDraft.showTooltips === undefined ? true : Boolean(shelfDraft.showTooltips)
        },
        {
            "id": "launcherDefaultPage",
            "title": qsTr("Launcher default page"),
            "subtitle": qsTr("Choose what appears before you start searching"),
            "icon": "home",
            "tone": "primary",
            "trailingKind": "dropdown",
            "options": root.launcherPageOptions,
            "currentIndex": root.optionIndex(root.launcherPageOptions, shelfDraft.launcherDefaultPage || "home")
        },
        {
            "id": "launcherWidth",
            "title": qsTr("Launcher width"),
            "subtitle": qsTr("Choose a compact, standard, or wide launcher surface"),
            "icon": "width",
            "tone": "tertiary",
            "trailingKind": "segmented",
            "options": root.launcherWidthOptions,
            "currentIndex": root.optionIndex(root.launcherWidthOptions, shelfDraft.launcherWidth || "standard")
        },
        {
            "id": "launcherShowFavorites",
            "title": qsTr("Pinned apps"),
            "subtitle": qsTr("Show pinned applications on the launcher Home page"),
            "icon": "keep",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": shelfDraft.launcherShowFavorites === undefined ? true : Boolean(shelfDraft.launcherShowFavorites)
        },
        {
            "id": "launcherShowRecents",
            "title": qsTr("Recent items"),
            "subtitle": qsTr("Show recent applications and documents on the launcher Home page"),
            "icon": "history",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": shelfDraft.launcherShowRecents === undefined ? true : Boolean(shelfDraft.launcherShowRecents)
        }
    ]

    readonly property var notificationContentRows: [
        {
            "id": "notificationView",
            "title": qsTr("Notification view"),
            "subtitle": qsTr("Choose card or compact-list presentation"),
            "icon": "view_agenda",
            "tone": "primary",
            "trailingKind": "segmented",
            "options": root.notificationViewOptions,
            "currentIndex": root.optionIndex(root.notificationViewOptions, notificationsDraft.notificationView || "cards")
        },
        {
            "id": "notificationPreview",
            "title": qsTr("Preview content"),
            "subtitle": qsTr("Control how much message content the shell notification surface shows"),
            "icon": "visibility",
            "tone": "secondary",
            "trailingKind": "dropdown",
            "options": root.previewOptions,
            "currentIndex": root.optionIndex(root.previewOptions, notificationsDraft.notificationPreview || "full")
        },
        {
            "id": "showNotificationHistory",
            "title": qsTr("Notification history"),
            "subtitle": qsTr("Keep the history section visible in the Meo notification surface"),
            "icon": "history",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": notificationsDraft.showNotificationHistory === undefined ? true : Boolean(notificationsDraft.showNotificationHistory)
        },
        {
            "id": "showUnreadBadge",
            "title": qsTr("Unread badge"),
            "subtitle": qsTr("Show unread notification count"),
            "icon": "mark_email_unread",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": notificationsDraft.showUnreadBadge === undefined ? true : Boolean(notificationsDraft.showUnreadBadge)
        },
        {
            "id": "showJobs",
            "title": qsTr("Background tasks"),
            "subtitle": qsTr("Show active jobs and progress"),
            "icon": "sync",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": notificationsDraft.showJobs === undefined ? true : Boolean(notificationsDraft.showJobs)
        }
    ]

    readonly property var notificationAppearanceRows: [
        {
            "id": "density",
            "title": qsTr("Density"),
            "subtitle": qsTr("Choose compact or comfortable spacing"),
            "icon": "density_medium",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.densityOptions,
            "currentIndex": root.optionIndex(root.densityOptions, notificationsDraft.density || "comfortable")
        },
        {
            "id": "textScalePercent",
            "title": qsTr("Text size"),
            "subtitle": qsTr("Scale text inside the Meo notification surface"),
            "icon": "text_fields",
            "tone": "primary",
            "trailingKind": "slider",
            "from": 85,
            "to": 125,
            "value": Number(notificationsDraft.textScalePercent === undefined ? 100 : notificationsDraft.textScalePercent),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "surfaceStyle",
            "title": qsTr("Surface style"),
            "subtitle": qsTr("Follow the theme or use a different shell surface treatment"),
            "icon": "layers",
            "tone": "tertiary",
            "trailingKind": "dropdown",
            "options": root.surfaceOptions,
            "currentIndex": root.optionIndex(root.surfaceOptions, notificationsDraft.surfaceStyle || "theme")
        },
        {
            "id": "surfaceOpacityPercent",
            "title": qsTr("Surface opacity"),
            "subtitle": qsTr("Adjust translucent shell surfaces"),
            "icon": "opacity",
            "tone": "tertiary",
            "trailingKind": "slider",
            "from": 70,
            "to": 100,
            "value": Number(notificationsDraft.surfaceOpacityPercent === undefined ? 100 : notificationsDraft.surfaceOpacityPercent),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "motionProfile",
            "title": qsTr("Motion"),
            "subtitle": qsTr("Choose the shell animation profile"),
            "icon": "animation",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.motionOptions,
            "currentIndex": root.optionIndex(root.motionOptions, notificationsDraft.motionProfile || "pixel")
        }
    ]

    readonly property var timeClockRows: [
        {
            "id": "clockFormat",
            "title": qsTr("Clock format"),
            "subtitle": qsTr("Follow the system or force a 12-hour or 24-hour clock"),
            "icon": "schedule",
            "tone": "primary",
            "trailingKind": "dropdown",
            "options": root.clockOptions,
            "currentIndex": root.optionIndex(root.clockOptions, timeCenterDraft.clockFormat || "system")
        },
        {
            "id": "showSeconds",
            "title": qsTr("Show seconds"),
            "subtitle": qsTr("Display seconds in the Time Center clock"),
            "icon": "timer",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(timeCenterDraft.showSeconds)
        },
        {
            "id": "showDate",
            "title": qsTr("Show date"),
            "subtitle": qsTr("Show the date alongside the time"),
            "icon": "calendar_today",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": timeCenterDraft.showDate === undefined ? true : Boolean(timeCenterDraft.showDate)
        },
        {
            "id": "popupLayout",
            "title": qsTr("Popup layout"),
            "subtitle": qsTr("Choose standard or wide Time Center layout"),
            "icon": "view_sidebar",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.popupOptions,
            "currentIndex": root.optionIndex(root.popupOptions, timeCenterDraft.popupLayout || "standard")
        },
        {
            "id": "defaultPage",
            "title": qsTr("Default content"),
            "subtitle": qsTr("Open the Time Center on notifications or calendar"),
            "icon": "tab",
            "tone": "primary",
            "trailingKind": "dropdown",
            "options": root.defaultPageOptions,
            "currentIndex": root.optionIndex(root.defaultPageOptions, timeCenterDraft.defaultPage || "notifications")
        },
        {
            "id": "showWeekNumbers",
            "title": qsTr("Week numbers"),
            "subtitle": qsTr("Show week numbers in the calendar"),
            "icon": "tag",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": Boolean(timeCenterDraft.showWeekNumbers)
        },
        {
            "id": "showSecondaryCalendar",
            "title": qsTr("Secondary calendar"),
            "subtitle": qsTr("Show the secondary calendar when the platform provides one"),
            "icon": "event_note",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": timeCenterDraft.showSecondaryCalendar === undefined ? true : Boolean(timeCenterDraft.showSecondaryCalendar)
        },
        {
            "id": "showNotifications",
            "title": qsTr("Notifications in Time Center"),
            "subtitle": qsTr("Keep the combined notification view available"),
            "icon": "notifications",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": timeCenterDraft.showNotifications === undefined ? true : Boolean(timeCenterDraft.showNotifications)
        }
    ]

    readonly property var timeAppearanceRows: [
        {
            "id": "density",
            "title": qsTr("Density"),
            "subtitle": qsTr("Choose compact or comfortable Time Center spacing"),
            "icon": "density_medium",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.densityOptions,
            "currentIndex": root.optionIndex(root.densityOptions, timeCenterDraft.density || "comfortable")
        },
        {
            "id": "textScalePercent",
            "title": qsTr("Text size"),
            "subtitle": qsTr("Scale text inside Time Center"),
            "icon": "text_fields",
            "tone": "primary",
            "trailingKind": "slider",
            "from": 75,
            "to": 150,
            "value": Number(timeCenterDraft.textScalePercent === undefined ? 100 : timeCenterDraft.textScalePercent),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "surfaceStyle",
            "title": qsTr("Surface style"),
            "subtitle": qsTr("Choose the Time Center shell surface treatment"),
            "icon": "layers",
            "tone": "tertiary",
            "trailingKind": "dropdown",
            "options": root.surfaceOptions,
            "currentIndex": root.optionIndex(root.surfaceOptions, timeCenterDraft.surfaceStyle || "theme")
        },
        {
            "id": "surfaceOpacityPercent",
            "title": qsTr("Surface opacity"),
            "subtitle": qsTr("Adjust opacity for translucent Time Center surfaces"),
            "icon": "opacity",
            "tone": "tertiary",
            "trailingKind": "slider",
            "from": 70,
            "to": 100,
            "value": Number(timeCenterDraft.surfaceOpacityPercent === undefined ? 100 : timeCenterDraft.surfaceOpacityPercent),
            "stepSize": 5,
            "discrete": true,
            "valueSuffix": "%"
        },
        {
            "id": "motionProfile",
            "title": qsTr("Motion"),
            "subtitle": qsTr("Choose the Time Center animation profile"),
            "icon": "animation",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": root.motionOptions,
            "currentIndex": root.optionIndex(root.motionOptions, timeCenterDraft.motionProfile || "pixel")
        }
    ]

    function optionValue(row, optionIndex) {
        if (!row.options || optionIndex < 0 || optionIndex >= row.options.length)
            return undefined
        const option = row.options[optionIndex]
        return option && option.value !== undefined ? option.value : undefined
    }

    Component.onCompleted: {
        loadShelf()
        loadNotifications()
        loadTimeCenter()
        loadTopTasks()
    }

    Connections {
        target: ShellSettingsBackend
        function onChanged() {
            if (!root.shelfDirty) root.loadShelf()
            if (!root.notificationsDirty) root.loadNotifications()
            if (!root.timeCenterDirty) root.loadTimeCenter()
            if (!root.topTasksDirty) root.loadTopTasks()
        }
        function onShelfSaved() { root.loadShelf() }
        function onNotificationsSaved() { root.loadNotifications() }
        function onTimeCenterSaved() { root.loadTimeCenter() }
        function onTopTasksSaved() { root.loadTopTasks() }
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Desktop & shell")
        subtitle: qsTr("Configure Meo-owned desktop surfaces through the active Plasma session.")

        Column {
            width: parent.width
            visible: ShellSettingsBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Shell settings need attention")
                text: qsTr("The current Plasma layout was left unchanged.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(ShellSettingsBackend.error)
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Detected surfaces")
            subtitle: qsTr("Only real Meo components in the active Plasma layout can be edited.")
            model: root.componentRows
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Quick Settings & top bar")
            subtitle: qsTr("Top-bar status, appearance, and Quick Settings tile layout.")
            model: [{
                "title": qsTr("Open Control Center settings"),
                "subtitle": qsTr("Edit top-bar status and Quick Settings tiles"),
                "icon": "tune",
                "tone": "primary",
                "route": "control-center",
                "trailingKind": "navigation"
            }]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.shelf.available)
            title: qsTr("Shelf & Launcher")
            subtitle: ""
            model: root.shelfRows
            onRowToggled: (index, checked, row) => root.setShelf(row.id, checked)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setShelf(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const next = root.optionValue(row, optionIndex)
                if (next !== undefined)
                    root.setShelf(row.id, next)
            }
        }

        Flow {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.shelf.available)
            spacing: MeoTheme.space8
            MeoButton {
                text: ShellSettingsBackend.busy ? qsTr("Applying…") : qsTr("Apply Shelf")
                type: "filled"
                enabled: root.shelfDirty && !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.saveShelf(root.shelfDraft)
            }
            MeoButton {
                text: qsTr("Restore Shelf defaults")
                type: "tonal"
                enabled: !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.resetShelf()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.notifications.available)
            title: qsTr("Notification Center content")
            subtitle: ""
            model: root.notificationContentRows
            onRowToggled: (index, checked, row) => root.setNotifications(row.id, checked)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setNotifications(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const next = root.optionValue(row, optionIndex)
                if (next !== undefined)
                    root.setNotifications(row.id, next)
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.notifications.available)
            title: qsTr("Notification Center appearance")
            subtitle: ""
            model: root.notificationAppearanceRows
            onRowSliderMoved: (index, value, row) => root.setNotifications(row.id, value)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setNotifications(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const next = root.optionValue(row, optionIndex)
                if (next !== undefined)
                    root.setNotifications(row.id, next)
            }
        }

        Flow {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.notifications.available)
            spacing: MeoTheme.space8
            MeoButton {
                text: ShellSettingsBackend.busy ? qsTr("Applying…") : qsTr("Apply Notification Center")
                type: "filled"
                enabled: root.notificationsDirty && !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.saveNotifications(root.notificationsDraft)
            }
            MeoButton {
                text: qsTr("Restore notification defaults")
                type: "tonal"
                enabled: !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.resetNotifications()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.timeCenter.available)
            title: qsTr("Time & calendar")
            subtitle: ""
            model: root.timeClockRows
            onRowToggled: (index, checked, row) => root.setTimeCenter(row.id, checked)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setTimeCenter(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const next = root.optionValue(row, optionIndex)
                if (next !== undefined)
                    root.setTimeCenter(row.id, next)
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.timeCenter.available)
            title: qsTr("Time Center appearance")
            subtitle: ""
            model: root.timeAppearanceRows
            onRowSliderMoved: (index, value, row) => root.setTimeCenter(row.id, value)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setTimeCenter(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const next = root.optionValue(row, optionIndex)
                if (next !== undefined)
                    root.setTimeCenter(row.id, next)
            }
        }

        Flow {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.timeCenter.available)
            spacing: MeoTheme.space8
            MeoButton {
                text: ShellSettingsBackend.busy ? qsTr("Applying…") : qsTr("Apply Time Center")
                type: "filled"
                enabled: root.timeCenterDirty && !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.saveTimeCenter(root.timeCenterDraft)
            }
            MeoButton {
                text: qsTr("Restore Time Center defaults")
                type: "tonal"
                enabled: !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.resetTimeCenter()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.topTasks.available)
            title: qsTr("Top tasks")
            subtitle: qsTr("Limit the number of application icons in the top-panel task strip.")
            model: [{
                "id": "taskLimit",
                "title": qsTr("Open application icons"),
                "subtitle": qsTr("Choose how many task icons the top panel may show"),
                "icon": "view_week",
                "tone": "secondary",
                "trailingKind": "slider",
                "from": 1,
                "to": 12,
                "value": Number(root.topTasksDraft.taskLimit === undefined ? 8 : root.topTasksDraft.taskLimit),
                "stepSize": 1,
                "discrete": true
            }]
            onRowSliderMoved: (index, value, row) => root.setTopTasks(row.id, value)
        }

        Flow {
            width: parent.width
            visible: Boolean(ShellSettingsBackend.topTasks.available)
            spacing: MeoTheme.space8
            MeoButton {
                text: ShellSettingsBackend.busy ? qsTr("Applying…") : qsTr("Apply top tasks")
                type: "filled"
                enabled: root.topTasksDirty && !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.saveTopTasks(root.topTasksDraft)
            }
            MeoButton {
                text: qsTr("Restore task defaults")
                type: "tonal"
                enabled: !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.resetTopTasks()
            }
        }

        Flow {
            width: parent.width
            spacing: MeoTheme.space8
            MeoButton {
                text: ShellSettingsBackend.busy ? qsTr("Refreshing…") : qsTr("Refresh shell surfaces")
                icon.name: "refresh"
                type: "text"
                enabled: !ShellSettingsBackend.busy
                onClicked: ShellSettingsBackend.refresh()
            }
            MeoButton {
                text: qsTr("Global notification behavior")
                type: "text"
                onClicked: root.navigateTo("notifications")
            }
        }
    }
}
