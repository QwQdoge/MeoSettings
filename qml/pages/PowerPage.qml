import QtQuick
import QtQuick.Controls
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function profileTitle(profile) {
        if (profile === "performance")
            return qsTr("Performance")
        if (profile === "power-saver")
            return qsTr("Power saver")
        if (profile === "balanced")
            return qsTr("Balanced")
        return profile
    }

    function profileDescription(profile) {
        if (profile === "performance")
            return qsTr("Prioritise speed and responsiveness")
        if (profile === "power-saver")
            return qsTr("Reduce energy use and background activity")
        if (profile === "balanced")
            return qsTr("Balance performance and battery life")
        return qsTr("System-provided power profile")
    }

    function profileIcon(profile) {
        if (profile === "performance")
            return "speed"
        if (profile === "power-saver")
            return "battery_saver"
        return "balance"
    }

    readonly property var batteryRows: [{
        "id": "battery-status",
        "title": qsTr("Battery"),
        "subtitle": PowerBackend.available
                    ? PowerBackend.summary
                    : qsTr("No primary battery is reported by this device"),
        "icon": PowerBackend.charging ? "battery_charging_full" : "battery_full",
        "tone": "primary",
        "trailingKind": "status",
        "trailingText": PowerBackend.available ? PowerBackend.stateLabel : qsTr("Unavailable"),
        "statusTone": PowerBackend.charging ? "primary" : "neutral",
        "interactive": false
    }]

    readonly property var profileRows: {
        const rows = []
        for (let index = 0; index < Platform.powerProfiles.length; ++index) {
            const profile = Platform.powerProfiles[index]
            rows.push({
                "id": "power-profile-" + profile,
                "profile": profile,
                "title": root.profileTitle(profile),
                "subtitle": root.profileDescription(profile),
                "icon": root.profileIcon(profile),
                "tone": "primary",
                "trailingKind": "radio",
                "checked": Platform.activePowerProfile === profile
            })
        }
        return rows
    }

    readonly property var sessionRows: [{
        "id": "lock-screen",
        "title": qsTr("Lock screen now"),
        "subtitle": qsTr("Lock without closing applications"),
        "icon": "lock",
        "tone": "neutral",
        "trailingKind": "action",
        "actionText": qsTr("Lock")
    }]

    readonly property var scheduledLogoutRows: [{
        "id": "schedule-logout",
        "title": qsTr("Sign out in 30 seconds"),
        "subtitle": qsTr("Shows a countdown notification. You can cancel or sign out immediately."),
        "icon": "logout",
        "tone": "tertiary",
        "trailingKind": "action",
        "actionText": qsTr("Schedule")
    }]

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Power & battery")
        subtitle: qsTr("Manage the live power profile directly. Scheduling and hardware policy remain in the maintained PowerDevil module.")

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: Platform.lastError !== ""

            Column {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon { icon: "error"; size: 24; color: MeoTheme.error }
                    MeoText {
                        width: parent.width - 36 * MeoTheme.globalScale
                        text: Platform.lastError
                        typeRole: "body"
                        typeSize: "medium"
                        color: MeoTheme.error
                        wrapMode: Text.WordWrap
                    }
                }

                MeoButton {
                    text: qsTr("Dismiss")
                    type: "text"
                    size: "s"
                    onClicked: Platform.clearError()
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: Platform.powerProfileDegradedReason !== ""

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "warning"; size: 24; color: MeoTheme.tertiary }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: Platform.powerProfileDegradedReason
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Battery")
            subtitle: qsTr("Live primary-battery state from the device service")
            model: root.batteryRows
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !SessionActions.scheduled
            title: qsTr("Session exit")
            subtitle: qsTr("A deliberate session-only action; it does not shut down the computer")
            model: root.scheduledLogoutRows
            onRowActionTriggered: (index, row) => {
                if (row.id === "schedule-logout")
                    SessionActions.scheduleLogout(30)
            }
        }

        MeoCard {
            width: parent.width
            visible: SessionActions.scheduled
            type: "outlined"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 12 * MeoTheme.globalScale
                    MeoIcon { icon: "logout"; size: 24; color: MeoTheme.tertiary }
                    MeoText {
                        width: parent.width - 36 * MeoTheme.globalScale
                        text: qsTr("Signing out in %1 seconds").arg(SessionActions.remainingSeconds)
                        typeRole: "title"
                        typeSize: "medium"
                        wrapMode: Text.WordWrap
                    }
                }

                MeoProgressBar {
                    width: parent.width
                    value: Math.max(0, Math.min(1, SessionActions.remainingSeconds / 30))
                    type: "linear"
                    Accessible.name: qsTr("Sign-out countdown")
                }

                MeoText {
                    width: parent.width
                    text: qsTr("Your applications may still ask you to save work before the session closes.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8 * MeoTheme.globalScale
                    MeoButton {
                        text: qsTr("Cancel")
                        type: "tonal"
                        onClicked: SessionActions.cancel()
                    }
                    MeoButton {
                        text: qsTr("Sign out now")
                        type: "filled"
                        icon.name: "logout"
                        onClicked: SessionActions.executeNow()
                    }
                }
            }
        }

        MeoCard {
            width: parent.width
            visible: !SessionActions.available && SessionActions.error !== ""
            type: "outlined"
            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "info"; size: 24; color: MeoTheme.primary }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("Scheduled sign-out is unavailable: %1").arg(SessionActions.error)
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Platform.powerProfilesAvailable
            title: qsTr("Power mode")
            subtitle: qsTr("The selected profile is applied by the system power-profiles service")
            model: root.profileRows
            onRowToggled: (index, checked, row) => {
                if (checked && row.profile)
                    Platform.activePowerProfile = row.profile
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !Platform.powerProfilesAvailable

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale
                MeoIcon { icon: "info"; size: 24; color: MeoTheme.primary }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: qsTr("This system does not publish power profiles. Battery state and the screen-lock action below remain available when supported.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Session action")
            subtitle: qsTr("Lock the current desktop session without closing applications")
            model: root.sessionRows
            onRowActionTriggered: (index, row) => {
                if (row.id === "lock-screen")
                    Platform.lockScreen()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: KcmBridge.isAvailable("kcm_powerdevilprofilesconfig")
            title: qsTr("Advanced power policy")
            subtitle: qsTr("Use the maintained PowerDevil module for AC/battery schedules, thresholds, and other recovery-sensitive policy.")
            model: [{
                "title": qsTr("Advanced Power Management"),
                "subtitle": qsTr("Schedules, critical-battery actions, and device-specific power policy"),
                "icon": "tune",
                "tone": "neutral",
                "route": "kcm:kcm_powerdevilprofilesconfig",
                "trailingKind": "choice",
                "trailingText": qsTr("Advanced")
            }]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }
    }
}
