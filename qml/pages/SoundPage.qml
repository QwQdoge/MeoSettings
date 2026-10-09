import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    Component.onDestruction: AudioBackend.stopTestSound()
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Sound")
        subtitle: qsTr("Choose where audio plays and which microphone to use.")

        Column {
            width: parent.width
            visible: AudioBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Sound needs attention")
                text: qsTr("Check that your audio device is connected, then refresh or choose an output.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(AudioBackend.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoText {
            text: qsTr("Output")
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            color: MeoTheme.contentOnSurface
            visible: AudioBackend.available
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: AudioBackend.available
            Accessible.role: Accessible.StatusBar
            Accessible.name: AudioBackend.outputMuted
                             ? qsTr("Output is muted")
                             : qsTr("Current output: %1").arg(AudioBackend.outputName)

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: AudioBackend.outputName
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                    elide: Text.ElideRight
                }
                RowLayout {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon {
                        icon: AudioBackend.outputMuted ? "volume_off" : "volume_up"
                        size: 24
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSlider {
                        id: outputVolumeSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 150
                        value: 0
                        enabled: !AudioBackend.outputMuted
                        Component.onCompleted: value = AudioBackend.outputVolume
                        onMoved: (currentValue) => AudioBackend.outputVolume = Math.round(currentValue)
                        Accessible.name: qsTr("Output volume")
                        Accessible.description: qsTr("%1 percent").arg(Math.round(outputVolumeSlider.value))
                        Connections {
                            target: AudioBackend
                            function onChanged() { outputVolumeSlider.value = AudioBackend.outputVolume }
                        }
                    }
                    MeoText {
                        text: AudioBackend.outputVolume + "%"
                        typeRole: "label"
                        typeSize: "medium"
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSwitch {
                        id: outputMuteSwitch
                        checked: false
                        Accessible.name: qsTr("Mute output")
                        Component.onCompleted: checked = !AudioBackend.outputMuted
                        onToggled: (enabled) => AudioBackend.outputMuted = !enabled
                        Connections {
                            target: AudioBackend
                            function onChanged() { outputMuteSwitch.checked = !AudioBackend.outputMuted }
                        }
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "elevated"
            visible: AudioBackend.available

            Column {
                width: parent.width
                spacing: 0
                Repeater {
                    model: AudioBackend.outputs
                    delegate: MeoListItem {
                        required property var modelData
                        width: parent.width
                        headline: modelData.name
                        supportingText: modelData.active ? qsTr("Current output") : modelData.formFactor
                        leadingIcon: modelData.formFactor === "headphones" || modelData.formFactor === "headset" ? "headphones" : "speaker"
                        selected: modelData.active
                        onClicked: AudioBackend.setDefaultOutput(modelData.id)
                    }
                }
            }
        }

        MeoText {
            text: qsTr("Input")
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            color: MeoTheme.contentOnSurface
            visible: AudioBackend.microphoneAvailable
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: AudioBackend.microphoneAvailable
            Accessible.role: Accessible.StatusBar
            Accessible.name: AudioBackend.inputMuted
                             ? qsTr("Microphone is muted")
                             : qsTr("Current input: %1").arg(AudioBackend.inputName)

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: AudioBackend.inputName
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                    elide: Text.ElideRight
                }
                RowLayout {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon {
                        icon: AudioBackend.inputMuted ? "mic_off" : "mic"
                        size: 24
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSlider {
                        id: inputVolumeSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 150
                        value: 0
                        enabled: !AudioBackend.inputMuted
                        Component.onCompleted: value = AudioBackend.inputVolume
                        onMoved: (currentValue) => AudioBackend.inputVolume = Math.round(currentValue)
                        Accessible.name: qsTr("Microphone volume")
                        Accessible.description: qsTr("%1 percent").arg(Math.round(inputVolumeSlider.value))
                        Connections {
                            target: AudioBackend
                            function onChanged() { inputVolumeSlider.value = AudioBackend.inputVolume }
                        }
                    }
                    MeoText {
                        text: AudioBackend.inputVolume + "%"
                        typeRole: "label"
                        typeSize: "medium"
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSwitch {
                        id: inputMuteSwitch
                        checked: false
                        Accessible.name: qsTr("Mute microphone")
                        Component.onCompleted: checked = !AudioBackend.inputMuted
                        onToggled: (enabled) => AudioBackend.inputMuted = !enabled
                        Connections {
                            target: AudioBackend
                            function onChanged() { inputMuteSwitch.checked = !AudioBackend.inputMuted }
                        }
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "elevated"
            visible: AudioBackend.microphoneAvailable

            Column {
                width: parent.width
                spacing: 0
                Repeater {
                    model: AudioBackend.inputs
                    delegate: MeoListItem {
                        required property var modelData
                        width: parent.width
                        headline: modelData.name
                        supportingText: modelData.active ? qsTr("Current input") : modelData.formFactor
                        leadingIcon: "mic"
                        selected: modelData.active
                        onClicked: AudioBackend.setDefaultInput(modelData.id)
                    }
                }
            }
        }

        MeoButton {
            text: AudioBackend.testingSound ? qsTr("Stop test sound") : qsTr("Test default output")
            enabled: AudioBackend.available; type: "tonal"
            onClicked: AudioBackend.testingSound ? AudioBackend.stopTestSound() : AudioBackend.testSound()
        }
        Column {
            width: parent.width; spacing: MeoTheme.space8
            visible: Object.keys(AudioBackend.notificationSound).length > 0
            MeoText { text: qsTr("Notification and system sounds"); typeRole: "title"; typeSize: "small" }
            MeoSlider {
                width: parent.width; from: 0; to: 100; value: AudioBackend.notificationSound.volume || 0
                enabled: AudioBackend.notificationSound.writable || false
                Accessible.name: qsTr("Notification sound volume")
                onMoved: value => AudioBackend.setNotificationSound(Math.round(value), AudioBackend.notificationSound.muted)
            }
            MeoSwitch {
                checked: !(AudioBackend.notificationSound.muted || false)
                enabled: AudioBackend.notificationSound.writable || false
                Accessible.name: qsTr("Enable notification sounds")
                onToggled: enabled => AudioBackend.setNotificationSound(AudioBackend.notificationSound.volume, !enabled)
            }
        }
        MeoText { text: qsTr("Applications"); typeRole: "title"; typeSize: "small"; visible: AudioBackend.streams.length > 0 }
        Repeater {
            model: AudioBackend.streams
            delegate: Column {
                id: streamEntry
                required property var modelData
                width: parent.width
                spacing: MeoTheme.space8
                readonly property var devices: modelData.input ? AudioBackend.inputs : AudioBackend.outputs
                readonly property string currentDevice: {
                    for (const device of devices) if (device.index === modelData.deviceIndex) return device.name
                    return qsTr("Unavailable")
                }
                MeoText { text: streamEntry.modelData.name + (streamEntry.modelData.input ? qsTr(" · Recording") : qsTr(" · Playback")); typeRole: "title"; typeSize: "small" }
                RowLayout {
                    width: parent.width
                    MeoSlider {
                        Layout.fillWidth: true; from: 0; to: 100
                        value: streamEntry.modelData.volume
                        enabled: streamEntry.modelData.hasVolume
                        Accessible.name: qsTr("Application volume for %1").arg(streamEntry.modelData.name)
                        onMoved: value => AudioBackend.setStreamVolume(streamEntry.modelData.id, Math.round(value))
                    }
                    MeoSwitch {
                        checked: !streamEntry.modelData.muted
                        Accessible.name: qsTr("Enable audio for %1").arg(streamEntry.modelData.name)
                        onToggled: enabled => AudioBackend.setStreamMuted(streamEntry.modelData.id, !enabled)
                    }
                }
                MeoExposedDropdown {
                    width: parent.width; label: qsTr("Device"); text: streamEntry.currentDevice
                    model: streamEntry.devices.map(device => device.name)
                    onSelected: (index, value) => AudioBackend.setStreamDevice(streamEntry.modelData.id, streamEntry.devices[index].id)
                }
            }
        }
        MeoText { text: qsTr("Hardware profiles"); typeRole: "title"; typeSize: "small"; visible: AudioBackend.cards.length > 0 }
        Repeater {
            model: AudioBackend.cards
            delegate: MeoExposedDropdown {
                required property var modelData
                width: parent.width; label: modelData.name
                readonly property var profiles: modelData.profiles.filter(profile => profile.available || profile.index === modelData.activeProfile)
                model: profiles.map(profile => profile.label)
                text: modelData.profiles[modelData.activeProfile]?.label || qsTr("Unavailable")
                onSelected: (index, value) => AudioBackend.setCardProfile(modelData.id, profiles[index].index)
            }
        }
        Repeater {
            model: AudioBackend.outputs.map(device => Object.assign({}, device, {input: false})).concat(AudioBackend.inputs.map(device => Object.assign({}, device, {input: true})))
            delegate: Column {
                id: hardwareEntry
                required property var modelData
                width: parent.width; spacing: MeoTheme.space8
                readonly property var ports: modelData.ports.filter(port => port.available || port.active)
                MeoExposedDropdown {
                    width: parent.width; visible: hardwareEntry.ports.length > 0
                    label: qsTr("%1 · Port").arg(hardwareEntry.modelData.name)
                    model: hardwareEntry.ports.map(port => port.name)
                    text: hardwareEntry.ports.find(port => port.active)?.name || qsTr("Unavailable")
                    onSelected: (index, value) => AudioBackend.setDevicePort(hardwareEntry.modelData.id, hardwareEntry.modelData.input, hardwareEntry.ports[index].index)
                }
                Repeater {
                    model: hardwareEntry.modelData.channels
                    delegate: RowLayout {
                        required property var modelData
                        width: parent.width
                        MeoText { text: hardwareEntry.modelData.name + " · " + modelData.name; Layout.maximumWidth: parent.width / 2; elide: Text.ElideRight }
                        MeoSlider {
                            Layout.fillWidth: true; from: 0; to: 100; value: parent.modelData.volume
                            Accessible.name: qsTr("%1 channel volume").arg(parent.modelData.name)
                            onMoved: value => AudioBackend.setChannelVolume(hardwareEntry.modelData.id, hardwareEntry.modelData.input, parent.modelData.index, Math.round(value))
                        }
                    }
                }
            }
        }

        MeoButton {
            text: qsTr("Advanced sound settings")
            type: "text"
            enabled: KcmBridge.isAvailable("kcm_pulseaudio")
            onClicked: root.navigateTo("kcm:kcm_pulseaudio")
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !AudioBackend.available
            icon: "volume_off"
            title: qsTr("Audio service is unavailable")
            description: qsTr("No audio output is available right now.")
            actionText: KcmBridge.isAvailable("kcm_pulseaudio") ? qsTr("Open KDE sound settings") : ""
            onActionClicked: root.navigateTo("kcm:kcm_pulseaudio")
        }

        RepairEntry {
            category: "audio"
            entryTitle: qsTr("Troubleshoot sound")
        }
    }
}
