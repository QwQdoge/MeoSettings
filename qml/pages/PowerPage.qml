import QtQuick
import QtQuick.Controls
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    readonly property var lidActions: [0].concat(PowerPolicyBackend.canSuspend ? [1] : []).concat(PowerPolicyBackend.canHibernate ? [2] : []).concat([32, 64, 8])
    readonly property var batteryActions: [-1, 0].concat(PowerPolicyBackend.canSuspend ? [1] : []).concat(PowerPolicyBackend.canHibernate ? [2] : []).concat([8])
    function actionTitle(action) {
        switch (action) {
        case -1: return qsTr("Keep current or system default")
        case 0: return qsTr("Do nothing")
        case 1: return qsTr("Sleep")
        case 2: return qsTr("Hibernate")
        case 8: return qsTr("Shut down")
        case 32: return qsTr("Lock screen")
        case 64: return qsTr("Turn off screen")
        default: return qsTr("Advanced action (%1)").arg(action)
        }
    }
    property var pendingPolicy: null
    MeoSettingsTaskSheet {
        id: confirmPolicy; popupParent: Overlay.overlay
        title: qsTr("Apply automatic power behavior?")
        subtitle: qsTr("The desktop will use this action when the lid closes or the battery reaches its critical level. Save work before choosing shutdown; doing nothing at critical battery may lose work when power runs out.")
        acceptText: qsTr("Apply"); rejectText: qsTr("Cancel")
        onAccepted: {
            const request = root.pendingPolicy
            if (!request) return
            if (request.kind === "lid") PowerPolicyBackend.setLidPolicy(request.profile, request.action, request.inhibit)
            else PowerPolicyBackend.setBatteryPolicy(request.low, request.critical, request.action)
            root.pendingPolicy = null
        }
        onRejected: root.pendingPolicy = null
    }

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
        return qsTr("Power mode provided by this device")
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
        subtitle: qsTr("Choose a power mode and review battery status. More advanced schedules are available in system power settings.")

        Column {
            width: parent.width
            visible: Platform.lastError !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("This power setting could not be applied")
                text: qsTr("Check the device power state, then try the setting again.")
                icon: "error"
                tone: "error"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(Platform.lastError)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
            MeoButton {
                text: qsTr("Dismiss")
                type: "text"
                size: "s"
                onClicked: Platform.clearError()
            }
        }

        MeoBanner { width: parent.width; visible: PowerPolicyBackend.error !== ""; title: qsTr("Power schedule needs attention"); text: PowerPolicyBackend.error; tone: "error" }
        Repeater {
            model: PowerPolicyBackend.policies
            delegate: Column {
                required property var modelData
                width: parent.width; spacing: MeoTheme.space12
                readonly property var minutes: [0, 1, 2, 5, 10, 15, 30, 60, 120, 180]
                readonly property var choices: minutes.map(value => value === 0 ? qsTr("Never") : qsTr("%1 minutes").arg(value))
                MeoExposedDropdown {
                    width: parent.width; label: qsTr("%1: turn off screen").arg(modelData.label)
                    model: parent.choices
                    text: modelData.screenMinutes < 0 ? qsTr("System default") : modelData.screenMinutes === 0 ? qsTr("Never") : qsTr("%1 minutes").arg(modelData.screenMinutes)
                    enabled: PowerPolicyBackend.available && modelData.screenWritable && !PowerPolicyBackend.busy
                    onSelected: (index, value) => PowerPolicyBackend.setTimeout(modelData.id, "screen", parent.minutes[index])
                }
                MeoExposedDropdown {
                    width: parent.width; label: qsTr("%1: sleep when idle").arg(modelData.label)
                    model: parent.choices
                    text: modelData.sleepMinutes < 0 ? qsTr("System default or advanced policy") : modelData.sleepMinutes === 0 ? qsTr("Never") : qsTr("%1 minutes").arg(modelData.sleepMinutes)
                    enabled: PowerPolicyBackend.available && modelData.sleepWritable && PowerPolicyBackend.canSuspend && !PowerPolicyBackend.busy
                    onSelected: (index, value) => PowerPolicyBackend.setTimeout(modelData.id, "sleep", parent.minutes[index])
                }
                Column {
                    id: lidPolicy
                    width: parent.width; spacing: MeoTheme.space8
                    visible: PowerPolicyBackend.lidPresent
                    property int action: modelData.lidAction
                    property bool inhibit: modelData.inhibitLidWithMonitor
                    MeoExposedDropdown {
                        width: parent.width; label: qsTr("%1: when closing the lid").arg(modelData.label)
                        model: root.lidActions.map(value => root.actionTitle(value))
                        text: lidPolicy.action < 0 ? qsTr("System default") : root.actionTitle(lidPolicy.action)
                        enabled: modelData.lidWritable && !PowerPolicyBackend.busy
                        onSelected: (index, value) => lidPolicy.action = root.lidActions[index]
                    }
                    MeoSettingsGroup {
                        width: parent.width
                        model: [{"title": qsTr("Keep running with an external monitor"), "trailingKind": "toggle", "checked": lidPolicy.inhibit, "enabled": modelData.lidWritable && !PowerPolicyBackend.busy}]
                        onRowToggled: (index, checked, row) => lidPolicy.inhibit = checked
                    }
                    MeoButton {
                        text: qsTr("Apply lid behavior"); type: "tonal"
                        enabled: modelData.lidWritable && !PowerPolicyBackend.busy && root.lidActions.indexOf(lidPolicy.action) >= 0
                        onClicked: {
                            root.pendingPolicy = {"kind": "lid", "profile": modelData.id, "action": lidPolicy.action, "inhibit": lidPolicy.inhibit}
                            confirmPolicy.open()
                        }
                    }
                }
            }
        }

        Column {
            id: batteryPolicy
            width: parent.width; spacing: MeoTheme.space12
            visible: PowerBackend.available
            property int action: PowerPolicyBackend.batteryPolicy.action
            MeoText { width: parent.width; text: qsTr("Low and critical battery"); typeRole: "title"; typeSize: "medium" }
            MeoTextField {
                id: lowLevel; width: parent.width; label: qsTr("Low-battery warning (%)")
                text: String(PowerPolicyBackend.batteryPolicy.lowPercent)
                enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                validator: IntValidator { bottom: 2; top: 100 }
            }
            MeoTextField {
                id: criticalLevel; width: parent.width; label: qsTr("Critical battery (%)")
                text: String(PowerPolicyBackend.batteryPolicy.criticalPercent)
                enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                validator: IntValidator { bottom: 1; top: 99 }
            }
            MeoExposedDropdown {
                width: parent.width; label: qsTr("At critical battery")
                model: root.batteryActions.map(value => root.actionTitle(value))
                text: root.actionTitle(batteryPolicy.action)
                enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                onSelected: (index, value) => batteryPolicy.action = root.batteryActions[index]
            }
            MeoButton {
                text: qsTr("Apply battery behavior"); type: "tonal"
                enabled: PowerPolicyBackend.available && PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy && lowLevel.acceptableInput && criticalLevel.acceptableInput && Number(criticalLevel.text) < Number(lowLevel.text) && root.batteryActions.indexOf(batteryPolicy.action) >= 0
                onClicked: {
                    root.pendingPolicy = {"kind": "battery", "low": Number(lowLevel.text), "critical": Number(criticalLevel.text), "action": batteryPolicy.action}
                    confirmPolicy.open()
                }
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: Platform.powerProfileDegradedReason !== ""
            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("Power mode needs attention")
            Accessible.description: Platform.powerProfileDegradedReason

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
            subtitle: qsTr("Battery level and charging status from this device")
            model: root.batteryRows.concat(PowerBackend.available ? [{
                "title": qsTr("Battery health"), "subtitle": PowerBackend.healthPercent < 0 ? qsTr("The battery does not report full and design capacity") : qsTr("%1% of design capacity · %2 / %3 Wh").arg(PowerBackend.healthPercent.toFixed(1)).arg(PowerBackend.fullEnergy.toFixed(1)).arg(PowerBackend.designEnergy.toFixed(1)),
                "icon": "battery_full", "interactive": false, "trailingKind": "status", "trailingText": PowerBackend.healthPercent < 0 ? qsTr("Unknown") : qsTr("Reported by device")
            }] : [])
        }

        MeoSettingsGroup {
            width: parent.width
            visible: !SessionActions.scheduled
            title: qsTr("Session exit")
            subtitle: qsTr("This only signs you out. It does not shut down your computer")
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
            Accessible.role: Accessible.StatusBar
            Accessible.name: qsTr("Sign-out countdown")
            Accessible.description: qsTr("%1 seconds remaining").arg(SessionActions.remainingSeconds)

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
                    Accessible.description: qsTr("%1 seconds remaining").arg(SessionActions.remainingSeconds)
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

        Column {
            width: parent.width
            visible: !SessionActions.available && SessionActions.error !== ""
            spacing: MeoTheme.space4
            MeoBanner {
                width: parent.width
                title: qsTr("Scheduled sign-out is unavailable")
                text: qsTr("Check that the current session supports sign-out, then try again.")
                icon: "info"
                tone: "tonal"
            }
            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(SessionActions.error)
                Accessible.name: text
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Platform.powerProfilesAvailable
            title: qsTr("Power mode")
            subtitle: qsTr("The system applies the power mode you choose")
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
                    text: qsTr("Power modes are not available on this device. Battery status and screen lock can still be available.")
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
            subtitle: qsTr("Use system power settings for charge limits and other device-specific options.")
            model: [{
                "title": qsTr("Advanced Power Management"),
                "subtitle": qsTr("Charge limits, sleep modes, and device-specific power options"),
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
