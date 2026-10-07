import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    readonly property string nightLightSummary: {
        if (!Platform.nightLightEnabled)
            return qsTr("Off")
        if (Platform.nightLightRunning && Platform.nightLightTemperature > 0)
            return qsTr("Active at %1 K").arg(Platform.nightLightTemperature)
        if (Platform.nightLightRunning)
            return qsTr("Active")
        return qsTr("Enabled; waiting for its schedule")
    }

    // These stay as verified handoffs: the desktop KCM owns lock policy and
    // the Power page owns timeout policy, so this page never renders a
    // decorative switch that cannot change the real session.
    readonly property var lockDisplayRows: {
        const rows = []
        if (KcmBridge.isAvailable("kcm_screenlocker")) {
            rows.push({
                "title": qsTr("Lock screen"),
                "subtitle": qsTr("Open screen lock settings"),
                "icon": "lock",
                "tone": "secondary",
                "route": "kcm:kcm_screenlocker",
                "trailingKind": "choice",
                "trailingText": qsTr("Advanced")
            })
        }
        rows.push({
            "title": qsTr("Screen timeout"),
            "subtitle": qsTr("Choose when the display turns off"),
            "icon": "timer",
            "tone": "secondary",
            "route": "power",
            "trailingKind": "navigation"
        })
        return rows
    }

    readonly property var nightLightRows: [{
        "id": "night-light",
        "title": qsTr("Night Light"),
        "subtitle": root.nightLightSummary,
        "icon": "dark_mode",
        "tone": "tertiary",
        "trailingKind": "toggle",
        "checked": Platform.nightLightEnabled
    }]

    // This is a truthful handoff, not a cosmetic font-size slider. KDE owns
    // global font/display scaling and its confirmation/revert behaviour; Meo
    // exposes the same reference-shaped entry point and sends users to the
    // verified Appearance/KCM workflow.
    readonly property var textAppearanceRows: [{
        "title": qsTr("Display size & text"),
        "subtitle": qsTr("Adjust text and interface appearance"),
        "icon": "format_size",
        "tone": "secondary",
        "route": "appearance",
        "trailingKind": "navigation"
    }]

    readonly property var advancedRows: {
        const rows = []
        if (KcmBridge.isAvailable("kcm_nightlight")) {
            rows.push({
                "title": qsTr("Night Light schedule and temperature"),
                "subtitle": qsTr("Set schedules and advanced Night Light behavior"),
                "icon": "schedule",
                "tone": "tertiary",
                "route": "kcm:kcm_nightlight",
                "trailingKind": "choice",
                "trailingText": qsTr("Advanced")
            })
        }
        if (KcmBridge.isAvailable("kcm_kscreen")) {
            rows.push({
                "title": qsTr("Display layout and modes"),
                "subtitle": qsTr("Resolution, refresh rate, HDR, VRR, and arrangement"),
                "icon": "monitor",
                "tone": "secondary",
                "route": "kcm:kcm_kscreen",
                "trailingKind": "choice",
                "trailingText": qsTr("Advanced")
            })
        }
        return rows
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Display & touch")
        subtitle: ""

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Lock display")
            subtitle: ""
            model: root.lockDisplayRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        Column {
            width: parent.width
            visible: DisplayBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Display needs attention")
                text: qsTr("Check the current display state, then try the setting again.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(DisplayBackend.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
            MeoButton {
                text: qsTr("Dismiss")
                type: "text"
                size: "s"
                onClicked: DisplayBackend.clearError()
            }
        }

        Column {
            width: parent.width
            visible: Platform.lastError !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("This display setting could not be applied")
                text: qsTr("Check the current display state, then try the setting again.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(Platform.lastError)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            width: parent.width
            visible: Platform.brightnessAvailable
            spacing: MeoTheme.space8

            MeoText {
                Layout.fillWidth: true
                text: qsTr("Brightness")
                typeRole: "title"
                typeSize: "small"
                emphasized: true
                color: MeoTheme.contentOnSurface
            }

            MeoText {
                Layout.fillWidth: true
                text: qsTr("Each control changes the current brightness of its reported display")
                typeRole: "body"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: Platform.brightnessDisplays

                delegate: MeoCard {
                    required property var modelData
                    Layout.fillWidth: true
                    type: "outlined"

                    MeoSteppedSlider {
                        anchors.fill: parent
                        readonly property real maximum: Math.max(1, Number(modelData.maximum))
                        readonly property real brightness: Math.max(0, Math.min(maximum, Number(modelData.brightness)))
                        title: modelData.label
                               ? modelData.label
                               : qsTr("Display brightness")
                        supportingText: modelData.internal
                                        ? qsTr("Built-in display")
                                        : qsTr("External display")
                        from: 0
                        to: maximum
                        value: brightness
                        stepSize: 1
                        discrete: true
                        showValueLabel: true
                        valueText: Math.round((brightness * 100) / maximum) + "%"
                        onMoved: Platform.setBrightness(modelData.id, Math.round(value))
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Platform.nightLightAvailable
            title: qsTr("Night Light")
            subtitle: ""
            model: root.nightLightRows
            onRowToggled: (index, checked, row) => {
                if (row.id === "night-light")
                    Platform.nightLightEnabled = checked
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Appearance")
            subtitle: ""
            model: root.textAppearanceRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !Platform.brightnessAvailable && !Platform.nightLightAvailable

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "info"; size: 24; color: MeoTheme.primary }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("This device does not make brightness or Night Light available here. Connected display scale is still configurable below.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        Flow {
            width: parent.width
            spacing: 8 * MeoTheme.globalScale
            MeoButton {
                text: DisplayBackend.busy ? qsTr("Applying…") : qsTr("Refresh")
                type: "tonal"
                enabled: !DisplayBackend.busy
                onClicked: DisplayBackend.refresh()
            }
        }

        MeoText {
            text: DisplayBackend.summary
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            color: MeoTheme.contentOnSurface
            visible: DisplayBackend.available
        }

        GridLayout {
            width: parent.width
            columns: Math.max(1, Math.min(3, page.contentPreferredColumns))
            columnSpacing: 12 * MeoTheme.globalScale
            rowSpacing: 12 * MeoTheme.globalScale
            visible: DisplayBackend.outputs.length > 0

            Repeater {
                model: DisplayBackend.outputs

                delegate: MeoCard {
                    id: displayCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: 286 * MeoTheme.globalScale
                    type: "elevated"
                    interactive: false

                    Column {
                        anchors.fill: parent
                        spacing: 10 * MeoTheme.globalScale

                        RowLayout {
                            width: parent.width
                            spacing: 12 * MeoTheme.globalScale
                            MeoIcon { icon: "monitor"; size: 28; color: MeoTheme.primary }
                            ColumnLayout {
                                Layout.fillWidth: true
                                MeoText {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    typeRole: "title"
                                    typeSize: "small"
                                    emphasized: true
                                    color: MeoTheme.contentOnSurface
                                    elide: Text.ElideRight
                                }
                                MeoText {
                                    Layout.fillWidth: true
                                    text: modelData.connector + (modelData.primary ? qsTr(" · Primary") : "")
                                    typeRole: "body"
                                    typeSize: "small"
                                    color: MeoTheme.contentOnSurfaceVariant
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MeoText {
                            width: parent.width
                            text: modelData.width > 0
                                  ? qsTr("%1 × %2 · %3 Hz")
                                        .arg(modelData.width)
                                        .arg(modelData.height)
                                        .arg(Math.round(modelData.refreshRate))
                                  : qsTr("Mode information unavailable")
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }

                        MeoSteppedSlider {
                            id: scaleSlider
                            width: parent.width
                            title: qsTr("Scale")
                            supportingText: qsTr("Apply a fractional scale to this display")
                            from: 0.5
                            to: 4.0
                            value: Number(modelData.scale)
                            stepSize: 0.05
                            discrete: true
                            showValueLabel: true
                            valueText: Math.round(value * 100) + "%"
                            enabled: !DisplayBackend.busy && modelData.enabled
                        }

                        Flow {
                            width: parent.width
                            spacing: 8 * MeoTheme.globalScale

                            MeoButton {
                                text: qsTr("Apply scale")
                                type: "tonal"
                                size: "s"
                                enabled: !DisplayBackend.busy
                                         && modelData.enabled
                                         && Math.abs(Number(scaleSlider.value) - Number(modelData.scale)) > 0.001
                                onClicked: DisplayBackend.setScale(modelData.id, scaleSlider.value)
                            }

                            MeoButton {
                                visible: modelData.enabled && !modelData.primary
                                text: qsTr("Make primary")
                                type: "text"
                                size: "s"
                                enabled: !DisplayBackend.busy
                                onClicked: DisplayBackend.setPrimary(modelData.id)
                            }
                        }
                    }
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: DisplayBackend.available && DisplayBackend.outputs.length === 0
            icon: "monitor_off"
            title: qsTr("No connected displays")
            description: qsTr("No display configuration was reported by this session.")
            actionText: qsTr("Refresh")
            onActionClicked: DisplayBackend.refresh()
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.advancedRows.length > 0
            title: qsTr("Advanced display configuration")
            subtitle: qsTr("Use this for resolution, refresh rate, HDR, VRR, arrangement, and recovery-sensitive display changes that are not native here yet.")
            model: root.advancedRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        RepairEntry {
            category: "display"
            entryTitle: qsTr("Troubleshoot displays")
        }
    }
}
