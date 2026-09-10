import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    function iconForFact(label) {
        if (label === qsTr("Operating system")) return "computer"
        if (label === qsTr("Kernel")) return "developer_board"
        if (label === qsTr("Architecture")) return "memory"
        if (label === qsTr("Memory")) return "memory_alt"
        if (label === qsTr("Host name")) return "dns"
        if (label === qsTr("User") || label === qsTr("Meo Account")) return "account_circle"
        if (label === qsTr("KDE platform")) return "desktop_windows"
        if (label === qsTr("Qt")) return "code"
        if (label === qsTr("Meo Settings")) return "settings"
        return "info"
    }

    function factRows(fromIndex, toIndex) {
        const rows = []
        const facts = root.displayFacts
        const end = Math.min(toIndex, facts.length)
        for (let index = fromIndex; index < end; ++index) {
            const fact = facts[index]
            rows.push({
                "title": fact.label,
                "subtitle": fact.value,
                "leadingIcon": root.iconForFact(fact.label),
                "leadingTone": index < 5 ? "primary" : "neutral",
                "leadingStyle": "tonal",
                "trailingKind": "none",
                "interactive": false
            })
        }
        return rows
    }

    readonly property var systemRows: factRows(0, Math.min(6, displayFacts.length))
    readonly property var runtimeRows: factRows(Math.min(6, displayFacts.length), displayFacts.length)
    readonly property var displayFacts: {
        const facts = []
        const systemFacts = SystemInfoBackend.facts
        for (let index = 0; index < systemFacts.length; ++index) {
            const fact = systemFacts[index]
            if (fact.label === qsTr("User") && AccountBackend.signedIn) {
                facts.push({
                    "label": qsTr("Meo Account"),
                    "value": AccountBackend.cloudId !== ""
                             ? qsTr("%1 · %2").arg(AccountBackend.cloudName || qsTr("Meo Account"))
                                                   .arg(AccountBackend.cloudId)
                             : (AccountBackend.cloudName || qsTr("Meo Account"))
                })
            } else {
                facts.push(fact)
            }
        }
        return facts
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("About")
        subtitle: qsTr("Device, operating system, and runtime information.")

        MeoCard {
            width: parent.width
            type: "filled"

            Column {
                width: parent.width
                spacing: MeoTheme.space8

                MeoIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    icon: "computer"
                    size: 56 * MeoTheme.globalScale
                    color: MeoTheme.primary
                }
                MeoText {
                    width: parent.width
                    text: SystemInfoBackend.operatingSystemName || qsTr("MeoArch")
                    typeRole: "headline"
                    typeSize: "medium"
                    emphasized: true
                    horizontalAlignment: Text.AlignHCenter
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: SystemInfoBackend.deviceName || qsTr("This device")
                    typeRole: "body"
                    typeSize: "medium"
                    horizontalAlignment: Text.AlignHCenter
                    color: MeoTheme.contentOnSurfaceVariant
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("System")
            model: root.systemRows
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Software")
            model: root.runtimeRows
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("More")
            model: [{
                "title": qsTr("Refresh information"),
                "subtitle": qsTr("Read the latest device and runtime facts"),
                "leadingIcon": "refresh",
                "leadingStyle": "tonal",
                "trailingKind": "button",
                "actionText": qsTr("Refresh")
            }, {
                "title": qsTr("Advanced system information"),
                "subtitle": KcmBridge.isAvailable("kcm_about-distro")
                            ? qsTr("Open the maintained KDE information tool")
                            : qsTr("The advanced system information tool is not installed"),
                "leadingIcon": "info",
                "leadingStyle": "tonal",
                "trailingKind": "navigation",
                "enabled": KcmBridge.isAvailable("kcm_about-distro")
            }]
            onRowActionTriggered: (index, row) => {
                if (index === 0)
                    SystemInfoBackend.refresh()
            }
            onRowActivated: (index, row) => {
                if (index === 1 && row.enabled)
                    root.navigateTo("kcm:kcm_about-distro")
            }
        }
    }
}
