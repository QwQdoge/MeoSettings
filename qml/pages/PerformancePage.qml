import QtQuick
import QtQuick.Controls
import MeoUI
import Meo.System 1.0
import org.kde.ksysguard.sensors as Sensors

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property int sampleInterval: 1000

    function clamp(value, minimum, maximum) {
        if (!isFinite(value))
            return minimum
        return Math.max(minimum, Math.min(maximum, value))
    }

    function percent(value) {
        return isFinite(value) && value >= 0 ? Math.round(value) + "%" : "—"
    }

    function formatBytes(bytes) {
        const value = Number(bytes)
        if (!isFinite(value) || value < 0)
            return "—"
        const units = ["B", "KiB", "MiB", "GiB", "TiB"]
        let scaled = value
        let unit = 0
        while (scaled >= 1024 && unit < units.length - 1) {
            scaled /= 1024
            unit++
        }
        const digits = scaled >= 100 || unit === 0 ? 0 : (scaled >= 10 ? 1 : 2)
        return scaled.toFixed(digits) + " " + units[unit]
    }

    function formatRate(bytesPerSecond) {
        const text = formatBytes(bytesPerSecond)
        return text === "—" ? text : text + "/s"
    }

    function sensorText(sensor) {
        return sensor.status === Sensors.Sensor.Ready && sensor.formattedValue !== ""
               ? sensor.formattedValue : "—"
    }

    function modelMaximum(model) {
        if (!model || !model.ready)
            return -1
        let result = -1
        for (let column = 0; column < model.columnCount(); ++column) {
            const value = Number(model.data(model.index(0, column), Sensors.SensorDataModel.Value))
            if (isFinite(value))
                result = Math.max(result, value)
        }
        return result
    }

    function modelSum(model) {
        if (!model || !model.ready)
            return -1
        let result = 0
        let count = 0
        for (let column = 0; column < model.columnCount(); ++column) {
            const value = Number(model.data(model.index(0, column), Sensors.SensorDataModel.Value))
            if (isFinite(value)) {
                result += value
                count++
            }
        }
        return count > 0 ? result : -1
    }

    function profileTitle(profile) {
        if (profile === "performance")
            return qsTr("Performance")
        if (profile === "power-saver")
            return qsTr("Power saver")
        if (profile === "balanced")
            return qsTr("Balanced")
        return profile
    }

    function profileDescription(profile) {
        if (profile === "performance")
            return qsTr("Prioritise responsiveness when the hardware allows it")
        if (profile === "power-saver")
            return qsTr("Reduce power use and background work")
        if (profile === "balanced")
            return qsTr("Balance responsiveness, heat, and battery life")
        return qsTr("Power mode provided by this device")
    }

    function profileIcon(profile) {
        if (profile === "performance")
            return "speed"
        if (profile === "power-saver")
            return "battery_saver"
        return "balance"
    }

    readonly property bool cpuReady: cpuUsage.status === Sensors.Sensor.Ready
    readonly property bool memoryReady: memoryUsed.status === Sensors.Sensor.Ready
                                        && memoryTotal.status === Sensors.Sensor.Ready
                                        && Number(memoryTotal.value) > 0
    readonly property bool gpuReady: gpuUsage.status === Sensors.Sensor.Ready
    readonly property real cpuPercent: cpuReady ? clamp(Number(cpuUsage.value), 0, 100) : -1
    readonly property real memoryPercent: memoryReady
                                          ? clamp(Number(memoryUsed.value) / Number(memoryTotal.value) * 100, 0, 100)
                                          : -1
    readonly property real swapPercent: swapTotal.status === Sensors.Sensor.Ready
                                        && Number(swapTotal.value) > 0
                                        ? clamp(Number(swapUsed.value) / Number(swapTotal.value) * 100, 0, 100)
                                        : -1
    readonly property real gpuPercent: gpuReady ? clamp(Number(gpuUsage.value), 0, 100) : -1
    readonly property real diskPercent: modelMaximum(diskUsageModel)

    readonly property var profileRows: {
        const rows = []
        for (let index = 0; index < Platform.powerProfiles.length; ++index) {
            const profile = Platform.powerProfiles[index]
            rows.push({
                "id": "performance-profile-" + profile,
                "profile": profile,
                "title": root.profileTitle(profile),
                "subtitle": root.profileDescription(profile),
                "icon": root.profileIcon(profile),
                "tone": profile === "performance" ? "tertiary" : "primary",
                "trailingKind": "radio",
                "checked": Platform.activePowerProfile === profile
            })
        }
        return rows
    }

    readonly property var relatedRows: [{
        "id": "power",
        "title": qsTr("Power & battery"),
        "subtitle": PowerBackend.available ? PowerBackend.summary : qsTr("Battery, sleep, and session power settings"),
        "icon": PowerBackend.charging ? "battery_charging_full" : "battery_full",
        "tone": "primary",
        "route": "power",
        "trailingKind": "navigation"
    }, {
        "id": "hardware",
        "title": qsTr("Hardware & drivers"),
        "subtitle": qsTr("Review detected hardware support and graphics drivers"),
        "icon": "memory",
        "tone": "secondary",
        "route": "hardware",
        "trailingKind": "navigation"
    }, {
        "id": "storage",
        "title": qsTr("Storage"),
        "subtitle": StorageBackend.summary,
        "icon": "storage",
        "tone": "tertiary",
        "route": "storage",
        "trailingKind": "navigation"
    }]

    Sensors.Sensor {
        id: cpuUsage
        sensorId: "cpu/all/usage"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: cpuFrequency
        sensorId: "cpu/all/averageFrequency"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: cpuTemperature
        sensorId: "cpu/all/maximumTemperature"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: gpuUsage
        sensorId: "gpu/all/usage"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: gpuUsedVram
        sensorId: "gpu/all/usedVram"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: gpuTotalVram
        sensorId: "gpu/all/totalVram"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: memoryUsed
        sensorId: "memory/physical/used"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: memoryTotal
        sensorId: "memory/physical/total"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: swapUsed
        sensorId: "memory/swap/used"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: swapTotal
        sensorId: "memory/swap/total"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: networkDownload
        sensorId: "network/all/download"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.Sensor {
        id: networkUpload
        sensorId: "network/all/upload"
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.SensorDataModel {
        id: gpuTemperatureModel
        sensors: ["gpu/(?!all).*/temperature"]
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.SensorDataModel {
        id: diskReadModel
        sensors: ["disk/(?!all).*/read"]
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.SensorDataModel {
        id: diskWriteModel
        sensors: ["disk/(?!all).*/write"]
        enabled: root.visible
        updateRateLimit: root.sampleInterval
    }
    Sensors.SensorDataModel {
        id: diskUsageModel
        sensors: ["disk/(?!all).*/usedPercent"]
        enabled: root.visible
        updateRateLimit: 2000
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 820 * MeoTheme.globalScale
        expandedWidth: 900 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Performance")
        subtitle: qsTr("Live resource usage and safe system performance controls")

        MeoBanner {
            width: parent.width
            visible: Platform.lastError !== ""
            title: qsTr("The power mode could not be changed")
            text: Platform.lastError
            icon: "error"
            tone: "error"
        }

        MeoCard {
            width: parent.width
            type: "filled"

            Row {
                width: parent.width
                spacing: 16 * MeoTheme.globalScale

                MeoIcon {
                    icon: "speed"
                    size: 32
                    color: MeoTheme.primary
                }

                Column {
                    width: parent.width - 48 * MeoTheme.globalScale
                    spacing: 4 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        text: Platform.powerProfilesAvailable
                              ? qsTr("Current mode: %1").arg(root.profileTitle(Platform.activePowerProfile))
                              : qsTr("Live performance overview")
                        typeRole: "title"
                        typeSize: "medium"
                        emphasized: true
                    }
                    MeoText {
                        width: parent.width
                        text: qsTr("Monitoring uses KDE System Stats and pauses when this page is not visible.")
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Grid {
            width: parent.width
            columns: root.isCompact ? 1 : 2
            columnSpacing: 12 * MeoTheme.globalScale
            rowSpacing: 12 * MeoTheme.globalScale

            ResourceCard {
                width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                title: qsTr("CPU")
                iconName: "memory"
                valueText: root.percent(root.cpuPercent)
                progressValue: root.cpuPercent / 100
                details: {
                    const values = []
                    const frequency = root.sensorText(cpuFrequency)
                    const temperature = root.sensorText(cpuTemperature)
                    if (frequency !== "—") values.push(frequency)
                    if (temperature !== "—") values.push(temperature)
                    return values.length > 0 ? values.join(" · ") : qsTr("Waiting for CPU sensors")
                }
            }

            ResourceCard {
                width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                title: qsTr("Memory")
                iconName: "memory_alt"
                valueText: root.percent(root.memoryPercent)
                progressValue: root.memoryPercent / 100
                details: root.memoryReady
                         ? qsTr("%1 used of %2")
                             .arg(root.formatBytes(Number(memoryUsed.value)))
                             .arg(root.formatBytes(Number(memoryTotal.value)))
                         : qsTr("Waiting for memory sensors")
            }

            ResourceCard {
                width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                title: qsTr("GPU")
                iconName: "desktop_windows"
                valueText: root.percent(root.gpuPercent)
                progressValue: root.gpuPercent / 100
                details: {
                    const values = []
                    if (gpuUsedVram.status === Sensors.Sensor.Ready
                            && gpuTotalVram.status === Sensors.Sensor.Ready
                            && Number(gpuTotalVram.value) > 0) {
                        values.push(qsTr("%1 / %2 VRAM")
                                    .arg(root.formatBytes(Number(gpuUsedVram.value)))
                                    .arg(root.formatBytes(Number(gpuTotalVram.value))))
                    }
                    const temperature = root.modelMaximum(gpuTemperatureModel)
                    if (temperature >= 0)
                        values.push(qsTr("%1 °C").arg(Math.round(temperature)))
                    return values.length > 0 ? values.join(" · ") : qsTr("GPU sensor unavailable")
                }
            }

            ResourceCard {
                width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                title: qsTr("Swap")
                iconName: "swap_horiz"
                valueText: root.percent(root.swapPercent)
                progressValue: root.swapPercent / 100
                details: swapTotal.status === Sensors.Sensor.Ready && Number(swapTotal.value) > 0
                         ? qsTr("%1 used of %2")
                             .arg(root.formatBytes(Number(swapUsed.value)))
                             .arg(root.formatBytes(Number(swapTotal.value)))
                         : qsTr("No swap or sensor unavailable")
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                MeoText {
                    text: qsTr("Network")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                }
                MetricRow {
                    width: parent.width
                    label: qsTr("Download")
                    iconName: "download"
                    value: networkDownload.status === Sensors.Sensor.Ready ? networkDownload.formattedValue : "—"
                }
                MetricRow {
                    width: parent.width
                    label: qsTr("Upload")
                    iconName: "upload"
                    value: networkUpload.status === Sensors.Sensor.Ready ? networkUpload.formattedValue : "—"
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    MeoText {
                        width: parent.width * 0.62
                        text: qsTr("Storage")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoText {
                        width: parent.width * 0.38
                        text: root.diskPercent >= 0
                              ? qsTr("%1% max used").arg(Math.round(root.diskPercent))
                              : StorageBackend.summary
                        typeRole: "label"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }
                MetricRow {
                    width: parent.width
                    label: qsTr("Read")
                    iconName: "download"
                    value: root.modelSum(diskReadModel) >= 0
                           ? root.formatRate(root.modelSum(diskReadModel)) : "—"
                }
                MetricRow {
                    width: parent.width
                    label: qsTr("Write")
                    iconName: "upload"
                    value: root.modelSum(diskWriteModel) >= 0
                           ? root.formatRate(root.modelSum(diskWriteModel)) : "—"
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Platform.powerProfilesAvailable
            title: qsTr("Performance mode")
            subtitle: qsTr("Use the system-supported power profile instead of hardware-specific overclock controls")
            model: root.profileRows
            onRowToggled: (index, checked, row) => {
                if (checked && row.profile)
                    Platform.activePowerProfile = row.profile
            }
        }

        MeoCard {
            width: parent.width
            visible: !Platform.powerProfilesAvailable
            type: "outlined"

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "info"; size: 24; color: MeoTheme.primary }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("This device does not expose system power profiles. Monitoring is still available; Meo does not fall back to unsafe vendor-specific tuning commands.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Related controls")
            subtitle: qsTr("Battery, hardware, and storage stay in their dedicated settings pages")
            model: root.relatedRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }
    }

    component ResourceCard: MeoCard {
        id: card
        required property string title
        required property string iconName
        required property string valueText
        property string details: ""
        property real progressValue: -1

        type: "filled"
        implicitHeight: 148 * MeoTheme.globalScale

        Column {
            width: parent.width
            spacing: 10 * MeoTheme.globalScale

            Row {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale
                MeoIcon { icon: card.iconName; size: 24; color: MeoTheme.primary }
                MeoText {
                    width: parent.width - 112 * MeoTheme.globalScale
                    text: card.title
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                }
                MeoText {
                    width: 78 * MeoTheme.globalScale
                    text: card.valueText
                    typeRole: "title"
                    typeSize: "medium"
                    emphasized: true
                    horizontalAlignment: Text.AlignRight
                }
            }

            MeoProgressBar {
                width: parent.width
                type: "linear"
                value: card.progressValue >= 0 ? root.clamp(card.progressValue, 0, 1) : 0
            }

            MeoText {
                width: parent.width
                text: card.details
                typeRole: "body"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                elide: Text.ElideRight
            }
        }
    }

    component MetricRow: Row {
        required property string label
        required property string iconName
        required property string value
        spacing: 10 * MeoTheme.globalScale

        MeoIcon { icon: parent.iconName; size: 20; color: MeoTheme.contentOnSurfaceVariant }
        MeoText {
            width: parent.width - 170 * MeoTheme.globalScale
            text: parent.label
            typeRole: "body"
            typeSize: "medium"
        }
        MeoText {
            width: 140 * MeoTheme.globalScale
            text: parent.value
            typeRole: "label"
            typeSize: "medium"
            emphasized: true
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }
}
