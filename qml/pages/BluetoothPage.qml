import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property var activeAdapter: {
        for (let index = 0; index < BluetoothBackend.adapters.length; ++index) {
            if (BluetoothBackend.adapters[index].selected)
                return BluetoothBackend.adapters[index]
        }
        return ({})
    }
    readonly property var pairingRequest: BluetoothBackend.authentication
    readonly property string pairingKind: pairingRequest && pairingRequest.kind ? pairingRequest.kind : ""
    readonly property bool pairingNeedsInput: pairingKind === "pin" || pairingKind === "passkey"
    readonly property bool pairingNeedsConfirmation: pairingKind === "confirmation"
                                                 || pairingKind === "authorization"
                                                 || pairingKind === "service-authorization"

    function deviceGroup(device) {
        if (device.blocked)
            return qsTr("Blocked devices")
        if (device.connected)
            return qsTr("Connected")
        if (device.paired)
            return qsTr("Saved devices")
        return qsTr("Nearby devices")
    }

    function deviceStatus(device) {
        if (device.blocked)
            return qsTr("Blocked")
        if (device.pairing)
            return qsTr("Pairing…")
        if (device.connected)
            return qsTr("Connected")
        if (device.paired)
            return device.trusted ? qsTr("Paired · Trusted") : qsTr("Paired")
        return qsTr("Available to pair")
    }

    function activateDevice(device) {
        if (device.blocked) {
            deviceDetails.device = device
            deviceDetails.open()
        } else if (device.connected) {
            BluetoothBackend.disconnectDevice(device.ubi)
        } else if (device.paired) {
            BluetoothBackend.connectDevice(device.ubi)
        } else {
            BluetoothBackend.pairDevice(device.ubi)
        }
    }

    function primaryActionText(device) {
        if (device.blocked)
            return qsTr("Manage")
        if (device.pairing)
            return qsTr("Pairing…")
        if (device.connected)
            return qsTr("Disconnect")
        if (device.paired)
            return qsTr("Connect")
        return qsTr("Pair")
    }

    function pairingTitle() {
        switch (pairingKind) {
        case "pin": return qsTr("Enter Bluetooth PIN")
        case "passkey": return qsTr("Enter passkey")
        case "confirmation": return qsTr("Confirm pairing code")
        case "authorization": return qsTr("Allow Bluetooth pairing")
        case "service-authorization": return qsTr("Allow Bluetooth service")
        case "display-pin": return qsTr("Bluetooth PIN")
        case "display-passkey": return qsTr("Bluetooth passkey")
        default: return qsTr("Pairing %1").arg(BluetoothBackend.pairingDeviceName || qsTr("device"))
        }
    }

    function pairingSubtitle() {
        const deviceName = pairingRequest && pairingRequest.deviceName
                           ? pairingRequest.deviceName
                           : BluetoothBackend.pairingDeviceName
        switch (pairingKind) {
        case "pin": return qsTr("Enter the PIN requested by %1. The PIN is sent only to BlueZ for this pairing.").arg(deviceName)
        case "passkey": return qsTr("Enter the numeric passkey requested by %1.").arg(deviceName)
        case "confirmation": return qsTr("Make sure this code is also shown on %1 before confirming.").arg(deviceName)
        case "authorization": return qsTr("%1 is requesting authorization to pair. Only continue if this is the device you chose.").arg(deviceName)
        case "service-authorization": return qsTr("%1 is requesting access to a Bluetooth service. Review the service identifier before allowing it.").arg(deviceName)
        case "display-pin": return qsTr("Enter this PIN on %1. Pairing continues automatically after the device accepts it.").arg(deviceName)
        case "display-passkey": return qsTr("Enter this passkey on %1. Pairing continues automatically after the device accepts it.").arg(deviceName)
        default: return qsTr("Preparing a private BlueZ authentication channel. This page never becomes the system default pairing agent.")
        }
    }

    function pairingAcceptText() {
        if (pairingNeedsInput)
            return qsTr("Continue")
        if (pairingNeedsConfirmation || pairingKind === "service-authorization")
            return qsTr("Allow")
        return ""
    }

    function submitPairingResponse() {
        if (pairingKind === "pin") {
            BluetoothBackend.submitPin(pairingTask.input)
            pairingTask.input = ""
        } else if (pairingKind === "passkey") {
            BluetoothBackend.submitPasskey(pairingTask.input)
            pairingTask.input = ""
        } else if (pairingNeedsConfirmation) {
            BluetoothBackend.respondToAuthentication(true)
        }
    }

    function syncPairingTask() {
        if (BluetoothBackend.pairing) {
            pairingTask.open()
        } else if (pairingTask.isOpen) {
            pairingTask.close()
            pairingTask.input = ""
        }
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Bluetooth")
        subtitle: qsTr("Scan, pair, and manage Bluetooth devices through BlueZ. Every PIN, passkey, code comparison, and authorization is confirmed in Meo Settings.")

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
                        text: qsTr("Bluetooth")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.contentOnSurface
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: !BluetoothBackend.available ? qsTr("No adapter found")
                              : (BluetoothBackend.rfkillBlocked ? qsTr("Blocked by hardware or rfkill")
                                 : (BluetoothBackend.enabled
                                    ? qsTr("On · %1").arg(root.activeAdapter.name || qsTr("Default adapter"))
                                    : qsTr("Off")))
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        elide: Text.ElideRight
                    }
                }

                MeoSwitch {
                    checked: BluetoothBackend.enabled
                    enabled: BluetoothBackend.available && !BluetoothBackend.busy && !BluetoothBackend.pairing
                    Accessible.name: qsTr("Turn Bluetooth on or off")
                    onToggled: BluetoothBackend.enabled = checked
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: BluetoothBackend.rfkillBlocked

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "block"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("Bluetooth is blocked by rfkill or a hardware switch. Turn it back on from the device hardware controls before pairing.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: BluetoothBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: BluetoothBackend.error
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
                text: BluetoothBackend.discovering ? qsTr("Stop scan") : qsTr("Scan for devices")
                type: "tonal"
                enabled: BluetoothBackend.enabled && !BluetoothBackend.busy && !BluetoothBackend.pairing
                onClicked: {
                    if (BluetoothBackend.discovering)
                        BluetoothBackend.stopDiscovery()
                    else
                        BluetoothBackend.startDiscovery()
                }
            }
            MeoButton {
                text: qsTr("Adapter options")
                type: "text"
                enabled: BluetoothBackend.available && !BluetoothBackend.pairing
                onClicked: adapterOptions.open()
            }
            MeoButton {
                text: qsTr("Advanced Bluetooth compatibility")
                type: "text"
                visible: KcmBridge.isAvailable("kcm_bluetooth")
                enabled: !BluetoothBackend.pairing
                onClicked: root.navigateTo("kcm:kcm_bluetooth")
            }
        }

        MeoText {
            text: qsTr("Devices")
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            color: MeoTheme.contentOnSurface
            visible: BluetoothBackend.available && BluetoothBackend.enabled
        }

        Column {
            width: parent.width
            spacing: 8 * MeoTheme.globalScale
            visible: BluetoothBackend.available && BluetoothBackend.enabled && BluetoothBackend.devices.length > 0

            Repeater {
                model: BluetoothBackend.devices

                delegate: Column {
                    required property var modelData
                    required property int index
                    width: parent.width
                    spacing: 8 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        visible: index === 0 || root.deviceGroup(modelData) !== root.deviceGroup(BluetoothBackend.devices[index - 1])
                        text: root.deviceGroup(modelData)
                        typeRole: "label"
                        typeSize: "medium"
                        emphasized: true
                        color: MeoTheme.contentOnSurfaceVariant
                        topPadding: index === 0 ? 0 : 10 * MeoTheme.globalScale
                    }

                    MeoCard {
                        width: parent.width
                        type: modelData.connected ? "filled" : "elevated"
                        interactive: false

                        Column {
                            width: parent.width
                            spacing: 12 * MeoTheme.globalScale

                            RowLayout {
                                width: parent.width
                                spacing: 12 * MeoTheme.globalScale

                                MeoIcon {
                                    icon: modelData.icon
                                    size: 28
                                    color: modelData.blocked ? MeoTheme.error : MeoTheme.primary
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2 * MeoTheme.globalScale
                                    MeoText {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        typeRole: "title"
                                        typeSize: "small"
                                        emphasized: true
                                        color: MeoTheme.contentOnSurface
                                        elide: Text.ElideRight
                                    }
                                    MeoText {
                                        Layout.fillWidth: true
                                        text: root.deviceStatus(modelData)
                                              + (modelData.batteryAvailable
                                                 ? qsTr(" · %1% battery").arg(modelData.batteryPercent)
                                                 : "")
                                        typeRole: "body"
                                        typeSize: "small"
                                        color: MeoTheme.contentOnSurfaceVariant
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Flow {
                                width: parent.width
                                spacing: 8 * MeoTheme.globalScale

                                MeoButton {
                                    text: root.primaryActionText(modelData)
                                    type: modelData.connected ? "tonal" : "filled"
                                    enabled: !BluetoothBackend.busy && !BluetoothBackend.pairing && !modelData.pairing
                                    onClicked: root.activateDevice(modelData)
                                }
                                MeoButton {
                                    text: qsTr("Details")
                                    type: "text"
                                    enabled: !BluetoothBackend.pairing
                                    onClicked: {
                                        deviceDetails.device = modelData
                                        deviceDetails.open()
                                    }
                                }
                                MeoButton {
                                    visible: modelData.paired
                                    text: qsTr("Forget")
                                    type: "text"
                                    enabled: !BluetoothBackend.busy && !BluetoothBackend.pairing
                                    onClicked: {
                                        forgetPrompt.device = modelData
                                        forgetPrompt.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        MeoText {
            width: parent.width
            visible: BluetoothBackend.available && BluetoothBackend.enabled && BluetoothBackend.devices.length === 0
            text: BluetoothBackend.discovering ? qsTr("Looking for devices…") : qsTr("No Bluetooth devices are visible yet. Put a device in pairing mode, then scan.")
            typeRole: "body"
            typeSize: "medium"
            color: MeoTheme.contentOnSurfaceVariant
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            topPadding: 28 * MeoTheme.globalScale
            bottomPadding: 28 * MeoTheme.globalScale
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !BluetoothBackend.available
            icon: "bluetooth_disabled"
            title: qsTr("Bluetooth is unavailable")
            description: qsTr("No Bluetooth adapter is currently available through the system BlueZ service.")
            actionText: KcmBridge.isAvailable("kcm_bluetooth") ? qsTr("Open advanced settings") : ""
            onActionClicked: root.navigateTo("kcm:kcm_bluetooth")
        }
    }

    MeoSettingsTaskSheet {
        id: pairingTask
        property string input: ""
        popupParent: Overlay.overlay
        title: root.pairingTitle()
        subtitle: root.pairingSubtitle()
        showCloseButton: false
        dismissible: false
        acceptText: root.pairingAcceptText()
        rejectText: qsTr("Cancel pairing")
        acceptEnabled: !root.pairingNeedsInput || input.length > 0
        closeOnAccept: false
        closeOnReject: false
        onAccepted: root.submitPairingResponse()
        onRejected: {
            input = ""
            BluetoothBackend.cancelPairing()
        }

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: pairingContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: pairingContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 16 * MeoTheme.globalScale

                    MeoCard {
                        width: parent.width
                        type: "outlined"
                        visible: root.pairingKind !== ""

                        Column {
                            width: parent.width
                            spacing: 6 * MeoTheme.globalScale
                            MeoText {
                                width: parent.width
                                text: root.pairingRequest.deviceName || BluetoothBackend.pairingDeviceName
                                typeRole: "title"
                                typeSize: "small"
                                emphasized: true
                                color: MeoTheme.contentOnSurface
                                wrapMode: Text.WordWrap
                            }
                            MeoText {
                                width: parent.width
                                text: root.pairingRequest.deviceAddress || ""
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MeoLoadingIndicator {
                        visible: root.pairingKind === ""
                        indeterminate: true
                        width: 32 * MeoTheme.globalScale
                        height: width
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    MeoText {
                        width: parent.width
                        visible: root.pairingKind === "display-pin" || root.pairingKind === "display-passkey"
                        text: root.pairingRequest.code || ""
                        typeRole: "display"
                        typeSize: "small"
                        emphasized: true
                        color: MeoTheme.primary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WrapAnywhere
                    }

                    MeoText {
                        width: parent.width
                        visible: root.pairingKind === "display-passkey" && root.pairingRequest.entered !== ""
                        text: qsTr("Entered on device: %1 of 6 digits").arg(root.pairingRequest.entered)
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        horizontalAlignment: Text.AlignHCenter
                    }

                    MeoText {
                        width: parent.width
                        visible: root.pairingNeedsConfirmation
                        text: root.pairingKind === "confirmation" ? (root.pairingRequest.code || "")
                              : (root.pairingKind === "service-authorization"
                                 ? (root.pairingRequest.serviceUuid || "") : "")
                        typeRole: "title"
                        typeSize: "medium"
                        emphasized: true
                        color: MeoTheme.primary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WrapAnywhere
                    }

                    MeoTextField {
                        width: parent.width
                        visible: root.pairingNeedsInput
                        label: root.pairingKind === "pin" ? qsTr("PIN") : qsTr("Passkey")
                        helperText: root.pairingKind === "pin"
                                    ? qsTr("1–16 letters or digits")
                                    : qsTr("A number from 0 to 999999")
                        isPassword: true
                        inputMethodHints: root.pairingKind === "passkey" ? Qt.ImhDigitsOnly : Qt.ImhNoPredictiveText
                        maximumLength: root.pairingKind === "pin" ? 16 : 6
                        text: pairingTask.input
                        onTextChanged: pairingTask.input = text
                        onAccepted: pairingTask.accept()
                    }

                    MeoText {
                        width: parent.width
                        visible: root.pairingKind === ""
                        text: qsTr("Waiting for the device to request verification. You can cancel safely at any time.")
                        typeRole: "body"
                        typeSize: "medium"
                        color: MeoTheme.contentOnSurfaceVariant
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    MeoText {
                        width: parent.width
                        visible: root.pairingKind !== ""
                        text: qsTr("Cancel if the device name, code, or requested authorization is not what you expected.")
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: deviceDetails
        property var device: ({})
        popupParent: Overlay.overlay
        title: device.name || qsTr("Bluetooth device")
        subtitle: qsTr("Manage one device explicitly. Trust, block, rename, connection, and removal never happen automatically after pairing.")
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: detailContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: detailContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 16 * MeoTheme.globalScale

                    MeoCard {
                        width: parent.width
                        type: "outlined"

                        Column {
                            width: parent.width
                            spacing: 6 * MeoTheme.globalScale
                            MeoText { width: parent.width; text: deviceDetails.device.address || ""; typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
                            MeoText { width: parent.width; text: deviceDetails.device.type || qsTr("Bluetooth device"); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
                            MeoText {
                                width: parent.width
                                visible: deviceDetails.device.rssi !== undefined && deviceDetails.device.rssi < 0
                                text: qsTr("Signal: %1 dBm").arg(deviceDetails.device.rssi)
                                typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                            }
                            MeoText {
                                width: parent.width
                                text: deviceDetails.device.legacyPairing ? qsTr("Uses legacy pairing")
                                      : (deviceDetails.device.servicesResolved ? qsTr("Services resolved") : qsTr("Services resolve after connection"))
                                typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                            }
                        }
                    }

                    MeoTextField {
                        id: renameField
                        width: parent.width
                        label: qsTr("Device name")
                        helperText: qsTr("This changes the local BlueZ alias, not the device hardware name.")
                        text: deviceDetails.device.name || ""
                    }
                    MeoButton {
                        text: qsTr("Save name")
                        type: "tonal"
                        enabled: renameField.text.trim().length > 0 && !BluetoothBackend.busy
                        onClicked: BluetoothBackend.renameDevice(deviceDetails.device.ubi, renameField.text)
                    }

                    MeoCard {
                        width: parent.width
                        type: "filled"

                        Column {
                            width: parent.width
                            spacing: 8 * MeoTheme.globalScale
                            MeoText {
                                width: parent.width
                                text: qsTr("Trust")
                                typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface
                            }
                            MeoText {
                                width: parent.width
                                text: deviceDetails.device.trusted
                                      ? qsTr("Trusted devices may reconnect without another approval.")
                                      : qsTr("This device needs an explicit connection each time.")
                                typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                            }
                            MeoButton {
                                visible: !!deviceDetails.device.paired
                                text: deviceDetails.device.trusted ? qsTr("Remove trust") : qsTr("Trust device")
                                type: "text"
                                enabled: !BluetoothBackend.busy
                                onClicked: BluetoothBackend.setDeviceTrusted(deviceDetails.device.ubi, !deviceDetails.device.trusted)
                            }
                        }
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"

                        Column {
                            width: parent.width
                            spacing: 8 * MeoTheme.globalScale
                            MeoText {
                                width: parent.width
                                text: qsTr("Block")
                                typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface
                            }
                            MeoText {
                                width: parent.width
                                text: deviceDetails.device.blocked
                                      ? qsTr("This device cannot connect or pair until it is unblocked.")
                                      : qsTr("Blocking prevents this device from connecting or pairing.")
                                typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                            }
                            MeoButton {
                                text: deviceDetails.device.blocked ? qsTr("Unblock device") : qsTr("Block device")
                                type: "text"
                                enabled: !BluetoothBackend.busy
                                onClicked: BluetoothBackend.setDeviceBlocked(deviceDetails.device.ubi, !deviceDetails.device.blocked)
                            }
                        }
                    }

                    MeoButton {
                        visible: !!deviceDetails.device.paired
                        text: qsTr("Forget pairing")
                        type: "text"
                        enabled: !BluetoothBackend.busy
                        onClicked: {
                            forgetPrompt.device = deviceDetails.device
                            deviceDetails.close()
                            forgetPrompt.open()
                        }
                    }
                }
            }
        }
    }

    MeoSettingsTaskSheet {
        id: adapterOptions
        popupParent: Overlay.overlay
        title: qsTr("Bluetooth adapter")
        subtitle: qsTr("Choose the adapter used for scanning. Visibility and pairability affect how other devices can find this computer.")
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: adapterContent.implicitHeight + 32 * MeoTheme.globalScale

                Column {
                    id: adapterContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    spacing: 12 * MeoTheme.globalScale

                    Repeater {
                        model: BluetoothBackend.adapters
                        delegate: MeoButton {
                            required property var modelData
                            width: parent.width
                            text: modelData.name + (modelData.selected ? qsTr(" · Selected") : "")
                            type: modelData.selected ? "filled" : "tonal"
                            enabled: !BluetoothBackend.busy
                            onClicked: BluetoothBackend.activeAdapterUbi = modelData.ubi
                        }
                    }

                    MeoCard {
                        width: parent.width
                        type: "outlined"
                        visible: root.activeAdapter.ubi !== undefined

                        Column {
                            width: parent.width
                            spacing: 12 * MeoTheme.globalScale

                            RowLayout {
                                width: parent.width
                                MeoText { Layout.fillWidth: true; text: qsTr("Discoverable"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                                MeoSwitch {
                                    checked: root.activeAdapter.discoverable || false
                                    enabled: !!root.activeAdapter.powered && !BluetoothBackend.busy
                                    onToggled: BluetoothBackend.setAdapterDiscoverable(root.activeAdapter.ubi, checked)
                                }
                            }
                            MeoText { width: parent.width; text: qsTr("Other devices can see this computer for %1 seconds (0 means no timeout).").arg(root.activeAdapter.discoverableTimeout || 0); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                            RowLayout {
                                width: parent.width
                                MeoText { Layout.fillWidth: true; text: qsTr("Pairable"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                                MeoSwitch {
                                    checked: root.activeAdapter.pairable || false
                                    enabled: !!root.activeAdapter.powered && !BluetoothBackend.busy
                                    onToggled: BluetoothBackend.setAdapterPairable(root.activeAdapter.ubi, checked)
                                }
                            }
                            MeoText { width: parent.width; text: qsTr("Other devices may request pairing for %1 seconds (0 means no timeout). Meo still asks before accepting.").arg(root.activeAdapter.pairableTimeout || 0); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                            Flow {
                                width: parent.width
                                spacing: 8 * MeoTheme.globalScale
                                MeoButton { text: qsTr("Visible for 5 minutes"); type: "text"; enabled: !!root.activeAdapter.powered && !BluetoothBackend.busy; onClicked: BluetoothBackend.setAdapterDiscoverableTimeout(root.activeAdapter.ubi, 300) }
                                MeoButton { text: qsTr("Pairable for 5 minutes"); type: "text"; enabled: !!root.activeAdapter.powered && !BluetoothBackend.busy; onClicked: BluetoothBackend.setAdapterPairableTimeout(root.activeAdapter.ubi, 300) }
                            }
                        }
                    }
                }
            }
        }
    }

    MeoMotionPopup {
        id: forgetPrompt
        property var device: ({})
        parent: Overlay.overlay
        presentation: MeoMotionPopup.Dialog
        width: Math.min(parent ? parent.width - 48 * MeoTheme.globalScale : 420 * MeoTheme.globalScale,
                        420 * MeoTheme.globalScale)
        padding: 24 * MeoTheme.globalScale
        x: parent ? Math.max(viewportMargin, (parent.width - width) / 2) : 0
        y: parent ? Math.max(viewportMargin, (parent.height - height) / 2) : 0

        contentItem: Column {
            spacing: 16 * MeoTheme.globalScale
            MeoText {
                width: parent.width
                text: qsTr("Forget %1?").arg(forgetPrompt.device.name || qsTr("device"))
                typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface; wrapMode: Text.WordWrap
            }
            MeoText {
                width: parent.width
                text: qsTr("This removes the pairing from its Bluetooth adapter. You will need to pair the device again before reconnecting.")
                typeRole: "body"; typeSize: "medium"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
            }
            Flow {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale
                MeoButton {
                    text: qsTr("Forget device")
                    type: "filled"
                    onClicked: {
                        BluetoothBackend.unpairDevice(forgetPrompt.device.ubi)
                        forgetPrompt.close()
                    }
                }
                MeoButton { text: qsTr("Cancel"); type: "text"; onClicked: forgetPrompt.close() }
            }
        }
    }

    Connections {
        target: BluetoothBackend
        function onPairingChanged() { root.syncPairingTask() }
        function onAuthenticationChanged() { root.syncPairingTask() }
    }

    Component.onCompleted: syncPairingTask()
    Component.onDestruction: {
        if (BluetoothBackend.pairing)
            BluetoothBackend.cancelPairing()
    }
}
