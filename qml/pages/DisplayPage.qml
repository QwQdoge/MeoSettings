import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    Component.onDestruction: DisplayBackend.revertChanges()

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

    // Dedicated native pages own screen-lock and display-idle policies.
    readonly property var lockDisplayRows: {
        const rows = []
        rows.push({
            "title": qsTr("Screen lock"), "subtitle": qsTr("Configure automatic lock and resume behavior"),
            "icon": "lock", "tone": "secondary", "route": "screen-lock", "trailingKind": "navigation"
        })
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
                "subtitle": qsTr("Resolution, scale, refresh rate, HDR, VRR, and arrangement"),
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

        MeoCard {
            width: parent.width
            visible: DisplayBackend.confirmationPending
            type: "filled"
            ColumnLayout {
                anchors.fill: parent
                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Keep these display settings? Reverting in %1 seconds.").arg(DisplayBackend.confirmationSeconds)
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space8
                    MeoButton { text: qsTr("Keep changes"); onClicked: DisplayBackend.keepChanges() }
                    MeoButton { text: qsTr("Revert"); type: "tonal"; onClicked: DisplayBackend.revertChanges() }
                }
            }
        }

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
                text: qsTr("Check that the display is connected, then refresh the display list.")
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

        Column {
            width: parent.width; spacing: MeoTheme.space8
            visible: Platform.nightLightAvailable && Object.keys(Platform.nightLightSettings).length > 0
            property var settings: Platform.nightLightSettings
            property int chosenMode: settings.mode === undefined ? 1 : settings.mode
            MeoExposedDropdown {
                width: parent.width; label: qsTr("Night Light schedule")
                model: [qsTr("Constant warm color"), qsTr("Follow the system day/night schedule")]
                text: model[parent.chosenMode]
                onSelected: (index, value) => parent.chosenMode = index
            }
            MeoTextField {
                id: dayTemperature; width: parent.width; label: qsTr("Day temperature (K)")
                text: String(parent.settings.dayTemperature || 6500)
                validator: IntValidator { bottom: 1000; top: 6500 }
            }
            MeoTextField {
                id: nightTemperature; width: parent.width; label: qsTr("Night temperature (K)")
                text: String(parent.settings.nightTemperature || 4500)
                validator: IntValidator { bottom: 1000; top: 6500 }
            }
            MeoButton {
                text: qsTr("Apply Night Light settings"); type: "tonal"
                enabled: dayTemperature.acceptableInput && nightTemperature.acceptableInput
                onClicked: Platform.configureNightLight(parent.chosenMode, Number(dayTemperature.text), Number(nightTemperature.text))
            }
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
                    text: qsTr("This device does not make brightness or Night Light available here. More display settings are available below when installed.")
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
                text: DisplayBackend.busy ? qsTr("Refreshing…") : qsTr("Refresh")
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
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: displayControls.implicitHeight + 2 * MeoTheme.space16
                    type: "elevated"
                    interactive: false

                    Column {
                        id: displayControls
                        anchors.fill: parent
                        spacing: MeoTheme.space12

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
                                  ? qsTr("%1 × %2 · %3 Hz · %4× scale")
                                        .arg(modelData.width)
                                        .arg(modelData.height)
                                        .arg(Math.round(modelData.refreshRate))
                                        .arg(Number(modelData.scale).toFixed(2))
                                  : qsTr("Mode information unavailable")
                            typeRole: "body"
                            typeSize: "medium"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                        MeoText {
                            text: modelData.enabled ? qsTr("Enabled") : qsTr("Disabled")
                            typeRole: "label"
                            typeSize: "medium"
                            color: modelData.enabled ? MeoTheme.primary : MeoTheme.contentOnSurfaceVariant
                        }
                        MeoExposedDropdown {
                            width: parent.width
                            label: qsTr("Resolution & refresh rate")
                            model: modelData.modes
                            textRole: "label"
                            valueRole: "id"
                            currentValue: modelData.modeId
                            enabled: !DisplayBackend.busy && modelData.enabled
                            onSelected: (index, value) => DisplayBackend.applyOutput(modelData.id, {"modeId": value})
                        }
                        MeoExposedDropdown {
                            width: parent.width
                            label: qsTr("Scale")
                            visible: modelData.scaleSupported
                            model: ["75%", "100%", "125%", "150%", "175%", "200%", "250%", "300%"]
                            text: Math.round(Number(modelData.scale) * 100) + "%"
                            enabled: !DisplayBackend.busy && modelData.enabled
                            onSelected: (index, value) => DisplayBackend.applyOutput(modelData.id, {"scale": parseInt(value) / 100})
                        }
                        MeoExposedDropdown {
                            width: parent.width
                            label: qsTr("Orientation")
                            model: [qsTr("Landscape"), qsTr("Portrait"), qsTr("Landscape, inverted"), qsTr("Portrait, inverted")]
                            currentIndex: [1, 2, 4, 8].indexOf(Number(modelData.rotation))
                            enabled: !DisplayBackend.busy && modelData.enabled
                            onSelected: (index, value) => DisplayBackend.applyOutput(modelData.id, {"rotation": [1, 2, 4, 8][index]})
                        }
                        RowLayout {
                            width: parent.width
                            MeoTextField {
                                id: positionX
                                Layout.fillWidth: true
                                label: qsTr("Horizontal position")
                                text: String(modelData.x)
                                validator: IntValidator { bottom: -16384; top: 16384 }
                                enabled: !DisplayBackend.busy && modelData.enabled
                            }
                            MeoTextField {
                                id: positionY
                                Layout.fillWidth: true
                                label: qsTr("Vertical position")
                                text: String(modelData.y)
                                validator: IntValidator { bottom: -16384; top: 16384 }
                                enabled: !DisplayBackend.busy && modelData.enabled
                            }
                        }
                        Flow {
                            width: parent.width
                            spacing: MeoTheme.space8
                            MeoButton {
                                text: qsTr("Apply position")
                                type: "tonal"
                                enabled: !DisplayBackend.busy && modelData.enabled && positionX.acceptableInput && positionY.acceptableInput
                                onClicked: DisplayBackend.applyOutput(modelData.id, {"x": Number(positionX.text), "y": Number(positionY.text)})
                            }
                            MeoButton {
                                text: modelData.primary ? qsTr("Primary display") : qsTr("Make primary")
                                type: "tonal"
                                enabled: !DisplayBackend.busy && modelData.enabled && !modelData.primary
                                onClicked: DisplayBackend.applyOutput(modelData.id, {"primary": true})
                            }
                            MeoButton {
                                text: modelData.enabled ? qsTr("Disable display") : qsTr("Enable display")
                                type: "text"
                                enabled: !DisplayBackend.busy
                                onClicked: DisplayBackend.applyOutput(modelData.id, {"enabled": !modelData.enabled})
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
            subtitle: qsTr("Use these tools for display options that need device-specific recovery or scheduling.")
            model: root.advancedRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        RepairEntry {
            category: "display"
            entryTitle: qsTr("Troubleshoot displays")
        }
    }
}
