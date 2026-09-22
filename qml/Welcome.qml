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

    required property var welcomeState
    required property var networkBackend
    required property var accountBackend
    required property var fingerprintBackend

    readonly property var steps: [
        {"title": qsTr("Installed system check"), "description": qsTr("Check this installed Meo system before choosing anything else."), "icon": "fact_check", "route": "home"},
        {"title": qsTr("Appearance"), "description": qsTr("Make your desktop feel personal and easy to read."), "icon": "palette", "route": "appearance"},
        {"title": qsTr("Internet"), "description": qsTr("Check the whole system connection, including Ethernet, Wi-Fi, VPN, and captive portals."), "icon": "wifi", "route": "wifi"},
        {"title": qsTr("Meo Account"), "description": qsTr("Connect cloud services only on the installed system, never inside the Live installer."), "icon": "account_circle", "route": "accounts"},
        {"title": qsTr("Displays"), "description": qsTr("Adjust your display in Settings when you are ready."), "icon": "monitor", "route": "display"},
        {"title": qsTr("Fingerprint"), "description": qsTr("Review support for this device before changing any fingerprint settings."), "icon": "fingerprint", "route": "hardware"},
        {"title": qsTr("Updates & recovery"), "description": qsTr("See available update and recovery information for this device."), "icon": "system_update", "route": "recovery"},
        {"title": qsTr("You're ready"), "description": qsTr("Return to Meo Settings whenever you want."), "icon": "check_circle", "route": "home"}
    ]
    property int currentStep: 0
    readonly property var current: steps[currentStep]
    readonly property var fingerprintDefaults: root.fingerprintBackend.supportPackage()
    readonly property bool lockScreenFingerprintDefault: fingerprintDefaults.defaultPAMTargets
                                                       && fingerprintDefaults.defaultPAMTargets.indexOf("lock-screen") >= 0
    readonly property bool passwordFallbackDefault: fingerprintDefaults.passwordFallbackRequired === true
    readonly property string networkCheckValue: root.networkBackend.internetAvailable
                                                 ? qsTr("Online")
                                                 : root.networkBackend.connectivityState === "portal"
                                                   ? qsTr("Sign-in required")
                                                   : root.networkBackend.systemConnected
                                                     ? qsTr("Limited")
                                                     : qsTr("Offline")
    readonly property string accountCheckValue: root.accountBackend.signedIn
                                                 ? qsTr("Connected")
                                                 : root.accountBackend.serviceRunning
                                                   ? qsTr("Not connected")
                                                   : qsTr("Unavailable")

    MeoPageLayout {
        anchors.fill: parent
        title: root.currentStep === 0 ? qsTr("Welcome to Meo") : root.current.title
        subtitle: root.currentStep === 0 ? qsTr("Fast. Private. Yours.") : ""

        MeoCard {
            width: parent.width
            type: "filled"
            implicitHeight: 260 * MeoTheme.globalScale
            Accessible.name: root.current.title
            Accessible.description: root.current.description
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
            visible: root.currentStep === 0
            Accessible.name: qsTr("Installed system status")
            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale

                Repeater {
                    model: [
                        {"icon": "computer", "title": qsTr("Environment"), "value": qsTr("Installed system")},
                        {"icon": "wifi", "title": qsTr("Network"), "value": root.networkCheckValue},
                        {"icon": "account_circle", "title": qsTr("Meo Account"), "value": root.accountCheckValue},
                        {"icon": "fingerprint", "title": qsTr("Fingerprint"), "value": root.fingerprintBackend.devicePresent ? qsTr("Detected") : qsTr("Not detected")}
                    ]

                    delegate: RowLayout {
                        id: systemCheckRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10 * MeoTheme.globalScale
                        MeoIcon { icon: systemCheckRow.modelData.icon; size: 20; color: MeoTheme.contentOnSurfaceVariant }
                        MeoText { Layout.fillWidth: true; text: systemCheckRow.modelData.title; typeRole: "body"; typeSize: "small" }
                        MeoText { text: systemCheckRow.modelData.value; typeRole: "label"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurfaceVariant }
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === 2
            Accessible.name: qsTr("System network status")
            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale

                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon { icon: root.networkBackend.internetAvailable ? "wifi" : "wifi_off"; size: 24; color: MeoTheme.primary }
                    MeoText {
                        Layout.fillWidth: true
                        text: root.networkBackend.primaryConnectionName !== ""
                              ? root.networkBackend.primaryConnectionName
                              : qsTr("No primary connection")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoBadge { text: root.networkCheckValue }
                }
                MeoText {
                    Layout.fillWidth: true
                    text: root.networkBackend.connectivityState === "portal"
                          ? qsTr("A captive-portal sign-in is required before full Internet access is available.")
                          : root.networkBackend.internetAvailable
                            ? qsTr("NetworkManager reports full Internet connectivity for this installed system.")
                            : qsTr("Open Wi-Fi settings to connect or review the current network.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === 3
            Accessible.name: qsTr("Meo Account status")
            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale

                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon { icon: "account_circle"; size: 24; color: MeoTheme.secondary }
                    MeoText {
                        Layout.fillWidth: true
                        text: root.accountBackend.signedIn && root.accountBackend.cloudName !== ""
                              ? root.accountBackend.cloudName
                              : qsTr("Meo Account")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoBadge { text: root.accountCheckValue }
                }
                MeoText {
                    Layout.fillWidth: true
                    text: root.accountBackend.summary
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    MeoButton {
                        visible: !root.accountBackend.signedIn && root.accountBackend.serviceRunning && root.accountBackend.oauthConfigured
                        text: qsTr("Connect Meo Account")
                        type: "filled"
                        enabled: !root.accountBackend.busy
                        loading: root.accountBackend.busy
                        onClicked: root.accountBackend.requestAuthentication()
                    }
                    MeoButton {
                        text: qsTr("Account settings")
                        type: "outlined"
                        onClicked: root.accountBackend.openAccountSettings()
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === root.steps.length - 1
            Accessible.name: qsTr("Setup complete")
            Accessible.description: qsTr("Review these installed-system checks again from Meo Settings at any time.")
            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale
                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon { icon: "check_circle"; size: 24; color: MeoTheme.primary }
                    MeoText { Layout.fillWidth: true; text: qsTr("Setup complete"); typeRole: "title"; typeSize: "small"; emphasized: true }
                }
                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Network, account, hardware, updates, and recovery remain available in Meo Settings.")
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                }
                MeoButton { text: qsTr("Open Meo Settings"); type: "outlined"; onClicked: root.welcomeState.openSettings("home") }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: root.currentStep === 5
            Accessible.name: qsTr("Fingerprint support")
            Accessible.description: qsTr("Review whether this device supports fingerprint authentication. This screen does not enroll fingerprints or change sign-in settings.")

            ColumnLayout {
                anchors.fill: parent
                spacing: 10 * MeoTheme.globalScale

                RowLayout {
                    Layout.fillWidth: true
                    MeoIcon {
                        icon: "fingerprint"
                        size: 24
                        color: root.fingerprintBackend.devicePresent ? MeoTheme.primary : MeoTheme.contentOnSurfaceVariant
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: root.fingerprintBackend.devicePresent
                              ? root.fingerprintBackend.deviceLabel
                              : qsTr("No compatible fingerprint reader detected")
                        typeRole: "title"
                        typeSize: "small"
                        emphasized: true
                    }
                    MeoBadge {
                        text: root.fingerprintBackend.devicePresent ? qsTr("Experimental") : qsTr("Unavailable")
                        color: root.fingerprintBackend.devicePresent ? MeoTheme.tertiary : MeoTheme.surfaceVariant
                    }
                }

                MeoText {
                    Layout.fillWidth: true
                    text: root.fingerprintBackend.supportSummary
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Fingerprint support details")
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
                        id: fingerprintDefaultRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10 * MeoTheme.globalScale
                        MeoIcon { icon: fingerprintDefaultRow.modelData.icon; size: 18; color: MeoTheme.contentOnSurfaceVariant }
                        MeoText { Layout.fillWidth: true; text: fingerprintDefaultRow.modelData.title; typeRole: "body"; typeSize: "small" }
                        MeoText { text: fingerprintDefaultRow.modelData.value; typeRole: "label"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurfaceVariant }
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
                    onClicked: root.welcomeState.openSettings("hardware")
                }
            }
        }

        MeoProgressBar {
            width: parent.width
            value: (root.currentStep + 1) / root.steps.length
            Accessible.name: qsTr("Welcome progress")
            Accessible.description: qsTr("Step %1 of %2: %3").arg(root.currentStep + 1).arg(root.steps.length).arg(root.current.title)
        }

        Column {
            width: parent.width
            visible: root.welcomeState.error !== ""
            spacing: MeoTheme.space4
            MeoBanner {
                width: parent.width
                title: qsTr("Couldn't open Meo Settings")
                text: qsTr("Try again after Meo Settings is installed and available.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(root.welcomeState.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            width: parent.width
            spacing: 12 * MeoTheme.globalScale
            MeoButton {
                text: qsTr("Skip for now")
                type: "text"
                visible: root.currentStep < root.steps.length - 1
                onClicked: root.welcomeState.complete()
            }
            Item { Layout.fillWidth: true }
            MeoButton {
                text: root.currentStep === root.steps.length - 1 ? qsTr("Finish") : qsTr("Open settings")
                type: "outlined"
                onClicked: {
                    if (root.currentStep === root.steps.length - 1)
                        root.welcomeState.complete()
                    else
                        root.welcomeState.openSettings(root.current.route)
                }
            }
            MeoButton {
                text: root.currentStep === root.steps.length - 1 ? qsTr("Done") : qsTr("Next")
                onClicked: {
                    if (root.currentStep === root.steps.length - 1)
                        root.welcomeState.complete()
                    else
                        ++root.currentStep
                }
            }
        }
    }
}
