import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    property var draft: ({})
    property bool hasChanges: false

    readonly property var pageOptions: [
        { "label": qsTr("Home"), "value": "home" },
        { "label": qsTr("All apps"), "value": "apps" }
    ]
    readonly property var widthOptions: [
        { "label": qsTr("Compact"), "value": "compact" },
        { "label": qsTr("Standard"), "value": "standard" },
        { "label": qsTr("Wide"), "value": "wide" }
    ]

    function loadDraft() {
        draft = Object.assign({}, ShelfBackend.settings || {})
        hasChanges = false
    }

    function optionIndex(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index
        }
        return 0
    }

    function setValue(key, value) {
        const next = Object.assign({}, draft)
        next[key] = value
        draft = next
        hasChanges = true
    }

    readonly property var shelfRows: [
        {
            "id": "showLauncherButton",
            "title": qsTr("Launcher button"),
            "subtitle": qsTr("Show the Meo launcher entry on the Shelf"),
            "icon": "apps",
            "tone": "primary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.showLauncherButton)
        },
        {
            "id": "filterTasksByVirtualDesktop",
            "title": qsTr("Current desktop only"),
            "subtitle": qsTr("Only show applications from the current virtual desktop"),
            "icon": "view_carousel",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.filterTasksByVirtualDesktop)
        },
        {
            "id": "showRunningIndicators",
            "title": qsTr("Running indicators"),
            "subtitle": qsTr("Show active and running app indicators below Shelf icons"),
            "icon": "radio_button_checked",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.showRunningIndicators)
        },
        {
            "id": "showTooltips",
            "title": qsTr("App tooltips"),
            "subtitle": qsTr("Show app names when the pointer rests on a Shelf item"),
            "icon": "tooltip",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.showTooltips)
        }
    ]

    readonly property var launcherRows: [
        {
            "id": "launcherDefaultPage",
            "title": qsTr("Open Launcher to"),
            "subtitle": qsTr("Choose the first page shown when Launcher opens"),
            "icon": "home",
            "tone": "primary",
            "trailingKind": "segmented",
            "options": pageOptions,
            "currentIndex": optionIndex(pageOptions, draft.launcherDefaultPage || "home")
        },
        {
            "id": "launcherWidth",
            "title": qsTr("Launcher width"),
            "subtitle": qsTr("Keep the centered Spotlight-like layout while changing its width"),
            "icon": "width",
            "tone": "secondary",
            "trailingKind": "segmented",
            "options": widthOptions,
            "currentIndex": optionIndex(widthOptions, draft.launcherWidth || "standard")
        },
        {
            "id": "launcherShowFavorites",
            "title": qsTr("Pinned apps"),
            "subtitle": qsTr("Show pinned applications on the Launcher Home page"),
            "icon": "keep",
            "tone": "tertiary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.launcherShowFavorites)
        },
        {
            "id": "launcherShowRecents",
            "title": qsTr("Recent items"),
            "subtitle": qsTr("Show recently used applications and documents on Home"),
            "icon": "history",
            "tone": "secondary",
            "trailingKind": "toggle",
            "checked": Boolean(draft.launcherShowRecents)
        }
    ]

    Component.onCompleted: loadDraft()

    Connections {
        target: ShelfBackend
        function onChanged() {
            if (!root.hasChanges)
                root.loadDraft()
        }
        function onSaved() { root.loadDraft() }
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Shelf & Launcher")
        subtitle: qsTr("Configure the Meo Shelf and the centered application Launcher")

        Column {
            width: parent.width
            visible: ShelfBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Shelf needs attention")
                text: qsTr("Your current Shelf setup remains unchanged.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(ShelfBackend.error)
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: ShelfBackend.available

            RowLayout {
                anchors.fill: parent
                spacing: MeoTheme.space12

                MeoIcon {
                    icon: "dock_to_bottom"
                    size: 28
                    color: MeoTheme.primary
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space2

                    MeoText {
                        Layout.fillWidth: true
                        text: qsTr("Meo Shelf")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: ShelfBackend.summary
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: ShelfBackend.available
            title: qsTr("Shelf")
            subtitle: ""
            model: root.shelfRows
            onRowToggled: (index, checked, row) => root.setValue(row.id, checked)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: ShelfBackend.available
            title: qsTr("Launcher")
            subtitle: qsTr("Search remains centered and uses the existing KRunner/Kicker backends.")
            model: root.launcherRows

            onRowToggled: (index, checked, row) => root.setValue(row.id, checked)
            onRowOptionSelected: (index, optionIndex, option, row) => {
                if (option && option.value !== undefined)
                    root.setValue(row.id, option.value)
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                const option = row.options && optionIndex >= 0 ? row.options[optionIndex] : null
                if (option && option.value !== undefined)
                    root.setValue(row.id, option.value)
            }
        }

        Flow {
            width: parent.width
            visible: ShelfBackend.available
            spacing: MeoTheme.space8

            MeoButton {
                text: ShelfBackend.busy ? qsTr("Applying…") : qsTr("Apply changes")
                type: "filled"
                enabled: root.hasChanges && !ShelfBackend.busy
                onClicked: ShelfBackend.save(root.draft)
            }

            MeoButton {
                text: qsTr("Restore defaults")
                type: "tonal"
                enabled: !ShelfBackend.busy
                onClicked: ShelfBackend.reset()
            }

            MeoButton {
                text: qsTr("Refresh")
                type: "text"
                enabled: !ShelfBackend.busy
                onClicked: ShelfBackend.refresh()
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !ShelfBackend.available && !ShelfBackend.busy
            icon: "dock_to_bottom"
            title: qsTr("Meo Shelf is unavailable")
            description: qsTr("Add the Meo Shelf to the active Plasma layout, then refresh this page.")
            actionText: qsTr("Refresh")
            onActionClicked: ShelfBackend.refresh()
        }
    }
}
