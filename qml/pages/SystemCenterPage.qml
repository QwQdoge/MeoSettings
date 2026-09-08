import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Services & logs")
        subtitle: qsTr("Understand background services before changing them")

        MeoCard {
            width: parent.width
            type: "filled"
            ColumnLayout {
                anchors.fill: parent
                spacing: 8 * MeoTheme.globalScale
                MeoText { Layout.fillWidth: true; text: SystemTransactionBackend.serviceAvailable ? qsTr("Meo System Center is connected") : qsTr("Meo System Center needs its privileged service"); typeRole: "title"; typeSize: "small"; emphasized: true }
                MeoText { Layout.fillWidth: true; text: SystemTransactionBackend.serviceAvailable ? qsTr("Normal, important, and critical system units have different safeguards.") : qsTr("Until the service is installed, Meo only exposes its status and will not run a hidden command or write a unit file."); wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
            }
        }
        MeoSettingsGroup {
            width: parent.width
            title: qsTr("How Meo protects services")
            subtitle: ""
            model: [
                {"title": qsTr("Normal"), "subtitle": qsTr("Services such as Bluetooth can be managed with a normal confirmation."), "icon": "check_circle", "tone": "primary", "trailingKind": "none"},
                {"title": qsTr("Important"), "subtitle": qsTr("Network and display services explain the connection or session impact before an action."), "icon": "warning", "tone": "tertiary", "trailingKind": "none"},
                {"title": qsTr("Critical"), "subtitle": qsTr("Login, D-Bus, and core session services are disabled in standard mode."), "icon": "security", "tone": "error", "trailingKind": "none"}
            ]
        }
    }
}
