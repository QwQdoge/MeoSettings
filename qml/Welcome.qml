import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

ApplicationWindow {
    id: root

    width: 680 * MeoTheme.globalScale
    height: 600 * MeoTheme.globalScale
    minimumWidth: 420 * MeoTheme.globalScale
    minimumHeight: 520 * MeoTheme.globalScale
    visible: true
    title: qsTr("Welcome to Meo")
    color: MeoTheme.surface

    readonly property var steps: [
        {"title": qsTr("Appearance"), "description": qsTr("Choose a personal, readable Material 3 Expressive workspace."), "icon": "palette", "route": "appearance"},
        {"title": qsTr("Internet"), "description": qsTr("Connect using Meo's clear NetworkManager-powered controls."), "icon": "wifi", "route": "wifi"},
        {"title": qsTr("Displays"), "description": qsTr("Set up your screen safely. Meo will restore a layout you do not keep."), "icon": "monitor", "route": "display"},
        {"title": qsTr("Fingerprint"), "description": qsTr("Review your device support before choosing whether to enable fingerprint authentication."), "icon": "fingerprint", "route": "hardware"},
        {"title": qsTr("Updates & recovery"), "description": qsTr("Understand full system updates and the recovery protection available on this device."), "icon": "system_update", "route": "recovery"},
        {"title": qsTr("You're ready"), "description": qsTr("Everything remains adjustable from Meo Settings whenever you need it."), "icon": "check_circle", "route": "home"}
    ]
    property int currentStep: 0
    readonly property var current: steps[currentStep]
    readonly property var fingerprintDefaults: FingerprintBackend.supportPackage()
    readonly property bool lockScreenFingerprintDefault: fingerprintDefaults.defaultPAMTargets
                                                       && fingerprintDefaults.defaultPAMTargets.indexOf("lock-screen") >= 0
    readonly property bool passwordFallbackDefault: fingerprintDefaults.passwordFallbackRequired === true

    MeoPageLayout {
        anchors.fill: parent
        title: currentStep === 0 ? qsTr("Welcome to Meo") : current.title
        subtitle: currentStep === 0 ? qsTr("Fast. Private. Yours.") : ""

        MeoCard {
            width: parent.width
            type: "filled"
            implicitHeight: 260 * MeoTheme.globalScale
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48 * MeoTheme.globalScale
                spacing: 16 * MeoTheme.globalScale
                MeoIcon { Layout.alignment: Qt.AlignHCenter; icon: root.current.icon; size: 56; color: MeoTheme.primary }
                MeoText { Layout.fillWidth: true; text: root.current.description; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; typeRole: "body"; typeSize: "large"; color: MeoTheme.contentOnSurface }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === root.steps.length - 1
            Accessible.name: qsTr("Optional Meo Account connection")
            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale
                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon { icon: "account_circle"; size: 24; color: MeoTheme.secondary }
                    MeoText { Layout.fillWidth: true; text: qsTr("Connect Meo Account"); typeRole: "title"; typeSize: "small"; emphasized: true }
                    MeoBadge { text: qsTr("Optional"); color: MeoTheme.secondaryContainer }
                }
                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Connect cloud services, backups, and AI credentials after setup. Your installer never received an account password or cloud session.")
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                }
                MeoButton { text: qsTr("Open account settings"); type: "outlined"; onClicked: WelcomeState.openSettings("accounts") }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === 3
            Accessible.name: qsTr("Fingerprint default authentication policy")

            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale

                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon {
                        icon: "fingerprint"
                        size: 24
                        color: FingerprintBackend.devicePresent ? MeoTheme.primary : MeoTheme.contentOnSurfaceVariant
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: FingerprintBackend.devicePresent
                              ? FingerprintBackend.deviceLabel
                              : qsTr("No compatible fingerprint reader detected")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoBadge {
                        text: FingerprintBackend.devicePresent ? qsTr("Experimental") : qsTr("Unavailable")
                        color: FingerprintBackend.devicePresent ? MeoTheme.tertiary : MeoTheme.surfaceVariant
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

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Fingerprint defaults")
                    typeRole: "label"
                    typeSize: "medium"
                    emphasized: true
                }

                Repeater {
                    model: [
                        {"icon": "lock", "title": qsTr("Lock screen"), "value": root.lockScreenFingerprintDefault ? qsTr("On after enrollment") : qsTr("Off")},
                        {"icon": "login", "title": qsTr("Login authentication"), "value": qsTr("Off")},
                        {"icon": "admin_panel_settings", "title": qsTr("sudo and system authorization"), "value": qsTr("Off")},
                        {"icon": "password", "title": qsTr("Password fallback"), "value": root.passwordFallbackDefault ? qsTr("Always available") : qsTr("Check settings")}
                    ]

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10 * MeoTheme.globalScale
                        MeoIcon { icon: modelData.icon; size: 18; color: MeoTheme.contentOnSurfaceVariant }
                        MeoText { Layout.fillWidth: true; text: modelData.title; typeRole: "body"; typeSize: "small" }
                        MeoText { text: modelData.value; typeRole: "label"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurfaceVariant }
                    }
                }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Meo never reads or exports fingerprint images, templates, or matcher internals.")
                    typeRole: "label"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoButton {
                    text: qsTr("Review fingerprint support")
                    type: "outlined"
                    onClicked: WelcomeState.openSettings("hardware")
                }
            }
        }

        MeoProgressBar {
            width: parent.width
            value: (currentStep + 1) / steps.length
            Accessible.name: qsTr("Welcome progress")
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: WelcomeState.error !== ""
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: WelcomeState.error
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.error
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 12 * MeoTheme.globalScale
            MeoButton {
                text: qsTr("Skip for now")
                type: "text"
                visible: currentStep < steps.length - 1
                onClicked: WelcomeState.complete()
            }
            Item { Layout.fillWidth: true }
            MeoButton {
                text: currentStep === steps.length - 1 ? qsTr("Finish") : qsTr("Open settings")
                type: "outlined"
                onClicked: {
                    if (currentStep === steps.length - 1)
                        WelcomeState.complete()
                    else
                        WelcomeState.openSettings(root.current.route)
                }
            }
            MeoButton {
                text: currentStep === steps.length - 1 ? qsTr("Done") : qsTr("Next")
                onClicked: {
                    if (currentStep === steps.length - 1)
                        WelcomeState.complete()
                    else
                        ++currentStep
                }
            }
        }
    }
}
