import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics; title: qsTr("Mobile hotspot")
        subtitle: qsTr("Sharing uses the selected Wi-Fi adapter and may disconnect its existing Wi-Fi connection. The hotspot stops when you turn off sharing.")
        MeoBanner { width: parent.width; visible: NetworkBackend.error !== ""; title: qsTr("Hotspot needs attention"); text: NetworkBackend.error; tone: "error" }
        MeoEmptyState { width: parent.width; visible: !NetworkBackend.hotspotSupported; icon: "wifi"; title: qsTr("Hotspot unavailable"); description: qsTr("A Wi-Fi adapter supporting WPA2 access point mode is required.") }
        MeoTextField { id: ssid; width: parent.width; label: qsTr("Network name"); enabled: NetworkBackend.hotspotSupported && !NetworkBackend.hotspotActive && !NetworkBackend.busy }
        MeoTextField { id: password; width: parent.width; label: qsTr("Password"); echoMode: TextInput.Password; enabled: ssid.enabled }
        MeoButton { text: NetworkBackend.hotspotActive ? qsTr("Stop sharing") : qsTr("Start sharing"); enabled: !NetworkBackend.busy && NetworkBackend.hotspotSupported && (NetworkBackend.hotspotActive || (ssid.text !== "" && password.text.length >= 8)); onClicked: { if (NetworkBackend.hotspotActive) NetworkBackend.stopHotspot(); else NetworkBackend.startHotspot(ssid.text, password.text) } }
        MeoButton { text: qsTr("Advanced sharing"); type: "text"; onClicked: root.navigateTo("kcm:kcm_mobile_hotspot") }
    }
}
