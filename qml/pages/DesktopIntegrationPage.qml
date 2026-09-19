import QtQuick
import MeoUI
import MeoKDE 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function stateText(available, readyText) {
        return available ? readyText : qsTr("Not available on this device")
    }

    readonly property var desktopRows: [
        {
            "title": qsTr("Advanced settings"),
            "subtitle": KcmBridge.launcherAvailable
                        ? qsTr("%1 advanced system tools are ready when you need a setting Meo does not manage here").arg(KcmBridge.modules.length)
                        : qsTr("No advanced system tools are available on this device"),
            "icon": "settings", "tone": "neutral", "route": "kcm:kcm_landingpage",
            "enabled": KcmBridge.isAvailable("kcm_landingpage"), "trailingKind": "navigation"
        },
        {
            "title": qsTr("Meo color theme"),
            "subtitle": root.stateText(MeoShellTheme.ready && DynamicColorBackend.available,
                                        qsTr("System colors are ready across Meo apps and the shell")),
            "icon": "palette", "tone": "tertiary", "route": "appearance",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Network"),
            "subtitle": root.stateText(Capabilities.network,
                                        NetworkBackend.connected
                                        ? qsTr("Connected to %1").arg(NetworkBackend.connectionName)
                                        : qsTr("Network service is ready; no Wi-Fi connection is active")),
            "icon": "wifi", "tone": "primary", "route": "wifi",
            "enabled": Capabilities.wifi, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Bluetooth"),
            "subtitle": root.stateText(Capabilities.bluetooth,
                                        BluetoothBackend.enabled ? qsTr("Bluetooth is on")
                                                                 : qsTr("Bluetooth is off")),
            "icon": "bluetooth", "tone": "secondary", "route": "bluetooth",
            "enabled": Capabilities.bluetooth, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Sound"),
            "subtitle": root.stateText(Capabilities.audio,
                                        AudioBackend.pipeWire ? qsTr("Sound is ready · %1").arg(AudioBackend.outputName)
                                                              : qsTr("Sound is ready · %1").arg(AudioBackend.outputName)),
            "icon": "volume_up", "tone": "primary", "route": "sound",
            "enabled": Capabilities.audio, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Display"),
            "subtitle": root.stateText(Capabilities.display, DisplayBackend.summary),
            "icon": "monitor", "tone": "secondary", "route": "display",
            "enabled": Capabilities.display, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Power"),
            "subtitle": root.stateText(PowerBackend.available, PowerBackend.summary),
            "icon": "battery_full", "tone": "neutral", "route": "power",
            "enabled": PowerBackend.available, "trailingKind": "navigation"
        }
    ]

    readonly property var meoRows: [
        {
            "title": qsTr("Meo Account"),
            "subtitle": AccountBackend.serviceRunning ? AccountBackend.summary
                                                       : qsTr("Account broker is not running"),
            "icon": "account_circle", "tone": "secondary", "route": "accounts",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("OmniStore apps"),
            "subtitle": OmniStoreAppsBackend.exporterAvailable ? OmniStoreAppsBackend.summary
                                                                 : qsTr("OmniStore inventory exporter is unavailable"),
            "icon": "store", "tone": "tertiary", "route": "storage",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Updates"),
            "subtitle": UpdatesBackend.orchestratorAvailable
                        ? qsTr("Update information from OmniStore is ready")
                        : qsTr("Available update information is ready to review"),
            "icon": "system_update", "tone": "primary", "route": "updates",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Control Center"),
            "subtitle": ControlCenterBackend.summary,
            "icon": "tune", "tone": "neutral", "route": "control-center",
            "enabled": ControlCenterBackend.available, "trailingKind": "navigation"
        }
    ]

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("System integration")
        subtitle: qsTr("Check whether the services used by Meo Settings are ready. This page does not change any settings.")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Device services")
            subtitle: qsTr("Only services that report a real status are shown. Open a row to view its settings.")
            model: root.desktopRows
            onRowActivated: (index, row) => { if (row.enabled) root.navigateTo(row.route) }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Meo services")
            subtitle: qsTr("Accounts, package changes, and protected actions stay with the service that manages them.")
            model: root.meoRows
            onRowActivated: (index, row) => { if (row.enabled) root.navigateTo(row.route) }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            Column {
                width: parent.width
                spacing: 6 * MeoTheme.globalScale
                MeoText {
                    text: qsTr("How these services work together")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings brings your settings together. Platform services keep their specialized controls, Meo Account keeps credentials and consent, and OmniStore handles protected package changes.")
                    typeRole: "body"; typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
