import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

ApplicationWindow {
    id: root

    width: 960 * MeoTheme.globalScale
    height: 660 * MeoTheme.globalScale
    minimumWidth: 420 * MeoTheme.globalScale
    minimumHeight: 560 * MeoTheme.globalScale
    visible: true
    title: qsTr("Welcome to Meo")
    color: MeoTheme.surface

    required property var welcomeState
    required property var fingerprintBackend
    required property var networkBackend
    required property var accountBackend

    readonly property bool compact: width < 760 * MeoTheme.globalScale
    readonly property var fingerprintDefaults: root.fingerprintBackend.supportPackage()
    readonly property bool lockScreenFingerprintDefault: fingerprintDefaults.defaultPAMTargets
                                                       && fingerprintDefaults.defaultPAMTargets.indexOf("lock-screen") >= 0
    readonly property bool passwordFallbackDefault: fingerprintDefaults.passwordFallbackRequired === true
    readonly property string networkCheckValue: root.networkBackend.internetAvailable
                                                 ? qsTr("Online")
                                                 : root.networkBackend.connectivityState === "portal"
                                                   ? qsTr("Sign-in required")
                                                   : root.networkBackend.connectivityState === "limited"
                                                     ? qsTr("Limited")
                                                     : root.networkBackend.connectivityState === "connecting"
                                                       ? qsTr("Connecting")
                                                       : root.networkBackend.systemConnected
                                                         ? qsTr("Connected")
                                                         : qsTr("Offline")
    readonly property string accountCheckValue: root.accountBackend.signedIn
                                                 ? qsTr("Connected")
                                                 : root.accountBackend.serviceRunning
                                                   ? qsTr("Not connected")
                                                   : qsTr("Unavailable")

    readonly property var steps: [
        {
            "title": qsTr("Make Meo yours"),
            "description": qsTr("Choose the look you want. Meo keeps the desktop underneath familiar and lets KDE continue owning the system behavior."),
            "icon": "palette",
            "tone": "primary",
            "links": [
                {"text": qsTr("Appearance"), "route": "appearance", "icon": "palette"}
            ]
        },
        {
            "title": qsTr("Get connected"),
            "description": qsTr("Join Wi-Fi and pair devices with the same system services Plasma already uses."),
            "icon": "wifi",
            "tone": "secondary",
            "links": [
                {"text": qsTr("Wi-Fi"), "route": "wifi", "icon": "wifi"},
                {"text": qsTr("Bluetooth"), "route": "bluetooth", "icon": "bluetooth"}
            ]
        },
        {
            "title": qsTr("Sign in and unlock"),
            "description": qsTr("Review login, lock-screen, keyboard, session, and fingerprint options without replacing KDE authentication."),
            "icon": "lock",
            "tone": "tertiary",
            "links": [
                {"text": qsTr("Lock screen & login"), "route": "session-entry", "icon": "lock"},
                {"text": qsTr("Fingerprint"), "route": "hardware", "icon": "fingerprint"}
            ]
        },
        {
            "title": qsTr("Comfort on this device"),
            "description": qsTr("Tune displays and power policy while KScreen and PowerDevil remain the source of truth."),
            "icon": "devices",
            "tone": "primary",
            "links": [
                {"text": qsTr("Displays"), "route": "display", "icon": "monitor"},
                {"text": qsTr("Power"), "route": "power", "icon": "battery_full"}
            ]
        },
        {
            "title": qsTr("Private and recoverable"),
            "description": qsTr("Review privacy choices, updates, and recovery before you start changing the system."),
            "icon": "shield",
            "tone": "secondary",
            "links": [
                {"text": qsTr("Privacy"), "route": "privacy", "icon": "shield"},
                {"text": qsTr("Updates"), "route": "updates", "icon": "system_update"},
                {"text": qsTr("Recovery"), "route": "recovery", "icon": "restore"}
            ]
        },
        {
            "title": qsTr("Ready for your desktop"),
            "description": qsTr("You can come back to any of these controls from Meo Settings. Nothing here creates a second copy of Plasma's system state."),
            "icon": "check_circle",
            "tone": "tertiary",
            "links": [
                {"text": qsTr("Open Meo Settings"), "route": "home", "icon": "settings"},
                {"text": qsTr("Meo Account"), "route": "accounts", "icon": "account_circle"}
            ]
        }
    ]

    property int currentStep: 0
    readonly property var current: steps[currentStep]

    onCurrentStepChanged: {
        if (!MeoTheme.reduceMotion)
            stepEntrance.restart()
    }

    ParallelAnimation {
        id: stepEntrance
        NumberAnimation {
            target: contentColumn
            property: "opacity"
            from: 0.62
            to: 1
            duration: MeoTheme.motionDurationMedium1
            easing.type: Easing.BezierSpline
            easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate
        }
        NumberAnimation {
            target: contentColumn
            property: "y"
            from: 10 * MeoTheme.globalScale
            to: 0
            duration: MeoTheme.motionDurationMedium2
            easing.type: Easing.BezierSpline
            easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate
        }
    }

    function toneContainer(tone) {
        if (tone === "secondary")
            return MeoTheme.secondaryContainer
        if (tone === "tertiary")
            return MeoTheme.tertiaryContainer
        return MeoTheme.primaryContainer
    }

    function toneContent(tone) {
        if (tone === "secondary")
            return MeoTheme.contentOnSecondaryContainer
        if (tone === "tertiary")
            return MeoTheme.contentOnTertiaryContainer
        return MeoTheme.contentOnPrimaryContainer
    }

    function openRoute(route) {
        if (route)
            root.welcomeState.openSettings(route)
    }

    Rectangle {
        anchors.fill: parent
        color: MeoTheme.surface

        Rectangle {
            width: Math.max(300, parent.width * 0.42)
            height: width
            radius: width / 2
            x: root.compact ? parent.width - width * 0.55
                            : -width * 0.34 + (root.currentStep % 2) * MeoTheme.space24
            y: -height * 0.55 + (root.currentStep % 3) * MeoTheme.space12
            color: root.toneContainer(root.current.tone)
            opacity: MeoTheme.isDarkMode ? 0.28 : 0.58

            Behavior on x { NumberAnimation { duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium2; easing.type: Easing.BezierSpline; easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate } }
            Behavior on y { NumberAnimation { duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium2; easing.type: Easing.BezierSpline; easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate } }
            Behavior on color { ColorAnimation { duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium1 } }
        }

        Rectangle {
            width: Math.max(260, parent.width * 0.32)
            height: width
            radius: width / 2
            x: parent.width - width * 0.62
            y: parent.height - height * 0.48 - (root.currentStep % 2) * MeoTheme.space16
            color: root.currentStep % 2 === 0 ? MeoTheme.secondaryContainer : MeoTheme.tertiaryContainer
            opacity: MeoTheme.isDarkMode ? 0.18 : 0.40

            Behavior on y { NumberAnimation { duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium2; easing.type: Easing.BezierSpline; easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate } }
            Behavior on color { ColorAnimation { duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium1 } }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? MeoTheme.space16 : MeoTheme.space24
        spacing: MeoTheme.space24

        MeoCard {
            id: hero
            visible: !root.compact
            Layout.fillHeight: true
            Layout.preferredWidth: Math.max(300 * MeoTheme.globalScale, root.width * 0.34)
            type: "filled"
            interactive: false

            ColumnLayout {
                anchors.fill: parent
                spacing: MeoTheme.space16

                Item { Layout.fillHeight: true; Layout.preferredHeight: MeoTheme.space24 }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 116 * MeoTheme.globalScale
                    height: width
                    radius: MeoTheme.shapeExtraLarge
                    color: root.toneContainer(root.current.tone)

                    MeoIcon {
                        anchors.centerIn: parent
                        icon: root.current.icon
                        size: 58
                        color: root.toneContent(root.current.tone)
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationMedium1
                        }
                    }
                }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Welcome to Meo")
                    typeRole: "title"
                    typeSize: "big"
                    emphasized: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Fast. Private. Yours.")
                    typeRole: "title"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { Layout.fillHeight: true }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Step %1 of %2").arg(root.currentStep + 1).arg(root.steps.length)
                    typeRole: "label"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    horizontalAlignment: Text.AlignHCenter
                }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: MeoTheme.space8

                    Repeater {
                        model: root.steps.length
                        delegate: Rectangle {
                            required property int index
                            width: index === root.currentStep ? 26 * MeoTheme.globalScale : 8 * MeoTheme.globalScale
                            height: 8 * MeoTheme.globalScale
                            radius: height / 2
                            color: index === root.currentStep ? MeoTheme.primary : MeoTheme.outlineVariant

                            Behavior on width {
                                NumberAnimation {
                                    duration: MeoTheme.reduceMotion ? 0 : MeoTheme.motionDurationShort4
                                    easing.type: Easing.BezierSpline; easing.bezierCurve: MeoTheme.motionEasingEmphasizedDecelerate
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: MeoTheme.space8 }
            }
        }

        MeoCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            type: "elevated"
            interactive: false

            Flickable {
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: contentColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: contentColumn
                    width: parent.width
                    spacing: MeoTheme.space16

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.compact
                        spacing: MeoTheme.space12

                        Rectangle {
                            width: 64 * MeoTheme.globalScale
                            height: width
                            radius: MeoTheme.shapeLarge
                            color: root.toneContainer(root.current.tone)
                            MeoIcon {
                                anchors.centerIn: parent
                                icon: root.current.icon
                                size: 32
                                color: root.toneContent(root.current.tone)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MeoTheme.space4
                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Welcome to Meo")
                                typeRole: "title"
                                typeSize: "large"
                                emphasized: true
                            }
                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Step %1 of %2").arg(root.currentStep + 1).arg(root.steps.length)
                                typeRole: "label"
                                typeSize: "medium"
                                color: MeoTheme.contentOnSurfaceVariant
                            }
                        }
                    }

                    MeoText {
                        Layout.fillWidth: true
                        text: root.current.title
                        typeRole: "title"
                        typeSize: root.compact ? "large" : "big"
                        emphasized: true
                        wrapMode: Text.WordWrap
                    }

                    MeoText {
                        Layout.fillWidth: true
                        text: root.current.description
                        typeRole: "body"
                        typeSize: "large"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }

                    MeoCard {
                        Layout.fillWidth: true
                        type: "outlined"
                        visible: root.currentStep === 0
                        Accessible.name: qsTr("Installed system status")
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10 * MeoTheme.globalScale
            
                            Repeater {
                                model: [
                                    {"icon": "computer", "title": qsTr("Environment"), "value": root.welcomeState.runtimeEnvironment === "installed" ? qsTr("Installed system") : qsTr("Unknown")},
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
                        Layout.fillWidth: true
                        type: "outlined"
                        visible: root.currentStep === 1
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
                                MeoText {
                                    text: root.networkCheckValue
                                    typeRole: "label"
                                    typeSize: "small"
                                    emphasized: true
                                    color: root.networkBackend.internetAvailable
                                           ? MeoTheme.primary
                                           : root.networkBackend.connectivityState === "portal"
                                             ? MeoTheme.tertiary
                                             : MeoTheme.contentOnSurfaceVariant
                                }
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
                        Layout.fillWidth: true
                        type: "filled"
                        visible: root.currentStep === 2
                        interactive: false

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: MeoTheme.space12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MeoTheme.space12
                                MeoIcon {
                                    icon: "fingerprint"
                                    size: 26
                                    color: root.fingerprintBackend.devicePresent
                                           ? MeoTheme.primary
                                           : MeoTheme.contentOnSurfaceVariant
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2 * MeoTheme.globalScale
                                    MeoText {
                                        Layout.fillWidth: true
                                        text: root.fingerprintBackend.devicePresent
                                              ? root.fingerprintBackend.deviceLabel
                                              : qsTr("No compatible fingerprint reader detected")
                                        typeRole: "title"
                                        typeSize: "small"
                                        emphasized: true
                                        wrapMode: Text.WordWrap
                                    }
                                    MeoText {
                                        Layout.fillWidth: true
                                        text: root.fingerprintBackend.supportSummary
                                        typeRole: "body"
                                        typeSize: "small"
                                        color: MeoTheme.contentOnSurfaceVariant
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                Row {
                                    spacing: MeoTheme.space4
                                    MeoIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: root.fingerprintBackend.devicePresent ? "check_circle" : "info"
                                        size: 18
                                        color: root.fingerprintBackend.devicePresent
                                               ? MeoTheme.primary
                                               : MeoTheme.contentOnSurfaceVariant
                                    }
                                    MeoText {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: root.fingerprintBackend.devicePresent ? qsTr("Available") : qsTr("Unavailable")
                                        typeRole: "label"
                                        typeSize: "medium"
                                        emphasized: true
                                        color: root.fingerprintBackend.devicePresent
                                               ? MeoTheme.primary
                                               : MeoTheme.contentOnSurfaceVariant
                                    }
                                }
                            }

                            MeoText {
                                Layout.fillWidth: true
                                text: root.lockScreenFingerprintDefault
                                      ? qsTr("KScreenLocker can use fingerprint after enrollment. Password fallback remains available.")
                                      : qsTr("Fingerprint is not enabled for the lock screen by default. Review hardware settings before changing authentication.")
                                typeRole: "label"
                                typeSize: "medium"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MeoCard {
                        Layout.fillWidth: true
                        type: "outlined"
                        visible: root.currentStep === 5
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
                                MeoText {
                                    text: root.accountCheckValue
                                    typeRole: "label"
                                    typeSize: "small"
                                    emphasized: true
                                    color: root.accountBackend.signedIn ? MeoTheme.primary : MeoTheme.contentOnSurfaceVariant
                                }
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

                    MeoSettingsGroup {
                        Layout.fillWidth: true
                        title: root.currentStep === root.steps.length - 1
                               ? qsTr("Keep exploring")
                               : qsTr("Open the real system control")
                        subtitle: root.currentStep === root.steps.length - 1
                                  ? qsTr("These shortcuts stay available after Welcome closes.")
                                  : qsTr("Welcome only guides you. The owning Meo/KDE page applies the change.")
                        model: root.current.links.map(link => ({
                            "title": link.text,
                            "subtitle": "",
                            "icon": link.icon,
                            "tone": root.current.tone,
                            "route": link.route,
                            "trailingKind": "navigation"
                        }))
                        onRowActivated: (index, row) => root.openRoute(row.route)
                    }

                    MeoCard {
                        Layout.fillWidth: true
                        type: "outlined"
                        visible: root.currentStep === 3
                        interactive: false

                        RowLayout {
                            anchors.fill: parent
                            spacing: MeoTheme.space12
                            MeoIcon { icon: "verified_user"; size: 24; color: MeoTheme.primary }
                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Display layout stays with KScreen. Power profiles and suspend policy stay with PowerDevil. Welcome does not create parallel settings.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MeoCard {
                        Layout.fillWidth: true
                        type: "outlined"
                        visible: root.currentStep === 4
                        interactive: false

                        RowLayout {
                            anchors.fill: parent
                            spacing: MeoTheme.space12
                            MeoIcon { icon: "restore"; size: 24; color: MeoTheme.secondary }
                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Check recovery before risky changes. Protected system actions keep their existing authorization and recovery owners.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Column {
                        Layout.fillWidth: true
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

                    MeoProgressBar {
                        Layout.fillWidth: true
                        visible: root.compact
                        value: (root.currentStep + 1) / root.steps.length
                        Accessible.name: qsTr("Welcome progress")
                        Accessible.description: qsTr("Step %1 of %2: %3")
                                                .arg(root.currentStep + 1)
                                                .arg(root.steps.length)
                                                .arg(root.current.title)
                    }

                    Item { Layout.preferredHeight: MeoTheme.space8 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space8

                        MeoButton {
                            text: qsTr("Skip for now")
                            type: "text"
                            visible: root.currentStep < root.steps.length - 1
                            onClicked: root.welcomeState.complete()
                        }

                        Item { Layout.fillWidth: true }

                        MeoButton {
                            text: qsTr("Back")
                            type: "text"
                            visible: root.currentStep > 0
                            onClicked: --root.currentStep
                        }

                        MeoButton {
                            text: root.currentStep === root.steps.length - 1
                                  ? qsTr("Finish")
                                  : qsTr("Next")
                            icon.name: root.currentStep === root.steps.length - 1
                                       ? "check"
                                       : "arrow_forward"
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
        }
    }
}
