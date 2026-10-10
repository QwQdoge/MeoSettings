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

    function deviceIcon(device, input) {
        if (input)
            return "mic"
        const form = String(device.formFactor || "").toLowerCase()
        return form === "headphones" || form === "headset" ? "headphones" : "speaker"
    }

    function availableProfiles(card) {
        return (card.profiles || []).filter(profile => profile.available || profile.index === card.activeProfile)
    }

    function activeProfileLabel(card) {
        const profiles = card.profiles || []
        for (let index = 0; index < profiles.length; ++index) {
            if (profiles[index].index === card.activeProfile)
                return profiles[index].label
        }
        return qsTr("Current profile")
    }

    function availablePorts(device) {
        return (device.ports || []).filter(port => port.available || port.active)
    }

    function activePortLabel(device) {
        const ports = device.ports || []
        for (let index = 0; index < ports.length; ++index) {
            if (ports[index].active)
                return ports[index].name
        }
        return qsTr("Automatic")
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 820 * MeoTheme.globalScale
        expandedWidth: MeoTheme.settingsContentMaxWidth
        title: root.isCompact ? "" : qsTr("Sound")
        subtitle: qsTr("Choose speakers and microphones, then adjust app audio only when needed.")

        Column {
            width: parent.width
            visible: AudioBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Sound needs attention")
                text: qsTr("The last audio change could not be applied. Check that the device is still connected and try again.")
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

        MeoSettingsGroup {
            width: parent.width
            visible: AudioBackend.available && AudioBackend.outputs.length > 0
            title: qsTr("Output device")
            subtitle: qsTr("Choose where system and application audio plays")
            model: AudioBackend.outputs.map(device => ({
                "id": device.id,
                "title": device.name,
                "subtitle": device.active ? qsTr("Current output") : (device.formFactor || qsTr("Audio output")),
                "icon": root.deviceIcon(device, false),
                "tone": device.active ? "primary" : "neutral",
                "trailingKind": "radio",
                "checked": device.active
            }))
            onRowToggled: (index, checked, row) => {
                if (checked)
                    AudioBackend.setDefaultOutput(row.id)
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: AudioBackend.available
            Accessible.role: Accessible.StatusBar
            Accessible.name: AudioBackend.outputMuted
                             ? qsTr("Output is muted")
                             : qsTr("Output volume %1 percent").arg(AudioBackend.outputVolume)

            Column {
                width: parent.width
                spacing: MeoTheme.space12

                RowLayout {
                    width: parent.width
                    spacing: MeoTheme.space12
                    MeoIcon {
                        icon: AudioBackend.outputMuted ? "volume_off" : "volume_up"
                        size: 26
                        color: MeoTheme.primary
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space2
                        MeoText {
                            Layout.fillWidth: true
                            text: qsTr("Output volume")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                        }
                        MeoText {
                            Layout.fillWidth: true
                            text: AudioBackend.outputName || qsTr("Default output")
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            elide: Text.ElideRight
                        }
                    }
                    MeoText {
                        text: AudioBackend.outputVolume + "%"
                        typeRole: "label"
                        typeSize: "medium"
                        emphasized: true
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSwitch {
                        id: outputMuteSwitch
                        checked: !AudioBackend.outputMuted
                        Accessible.name: qsTr("Enable output audio")
                        onToggled: enabled => AudioBackend.outputMuted = !enabled
                    }
                }

                MeoSlider {
                    id: outputVolumeSlider
                    width: parent.width
                    from: 0
                    to: 150
                    value: AudioBackend.outputVolume
                    enabled: !AudioBackend.outputMuted
                    Accessible.name: qsTr("Output volume")
                    Accessible.description: qsTr("%1 percent").arg(Math.round(value))
                    onMoved: currentValue => AudioBackend.outputVolume = Math.round(currentValue)
                }
            }
        }

        MeoButton {
            visible: AudioBackend.available
            text: AudioBackend.testingSound ? qsTr("Stop test sound") : qsTr("Test speakers")
            enabled: !AudioBackend.outputMuted
            type: "tonal"
            onClicked: AudioBackend.testingSound ? AudioBackend.stopTestSound() : AudioBackend.testSound()
        }

        MeoSettingsGroup {
            width: parent.width
            visible: AudioBackend.microphoneAvailable && AudioBackend.inputs.length > 0
            title: qsTr("Microphone")
            subtitle: qsTr("Choose the microphone used by recording and calling apps")
            model: AudioBackend.inputs.map(device => ({
                "id": device.id,
                "title": device.name,
                "subtitle": device.active ? qsTr("Current microphone") : (device.formFactor || qsTr("Audio input")),
                "icon": "mic",
                "tone": device.active ? "secondary" : "neutral",
                "trailingKind": "radio",
                "checked": device.active
            }))
            onRowToggled: (index, checked, row) => {
                if (checked)
                    AudioBackend.setDefaultInput(row.id)
            }
        }

        MeoCard {
            width: parent.width
            type: "filled"
            visible: AudioBackend.microphoneAvailable
            Accessible.role: Accessible.StatusBar
            Accessible.name: AudioBackend.inputMuted
                             ? qsTr("Microphone is muted")
                             : qsTr("Microphone volume %1 percent").arg(AudioBackend.inputVolume)

            Column {
                width: parent.width
                spacing: MeoTheme.space12

                RowLayout {
                    width: parent.width
                    spacing: MeoTheme.space12
                    MeoIcon {
                        icon: AudioBackend.inputMuted ? "mic_off" : "mic"
                        size: 26
                        color: MeoTheme.secondary
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space2
                        MeoText {
                            Layout.fillWidth: true
                            text: qsTr("Microphone level")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                        }
                        MeoText {
                            Layout.fillWidth: true
                            text: AudioBackend.inputName || qsTr("Default microphone")
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            elide: Text.ElideRight
                        }
                    }
                    MeoText {
                        text: AudioBackend.inputVolume + "%"
                        typeRole: "label"
                        typeSize: "medium"
                        emphasized: true
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                    MeoSwitch {
                        checked: !AudioBackend.inputMuted
                        Accessible.name: qsTr("Enable microphone")
                        onToggled: enabled => AudioBackend.inputMuted = !enabled
                    }
                }

                MeoSlider {
                    width: parent.width
                    from: 0
                    to: 150
                    value: AudioBackend.inputVolume
                    enabled: !AudioBackend.inputMuted
                    Accessible.name: qsTr("Microphone volume")
                    Accessible.description: qsTr("%1 percent").arg(Math.round(value))
                    onMoved: currentValue => AudioBackend.inputVolume = Math.round(currentValue)
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: Object.keys(AudioBackend.notificationSound).length > 0

            RowLayout {
                anchors.fill: parent
                spacing: MeoTheme.space12
                MeoIcon {
                    icon: "notifications"
                    size: 24
                    color: MeoTheme.contentOnSurfaceVariant
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    MeoText {
                        Layout.fillWidth: true
                        text: qsTr("System sounds")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: AudioBackend.notificationSound.writable
                              ? qsTr("Notification and interface sound volume")
                              : qsTr("Managed by the current audio session")
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                }
                MeoSwitch {
                    checked: !(AudioBackend.notificationSound.muted || false)
                    enabled: AudioBackend.notificationSound.writable || false
                    Accessible.name: qsTr("Enable system sounds")
                    onToggled: enabled => AudioBackend.setNotificationSound(AudioBackend.notificationSound.volume, !enabled)
                }
            }
        }

        Column {
            width: parent.width
            visible: Object.keys(AudioBackend.notificationSound).length > 0
                     && (AudioBackend.notificationSound.writable || false)
            spacing: MeoTheme.space4
            MeoSlider {
                width: parent.width
                from: 0
                to: 100
                value: AudioBackend.notificationSound.volume || 0
                enabled: !(AudioBackend.notificationSound.muted || false)
                Accessible.name: qsTr("System sound volume")
                onMoved: currentValue => AudioBackend.setNotificationSound(Math.round(currentValue), AudioBackend.notificationSound.muted)
            }
        }

        MeoText {
            width: parent.width
            text: qsTr("Applications")
            typeRole: "title"
            typeSize: "small"
            emphasized: true
            visible: AudioBackend.streams.length > 0
        }

        Column {
            width: parent.width
            spacing: MeoTheme.space12
            visible: AudioBackend.streams.length > 0

            Repeater {
                model: AudioBackend.streams

                delegate: MeoCard {
                    id: streamCard
                    required property var modelData
                    width: parent.width
                    type: "outlined"
                    readonly property var devices: modelData.input ? AudioBackend.inputs : AudioBackend.outputs
                    readonly property string currentDevice: {
                        for (let index = 0; index < devices.length; ++index) {
                            if (devices[index].index === modelData.deviceIndex)
                                return devices[index].name
                        }
                        return qsTr("Current device")
                    }

                    Column {
                        width: parent.width
                        spacing: MeoTheme.space12

                        RowLayout {
                            width: parent.width
                            spacing: MeoTheme.space12
                            MeoIcon {
                                icon: streamCard.modelData.input ? "mic" : "play_circle"
                                size: 24
                                color: streamCard.modelData.input ? MeoTheme.secondary : MeoTheme.primary
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                MeoText {
                                    Layout.fillWidth: true
                                    text: streamCard.modelData.name
                                    typeRole: "title"
                                    typeSize: "small"
                                    emphasized: true
                                    elide: Text.ElideRight
                                }
                                MeoText {
                                    Layout.fillWidth: true
                                    text: streamCard.modelData.input ? qsTr("Recording") : qsTr("Playback")
                                    typeRole: "body"
                                    typeSize: "small"
                                    color: MeoTheme.contentOnSurfaceVariant
                                }
                            }
                            MeoText {
                                visible: streamCard.modelData.hasVolume
                                text: Math.round(streamCard.modelData.volume) + "%"
                                typeRole: "label"
                                typeSize: "medium"
                                color: MeoTheme.contentOnSurfaceVariant
                            }
                            MeoSwitch {
                                checked: !streamCard.modelData.muted
                                Accessible.name: qsTr("Enable audio for %1").arg(streamCard.modelData.name)
                                onToggled: enabled => AudioBackend.setStreamMuted(streamCard.modelData.id, !enabled)
                            }
                        }

                        MeoSlider {
                            width: parent.width
                            visible: streamCard.modelData.hasVolume
                            from: 0
                            to: 100
                            value: streamCard.modelData.volume
                            enabled: !streamCard.modelData.muted
                            Accessible.name: qsTr("Application volume for %1").arg(streamCard.modelData.name)
                            onMoved: currentValue => AudioBackend.setStreamVolume(streamCard.modelData.id, Math.round(currentValue))
                        }

                        MeoExposedDropdown {
                            width: parent.width
                            visible: streamCard.devices.length > 1
                            label: streamCard.modelData.input ? qsTr("Record from") : qsTr("Play through")
                            text: streamCard.currentDevice
                            model: streamCard.devices.map(device => device.name)
                            onSelected: (index, value) => AudioBackend.setStreamDevice(streamCard.modelData.id, streamCard.devices[index].id)
                        }

                        MeoText {
                            width: parent.width
                            visible: streamCard.devices.length === 1
                            text: (streamCard.modelData.input ? qsTr("Recording from ") : qsTr("Playing through ")) + streamCard.currentDevice
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            visible: AudioBackend.cards.length > 0
            type: "outlined"

            Column {
                width: parent.width
                spacing: MeoTheme.space8

                MeoText {
                    width: parent.width
                    text: qsTr("Advanced hardware routing")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Profiles and physical ports are normally automatic. Change them only when HDMI, headphones, or microphone routing is wrong.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: AudioBackend.cards
                    delegate: Column {
                        id: cardEntry
                        required property var modelData
                        width: parent.width
                        spacing: MeoTheme.space4
                        readonly property var profiles: root.availableProfiles(modelData)

                        MeoExposedDropdown {
                            width: parent.width
                            visible: cardEntry.profiles.length > 1
                            label: cardEntry.modelData.name
                            model: cardEntry.profiles.map(profile => profile.label)
                            text: root.activeProfileLabel(cardEntry.modelData)
                            onSelected: (index, value) => AudioBackend.setCardProfile(cardEntry.modelData.id, cardEntry.profiles[index].index)
                        }

                        MeoText {
                            width: parent.width
                            visible: cardEntry.profiles.length === 1
                            text: cardEntry.modelData.name + " · " + root.activeProfileLabel(cardEntry.modelData)
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            elide: Text.ElideRight
                        }
                    }
                }

                Repeater {
                    model: AudioBackend.outputs.map(device => Object.assign({}, device, {input: false}))
                              .concat(AudioBackend.inputs.map(device => Object.assign({}, device, {input: true})))
                    delegate: Column {
                        id: portEntry
                        required property var modelData
                        width: parent.width
                        spacing: MeoTheme.space4
                        readonly property var ports: root.availablePorts(modelData)

                        MeoExposedDropdown {
                            width: parent.width
                            visible: portEntry.ports.length > 1
                            label: qsTr("%1 port").arg(portEntry.modelData.name)
                            model: portEntry.ports.map(port => port.name)
                            text: root.activePortLabel(portEntry.modelData)
                            onSelected: (index, value) => AudioBackend.setDevicePort(portEntry.modelData.id, portEntry.modelData.input, portEntry.ports[index].index)
                        }
                    }
                }

                MeoButton {
                    text: qsTr("Open full audio controls")
                    type: "text"
                    enabled: KcmBridge.isAvailable("kcm_pulseaudio")
                    onClicked: root.navigateTo("kcm:kcm_pulseaudio")
                }
            }
        }

        MeoEmptyState {
            width: parent.width
            height: 260 * MeoTheme.globalScale
            visible: !AudioBackend.available
            icon: "volume_off"
            title: qsTr("Audio service is unavailable")
            description: qsTr("No working audio output is available right now.")
            actionText: KcmBridge.isAvailable("kcm_pulseaudio") ? qsTr("Open full audio controls") : ""
            onActionClicked: root.navigateTo("kcm:kcm_pulseaudio")
        }

        RepairEntry {
            category: "audio"
            entryTitle: qsTr("Troubleshoot sound")
        }
    }
}
