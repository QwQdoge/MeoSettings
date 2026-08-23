import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

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
        subtitle: qsTr("Connect using NetworkManager. Choose whether a new network is saved to disk or is only kept until it disconnects.")

        MeoCard {
            width: parent.width
            type: "filled"

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

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: NetworkBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: NetworkBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
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
                        leadingIcon: modelData.connected ? "wifi" : (modelData.secured ? "wifi_lock" : "wifi")
                        enabled: !NetworkBackend.busy
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
                text: qsTr("A saved network creates a NetworkManager profile on disk. Turn this off for a one-time connection that NetworkManager removes when it disconnects.")
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
                helperText: qsTr("Saved credentials remain under NetworkManager’s system policy.")
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
}
