import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null

    function supportText(level) {
        if (level === "supported") return qsTr("Ready")
        if (level === "experimental") return qsTr("Experimental")
        return qsTr("Unavailable")
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Hardware & drivers")
        subtitle: qsTr("Evidence-based support, without hard-coding this computer")

        MeoCard {
            width: parent.width
            type: "filled"
            ColumnLayout {
                anchors.fill: parent
                spacing: 8 * MeoTheme.globalScale
                MeoText { Layout.fillWidth: true; text: HardwareCapabilities.summary; typeRole: "title"; typeSize: "small"; emphasized: true }
                MeoText { Layout.fillWidth: true; text: qsTr("Meo detects capabilities from the hardware and supported providers. It does not identify compatibility from your laptop model alone."); wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
            }
        }

        Repeater {
            model: HardwareCapabilities.capabilities
            delegate: MeoCard {
                required property var modelData
                width: parent.width
                type: "outlined"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8 * MeoTheme.globalScale
                    RowLayout {
                        Layout.fillWidth: true
                        MeoText { Layout.fillWidth: true; text: modelData.title; typeRole: "title"; typeSize: "small"; emphasized: true }
                        MeoBadge { text: root.supportText(modelData.supportLevel); color: modelData.supportLevel === "supported" ? MeoTheme.primary : MeoTheme.tertiary }
                    }
                    MeoText { Layout.fillWidth: true; text: qsTr("Provider: %1").arg(modelData.provider); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
                    MeoText { Layout.fillWidth: true; text: modelData.evidence; wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small" }
                    MeoText { Layout.fillWidth: true; text: modelData.risk; wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
                    MeoButton {
                        visible: modelData.id === "fingerprint.fpc-10a5-9800"
                        text: qsTr("Review fingerprint support")
                        type: "outlined"
                        onClicked: root.navigateTo("recovery")
                    }
                }
            }
        }
    }
}
