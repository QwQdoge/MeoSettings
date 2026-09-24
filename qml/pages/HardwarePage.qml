import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property var fingerprintPackage: {
        // Keep the binding reactive while the backend retains ownership of the
        // bounded support metadata.
        const present = FingerprintBackend.devicePresent
        return FingerprintBackend.supportPackage()
    }

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

        Flow {
            width: parent.width
            spacing: MeoTheme.space8

            MeoButton {
                text: qsTr("Refresh hardware")
                icon.name: "refresh"
                type: "tonal"
                onClicked: {
                    HardwareCapabilities.refresh()
                    FingerprintBackend.refresh()
                }
            }
        }

        MeoCard {
            width: parent.width
            type: FingerprintBackend.devicePresent ? "filled" : "outlined"

            ColumnLayout {
                anchors.fill: parent
                spacing: MeoTheme.space8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space12

                    MeoIcon {
                        icon: "fingerprint"
                        size: 28
                        color: FingerprintBackend.devicePresent
                               ? MeoTheme.tertiary
                               : MeoTheme.contentOnSurfaceVariant
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space2

                        MeoText {
                            Layout.fillWidth: true
                            text: qsTr("Fingerprint")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }

                        MeoText {
                            Layout.fillWidth: true
                            text: FingerprintBackend.deviceLabel
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }

                    MeoBadge {
                        text: root.supportText(FingerprintBackend.supportLevel)
                        color: FingerprintBackend.supportLevel === "supported"
                               ? MeoTheme.primary : MeoTheme.tertiary
                    }
                }

                MeoText {
                    Layout.fillWidth: true
                    text: FingerprintBackend.supportSummary
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoSettingsGroup {
                    Layout.fillWidth: true
                    visible: FingerprintBackend.devicePresent
                    title: ""
                    subtitle: ""
                    model: [
                        {
                            "title": qsTr("Support component"),
                            "subtitle": String(root.fingerprintPackage.provider || ""),
                            "icon": "extension",
                            "tone": "tertiary",
                            "trailingKind": "status",
                            "trailingText": root.fingerprintPackage.requiresConsent ? qsTr("Consent required") : qsTr("Available"),
                            "interactive": false
                        },
                        {
                            "title": qsTr("Password fallback"),
                            "subtitle": qsTr("Password sign-in remains available even when fingerprint unlock is enabled"),
                            "icon": "password",
                            "tone": "primary",
                            "trailingKind": "status",
                            "trailingText": root.fingerprintPackage.passwordFallbackRequired ? qsTr("Required") : qsTr("Optional"),
                            "interactive": false
                        },
                        {
                            "title": qsTr("Authentication scope"),
                            "subtitle": qsTr("Experimental support is limited to the lock screen; login, Polkit, and sudo remain disabled"),
                            "icon": "lock",
                            "tone": "secondary",
                            "trailingKind": "status",
                            "trailingText": qsTr("Lock screen"),
                            "interactive": false
                        }
                    ]
                }

                MeoText {
                    Layout.fillWidth: true
                    visible: FingerprintBackend.devicePresent
                    text: String(root.fingerprintPackage.privacy || "")
                    typeRole: "label"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
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
                }
            }
        }

        RepairEntry {
            category: "graphics"
            entryTitle: qsTr("Troubleshoot graphics and drivers")
        }
    }
}
