import QtQuick
import MeoUI
import MeoKDE 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function stateText(available, readyText) {
        return available ? readyText : qsTr("Unavailable on this system")
    }

    readonly property var desktopRows: [
        {
            "title": qsTr("KDE settings modules"),
            "subtitle": KcmBridge.launcherAvailable
                        ? qsTr("%1 installed modules are available through the protected KDE bridge").arg(KcmBridge.modules.length)
                        : qsTr("The Plasma settings-module launcher is unavailable"),
            "icon": "settings", "tone": "neutral", "route": "kcm:kcm_landingpage",
            "enabled": KcmBridge.isAvailable("kcm_landingpage"), "trailingKind": "navigation"
        },
        {
            "title": qsTr("Material theme pipeline"),
            "subtitle": root.stateText(MeoShellTheme.ready && DynamicColorBackend.available,
                                        qsTr("KDE palette → Meo HCT roles → MeoUI, shell, and application icons")),
            "icon": "palette", "tone": "tertiary", "route": "appearance",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("NetworkManager"),
            "subtitle": root.stateText(Capabilities.network,
                                        NetworkBackend.connected
                                        ? qsTr("Connected to %1").arg(NetworkBackend.connectionName)
                                        : qsTr("Network service connected; no active Wi-Fi connection")),
            "icon": "wifi", "tone": "primary", "route": "wifi",
            "enabled": Capabilities.wifi, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Bluetooth"),
            "subtitle": root.stateText(Capabilities.bluetooth,
                                        BluetoothBackend.enabled ? qsTr("BlueZ connected and Bluetooth is on")
                                                                 : qsTr("BlueZ connected and Bluetooth is off")),
            "icon": "bluetooth", "tone": "secondary", "route": "bluetooth",
            "enabled": Capabilities.bluetooth, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Audio service"),
            "subtitle": root.stateText(Capabilities.audio,
                                        AudioBackend.pipeWire ? qsTr("PipeWire connected · %1").arg(AudioBackend.outputName)
                                                              : qsTr("PulseAudio-compatible service connected · %1").arg(AudioBackend.outputName)),
            "icon": "volume_up", "tone": "primary", "route": "sound",
            "enabled": Capabilities.audio, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Display service"),
            "subtitle": root.stateText(Capabilities.display, DisplayBackend.summary),
            "icon": "monitor", "tone": "secondary", "route": "display",
            "enabled": Capabilities.display, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Power management"),
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
            "title": qsTr("OmniStore application data"),
            "subtitle": OmniStoreAppsBackend.exporterAvailable ? OmniStoreAppsBackend.summary
                                                                 : qsTr("OmniStore inventory exporter is unavailable"),
            "icon": "store", "tone": "tertiary", "route": "storage",
            "enabled": true, "trailingKind": "navigation"
        },
        {
            "title": qsTr("Update orchestration"),
            "subtitle": UpdatesBackend.orchestratorAvailable
                        ? qsTr("OmniStore update state is connected to Meo Settings")
                        : qsTr("Read-only pacman metadata is available; OmniStore orchestration is not connected"),
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
        title: root.isCompact ? "" : qsTr("Desktop integration")
        subtitle: qsTr("Live, read-only status for the KDE and Meo services behind every Settings page.")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("KDE & hardware services")
            subtitle: qsTr("Rows are shown only from real backend state. Opening a row never claims support that the service did not report.")
            model: root.desktopRows
            onRowActivated: (index, row) => { if (row.enabled) root.navigateTo(row.route) }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Meo services")
            subtitle: qsTr("Credentials, package changes, and protected operations remain with their owning service.")
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
                    text: qsTr("Integration contract")
                    typeRole: "title"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings owns the unified interface. KDE modules own specialized configuration, Meo Account owns credentials and consent, and OmniStore owns privileged package transactions. This page verifies those connections without moving authority into the UI.")
                    typeRole: "body"; typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
