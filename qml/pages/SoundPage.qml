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
