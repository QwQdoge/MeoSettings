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
        title: qsTr("Recovery")
        subtitle: qsTr("See what can recover before making a risky change")

        MeoCard {
            width: parent.width
            type: "filled"
            ColumnLayout {
                anchors.fill: parent
                spacing: 8 * MeoTheme.globalScale
                MeoText { Layout.fillWidth: true; text: RecoveryBackend.mode === "btrfs" ? qsTr("Full recovery") : qsTr("Limited recovery"); typeRole: "title"; typeSize: "small"; emphasized: true }
                MeoText { Layout.fillWidth: true; text: RecoveryBackend.summary; wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Recovery protection")
            subtitle: ""
            model: RecoveryBackend.capabilities.map(item => ({
                "title": item.title,
                "subtitle": item.detail,
                "icon": item.available ? "check_circle" : "info",
                "tone": item.available ? "primary" : "neutral",
                "trailingKind": "none"
            }))
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            ColumnLayout {
                anchors.fill: parent
                spacing: 8 * MeoTheme.globalScale
                MeoText { Layout.fillWidth: true; text: qsTr("Transaction protection"); typeRole: "title"; typeSize: "small"; emphasized: true }
                MeoText { Layout.fillWidth: true; text: SystemTransactionBackend.serviceAvailable ? qsTr("The Meo transaction service is available for inspect, preview, authorization, recovery point, apply, validation, and commit.") : qsTr("The privileged Meo transaction service is not installed yet. Settings will not pretend that it can apply or recover a protected change."); wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
            }
        }
    }
}
