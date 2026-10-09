import QtQuick
import QtQuick.Layouts
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Wired, VPN & saved networks")
        MeoBanner { width: parent.width; visible: NetworkBackend.error !== ""; title: qsTr("Network setting needs attention"); text: NetworkBackend.error; tone: "error" }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Connectivity")
            model: [{"id": "enabled", "title": qsTr("Networking enabled"), "subtitle": qsTr("Turn off to disconnect NetworkManager connections"), "trailingKind": "toggle", "checked": NetworkBackend.networkingEnabled, "enabled": !NetworkBackend.busy}]
            onRowToggled: (index, checked, row) => NetworkBackend.setNetworkingEnabled(checked)
        }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Network adapters")
            model: NetworkBackend.devices.map(device => ({"title": device.name, "subtitle": device.wired ? qsTr("Ethernet") : qsTr("Wi-Fi"), "trailingKind": "status", "trailingText": device.connected ? qsTr("Connected") : qsTr("Disconnected"), "interactive": false}))
        }
        Repeater {
            model: NetworkBackend.profiles
            delegate: Column {
                id: entry
                required property var modelData
                width: parent.width; spacing: MeoTheme.space12
                property string ipMethod: modelData.ipv4Method === "manual" ? "manual" : "auto"
                MeoSettingsGroup {
                    width: parent.width; title: entry.modelData.name
                    model: [
                        {"id": "connect", "title": entry.modelData.type, "subtitle": entry.modelData.connecting ? qsTr("Connecting") : entry.modelData.active ? qsTr("Connected") : qsTr("Disconnected"), "trailingKind": "action", "actionText": entry.modelData.active ? qsTr("Disconnect") : qsTr("Connect"), "enabled": !NetworkBackend.busy},
                        {"id": "automatic", "title": qsTr("Connect automatically"), "trailingKind": "toggle", "checked": entry.modelData.autoconnect, "enabled": !NetworkBackend.busy}
                    ]
                    onRowActionTriggered: (index, row) => { if (row.id === "connect") { if (entry.modelData.active) NetworkBackend.deactivateProfile(entry.modelData.uuid); else NetworkBackend.activateProfile(entry.modelData.uuid) } }
                    onRowToggled: (index, checked, row) => { if (row.id === "automatic") NetworkBackend.configureProfile(entry.modelData.uuid, {"autoconnect": checked}) }
                }
                MeoExposedDropdown {
                    width: parent.width; label: qsTr("Data usage")
                    model: [qsTr("Detect automatically"), qsTr("Metered"), qsTr("Unmetered")]
                    text: model[Number(entry.modelData.metered)] || model[0]
                    enabled: !NetworkBackend.busy
                    onSelected: (index, value) => NetworkBackend.configureProfile(entry.modelData.uuid, {"metered": index})
                }
                Column {
                    width: parent.width; spacing: MeoTheme.space12
                    visible: entry.modelData.basicIpv4
                    MeoExposedDropdown {
                        width: parent.width; label: qsTr("IPv4 configuration")
                        model: [qsTr("Automatic (DHCP)"), qsTr("Manual")]
                        text: entry.ipMethod === "auto" ? model[0] : model[1]
                        onSelected: (index, value) => entry.ipMethod = index === 0 ? "auto" : "manual"
                    }
                    MeoTextField { id: address; width: parent.width; visible: entry.ipMethod === "manual"; label: qsTr("IPv4 address"); text: entry.modelData.address; placeholder: "192.168.1.10" }
                    MeoTextField { id: prefix; width: parent.width; visible: entry.ipMethod === "manual"; label: qsTr("Prefix length"); text: String(entry.modelData.prefix); validator: IntValidator { bottom: 1; top: 32 } }
                    MeoTextField { id: gateway; width: parent.width; visible: entry.ipMethod === "manual"; label: qsTr("Gateway (optional)"); text: entry.modelData.gateway; placeholder: "192.168.1.1" }
                    MeoTextField { id: dns; width: parent.width; label: qsTr("DNS servers (optional)"); text: entry.modelData.dns; helperText: qsTr("Separate addresses with commas; leave empty to use automatic DNS.") }
                    MeoText { width: parent.width; text: qsTr("Address changes take effect when this connection reconnects."); wrapMode: Text.WordWrap }
                    MeoButton {
                        text: qsTr("Save IPv4 configuration"); type: "tonal"; enabled: !NetworkBackend.busy && (entry.ipMethod === "auto" || (address.text.trim() !== "" && prefix.acceptableInput))
                        onClicked: NetworkBackend.configureProfile(entry.modelData.uuid, {"ipv4Method": entry.ipMethod, "address": address.text.trim(), "prefix": Number(prefix.text), "gateway": gateway.text.trim(), "dns": dns.text.trim()})
                    }
                }
            }
        }
        MeoButton { text: qsTr("Advanced connections and VPN import"); type: "text"; onClicked: root.navigateTo("kcm:kcm_networkmanagement") }
    }
}
