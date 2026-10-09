import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property int selectedMode: Number(ProxyBackend.configuration.mode)
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics; title: qsTr("Proxy")
        subtitle: qsTr("Configure proxy access for KDE applications using KIO. Applications with their own proxy settings keep those settings.")
        MeoBanner { width: parent.width; visible: ProxyBackend.error !== ""; title: qsTr("Proxy configuration needs attention"); text: ProxyBackend.error; tone: "error" }
        MeoExposedDropdown { width: parent.width; label: qsTr("Proxy mode"); model: [qsTr("No proxy"), qsTr("Manual"), qsTr("Automatic script"), qsTr("Auto discover"), qsTr("Environment variables")]; text: model[root.selectedMode] || ""; onSelected: (index, value) => root.selectedMode = index }
        MeoTextField { id: http; width: parent.width; visible: root.selectedMode === 1 || root.selectedMode === 4; label: qsTr("HTTP proxy"); text: ProxyBackend.configuration.httpProxy; placeholder: root.selectedMode === 4 ? "http_proxy" : "http://127.0.0.1:8080" }
        MeoTextField { id: https; width: parent.width; visible: http.visible; label: qsTr("HTTPS proxy"); text: ProxyBackend.configuration.httpsProxy; placeholder: root.selectedMode === 4 ? "https_proxy" : "http://127.0.0.1:8080" }
        MeoTextField { id: ftp; width: parent.width; visible: http.visible; label: qsTr("FTP proxy"); text: ProxyBackend.configuration.ftpProxy }
        MeoTextField { id: socks; width: parent.width; visible: http.visible; label: qsTr("SOCKS proxy"); text: ProxyBackend.configuration.socksProxy; placeholder: root.selectedMode === 4 ? "all_proxy" : "socks://127.0.0.1:1080" }
        MeoTextField { id: pac; width: parent.width; visible: root.selectedMode === 2; label: qsTr("Automatic script URL"); text: ProxyBackend.configuration.pac }
        MeoTextField { id: exceptions; width: parent.width; visible: http.visible; label: qsTr("Proxy exceptions"); text: ProxyBackend.configuration.exceptions }
        MeoSettingsGroup { width: parent.width; visible: http.visible; model: [{"title": qsTr("Use proxy only for the exceptions"), "trailingKind": "toggle", "checked": exceptionMode.reverse}]; onRowToggled: (index, checked, row) => reverse = checked; property bool reverse: Boolean(ProxyBackend.configuration.reverse); id: exceptionMode }
        MeoButton { text: qsTr("Apply proxy configuration"); onClicked: ProxyBackend.apply({"mode": root.selectedMode, "httpProxy": http.text, "httpsProxy": https.text, "ftpProxy": ftp.text, "socksProxy": socks.text, "pac": pac.text, "exceptions": exceptions.text, "reverse": exceptionMode.reverse}) }
        MeoButton { text: qsTr("Advanced proxy settings"); type: "text"; onClicked: root.navigateTo("kcm:kcm_proxy") }
    }
}
