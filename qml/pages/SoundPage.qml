import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Sound")
        subtitle: AudioBackend.pipeWire
                  ? qsTr("Audio is provided by PipeWire through KDE’s PulseAudioQt backend.")
                  : qsTr("Choose audio devices and adjust volume through KDE’s PulseAudioQt backend.")

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: AudioBackend.error !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: AudioBackend.error
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
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
            description: qsTr("No default output is available from the audio service.")
            actionText: KcmBridge.isAvailable("kcm_pulseaudio") ? qsTr("Open KDE sound settings") : ""
            onActionClicked: root.navigateTo("kcm:kcm_pulseaudio")
        }
    }
}
