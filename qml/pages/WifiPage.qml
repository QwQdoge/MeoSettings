import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property string wifiStatusAccessible: !NetworkBackend.wifiAvailable
                                                ? qsTr("Wi-Fi unavailable")
                                                : (NetworkBackend.wifiEnabled
                                                   ? (NetworkBackend.connected
                                                      ? qsTr("Connected to %1").arg(NetworkBackend.connectionName)
                                                      : qsTr("Wi-Fi on, not connected"))
                                                   : qsTr("Wi-Fi off"))

    function activateNetwork(network) {
        if (network.connected) {
            NetworkBackend.disconnectCurrent()
        } else if (!network.saved && !network.directConnectSupported) {
            // Enterprise/WEP and other uncommon security flows need KDE's
            // established NetworkManager editor rather than a misleading
            // generic password prompt.
            root.navigateTo("kcm:kcm_networkmanagement")
        } else if (network.requiresPassword && !network.saved) {
            passwordPrompt.ssid = network.ssid
            passwordField.text = ""
            saveNetwork.checked = true
            passwordPrompt.open()
        } else {
            NetworkBackend.connectNetwork(network.ssid)
        }
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Wi-Fi")
        subtitle: qsTr("Choose a Wi-Fi network. For a new connection, decide whether to save it for next time.")

        MeoCard {
            width: parent.width
            type: "filled"
            Accessible.role: Accessible.StatusBar
            Accessible.name: root.wifiStatusAccessible

            RowLayout {
                anchors.fill: parent
                spacing: 16 * MeoTheme.globalScale

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2 * MeoTheme.globalScale

                    MeoText {
                        text: qsTr("Wi-Fi")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: !NetworkBackend.wifiAvailable ? qsTr("No wireless adapter found")
                              : (NetworkBackend.wifiEnabled
                                 ? (NetworkBackend.connected ? NetworkBackend.connectionName : qsTr("On, not connected"))
                                 : qsTr("Off"))
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        elide: Text.ElideRight
                    }
                }
                MeoSwitch {
                    id: wifiSwitch
                    checked: false
                    enabled: NetworkBackend.wifiAvailable && !NetworkBackend.busy
                    Accessible.name: qsTr("Turn Wi-Fi on or off")
                    Component.onCompleted: checked = NetworkBackend.wifiEnabled
                    onToggled: (checked) => NetworkBackend.wifiEnabled = checked
                    Connections {
                        target: NetworkBackend
                        function onChanged() { wifiSwitch.checked = NetworkBackend.wifiEnabled }
                        function onErrorChanged() { wifiSwitch.checked = NetworkBackend.wifiEnabled }
                    }
                }
            }
        }

        Column {
            width: parent.width
            visible: NetworkBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Wi-Fi needs attention")
                text: qsTr("Check that Wi-Fi is turned on and you are in range, then refresh the network list.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(NetworkBackend.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        Flow {
            width: parent.width
            spacing: 8 * MeoTheme.globalScale

            MeoButton {
                text: NetworkBackend.scanning ? qsTr("Scanning…") : qsTr("Refresh networks")
                type: "tonal"
                enabled: NetworkBackend.wifiAvailable && NetworkBackend.wifiEnabled && !NetworkBackend.busy && !NetworkBackend.scanning
                onClicked: NetworkBackend.requestScan()
            }
            MeoButton {
                text: qsTr("Advanced network settings")
                type: "text"
                enabled: KcmBridge.isAvailable("kcm_networkmanagement")
                onClicked: root.navigateTo("kcm:kcm_networkmanagement")
            }
        }

        MeoText {
            text: qsTr("Available networks")
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            color: MeoTheme.contentOnSurface
            visible: NetworkBackend.wifiAvailable && NetworkBackend.wifiEnabled
        }

        MeoCard {
            width: parent.width
            type: "elevated"
            visible: NetworkBackend.wifiAvailable && NetworkBackend.wifiEnabled

            Column {
                width: parent.width
                spacing: 0

                Repeater {
                    model: NetworkBackend.networks

                    delegate: MeoListItem {
                        required property var modelData
                        width: parent.width
                        headline: modelData.ssid
                        supportingText: (modelData.connected ? qsTr("Connected")
                                        : (modelData.connecting ? qsTr("Connecting…")
                                           : (modelData.saved ? qsTr("Saved") : modelData.securityLabel)))
                                        + qsTr(" · %1%").arg(modelData.strength)
                        Accessible.name: modelData.ssid
                        Accessible.description: supportingText
                        leadingIcon: modelData.connected ? "wifi" : (modelData.secured ? "wifi_lock" : "wifi")
                        enabled: !NetworkBackend.busy
                        trailingComponent: Component {
                            MeoIconButton {
                                visible: modelData.saved && !modelData.connected
                                icon.name: "delete"
                                type: "standard"
                                size: "s"
                                Accessible.name: qsTr("Forget %1").arg(modelData.ssid)
                                onClicked: {
                                    forgetPrompt.ssid = modelData.ssid
                                    forgetPrompt.open()
                                }
                            }
                        }
                        onClicked: root.activateNetwork(modelData)
                    }
                }

                MeoText {
                    width: parent.width
                    visible: NetworkBackend.networks.length === 0 && !NetworkBackend.scanning
                    text: qsTr("No Wi-Fi networks are currently visible.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: 28 * MeoTheme.globalScale
                    bottomPadding: 28 * MeoTheme.globalScale
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !NetworkBackend.wifiAvailable
            icon: "wifi_off"
            title: qsTr("Wi-Fi is unavailable")
            description: qsTr("Connect a compatible wireless adapter or use advanced network settings for another connection type.")
            actionText: KcmBridge.isAvailable("kcm_networkmanagement") ? qsTr("Open advanced settings") : ""
            onActionClicked: root.navigateTo("kcm:kcm_networkmanagement")
        }

        RepairEntry {
            category: "network"
            entryTitle: qsTr("Troubleshoot network access")
        }
    }

    MeoMotionPopup {
        id: passwordPrompt
        property string ssid: ""
        parent: Overlay.overlay
        presentation: MeoMotionPopup.Dialog
        width: Math.min(parent ? parent.width - 48 * MeoTheme.globalScale : 440 * MeoTheme.globalScale,
                        440 * MeoTheme.globalScale)
        padding: 24 * MeoTheme.globalScale
        x: parent ? Math.max(viewportMargin, (parent.width - width) / 2) : 0
        y: parent ? Math.max(viewportMargin, (parent.height - height) / 2) : 0
        initialFocusItem: passwordField

        contentItem: Column {
            spacing: 16 * MeoTheme.globalScale
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Connect to %1").arg(passwordPrompt.ssid)

            MeoText {
                width: parent.width
                text: qsTr("Connect to %1").arg(passwordPrompt.ssid)
                typeRole: "title"
                typeSize: "small"
                emphasized: true
                color: MeoTheme.contentOnSurface
                wrapMode: Text.WordWrap
            }
            MeoText {
                width: parent.width
                text: qsTr("Save this network to reconnect more easily. Turn it off for a one-time connection that is removed when you disconnect.")
                typeRole: "body"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
            MeoTextField {
                id: passwordField
                width: parent.width
                label: qsTr("Password")
                isPassword: true
                onAccepted: connectButton.clicked()
            }
            MeoCheckbox {
                id: saveNetwork
                width: parent.width
                label: qsTr("Save this network")
                helperText: qsTr("Saved password handling follows your device’s network security settings.")
                checked: true
            }
            Flow {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale

                MeoButton {
                    id: connectButton
                    text: qsTr("Connect")
                    type: "filled"
                    enabled: passwordField.text.length > 0
                    onClicked: {
                        NetworkBackend.connectNetwork(passwordPrompt.ssid, passwordField.text, saveNetwork.checked)
                        passwordField.text = ""
                        passwordPrompt.close()
                    }
                }
                MeoButton {
                    text: qsTr("Cancel")
                    type: "text"
                    onClicked: passwordPrompt.close()
                }
            }
        }
    }

    MeoMotionPopup {
        id: forgetPrompt
        property string ssid: ""
        parent: Overlay.overlay
        presentation: MeoMotionPopup.Dialog
        width: Math.min(parent ? parent.width - 48 * MeoTheme.globalScale : 440 * MeoTheme.globalScale,
                        440 * MeoTheme.globalScale)
        padding: 24 * MeoTheme.globalScale
        x: parent ? Math.max(viewportMargin, (parent.width - width) / 2) : 0
        y: parent ? Math.max(viewportMargin, (parent.height - height) / 2) : 0
        initialFocusItem: forgetNetworkButton

        contentItem: Column {
            spacing: 16 * MeoTheme.globalScale
            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Forget this network?")

            MeoText {
                width: parent.width
                text: qsTr("Forget this network?")
                typeRole: "title"
                typeSize: "small"
                emphasized: true
                color: MeoTheme.contentOnSurface
            }
            MeoText {
                width: parent.width
                text: qsTr("This removes the saved connection and password for %1 from this device.").arg(forgetPrompt.ssid)
                typeRole: "body"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
            Flow {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale
                MeoButton {
                    id: forgetNetworkButton
                    text: qsTr("Forget network")
                    type: "filled"
                    onClicked: {
                        NetworkBackend.forgetNetwork(forgetPrompt.ssid)
                        forgetPrompt.close()
                    }
                }
                MeoButton {
                    text: qsTr("Cancel")
                    type: "text"
                    onClicked: forgetPrompt.close()
                }
            }
        }
    }
}
