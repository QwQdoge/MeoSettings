import QtQuick
import QtQuick.Controls
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    property string expandedPolicyId: ""
    property int pendingLidAction: -1
    property bool pendingLidInhibit: true
    property bool batteryEditorOpen: false
    property int pendingBatteryAction: -1
    property var pendingPolicy: null

    readonly property var timeoutMinutes: [0, 1, 2, 5, 10, 15, 30, 60, 120, 180]
    readonly property var timeoutChoices: timeoutMinutes.map(value => root.timeoutTitle(value))
    readonly property var lidActions: [0]
        .concat(PowerPolicyBackend.canSuspend ? [1] : [])
        .concat(PowerPolicyBackend.canHibernate ? [2] : [])
        .concat([32, 64, 8])
    readonly property var batteryActions: [-1, 0]
        .concat(PowerPolicyBackend.canSuspend ? [1] : [])
        .concat(PowerPolicyBackend.canHibernate ? [2] : [])
        .concat([8])

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

    function timeoutTitle(minutes) {
        if (minutes < 0)
            return qsTr("System default")
        if (minutes === 0)
            return qsTr("Never")
        if (minutes === 1)
            return qsTr("1 minute")
        return qsTr("%1 minutes").arg(minutes)
    }

    function policyForId(id) {
        for (let index = 0; index < PowerPolicyBackend.policies.length; ++index) {
            const policy = PowerPolicyBackend.policies[index]
            if (policy.id === id)
                return policy
        }
        return null
    }

    function policyIcon(id) {
        if (id === "AC")
            return "power"
        if (id === "LowBattery")
            return "battery_saver"
        return "battery_full"
    }

    function policyTone(id) {
        if (id === "AC")
            return "primary"
        if (id === "LowBattery")
            return "tertiary"
        return "secondary"
    }

    function policySummary(policy) {
        if (!policy)
            return ""
        const parts = [
            qsTr("Screen: %1").arg(root.timeoutTitle(policy.screenMinutes))
        ]
        if (PowerPolicyBackend.canSuspend)
            parts.push(qsTr("Sleep: %1").arg(root.timeoutTitle(policy.sleepMinutes)))
        if (PowerPolicyBackend.lidPresent)
            parts.push(qsTr("Lid: %1").arg(policy.lidAction < 0
                                            ? qsTr("System default")
                                            : root.actionTitle(policy.lidAction)))
        return parts.join(" · ")
    }

    function openPolicyEditor(policyId) {
        const policy = root.policyForId(policyId)
        if (!policy)
            return
        root.pendingLidAction = policy.lidAction
        root.pendingLidInhibit = policy.inhibitLidWithMonitor
        root.batteryEditorOpen = false
        root.expandedPolicyId = policyId
    }

    function closePolicyEditor() {
        root.expandedPolicyId = ""
    }

    function openBatteryEditor() {
        root.expandedPolicyId = ""
        root.pendingBatteryAction = PowerPolicyBackend.batteryPolicy.action
        lowLevel.text = String(PowerPolicyBackend.batteryPolicy.lowPercent)
        criticalLevel.text = String(PowerPolicyBackend.batteryPolicy.criticalPercent)
        root.batteryEditorOpen = true
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

    readonly property var selectedPolicy: root.policyForId(root.expandedPolicyId)

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
                "tone": profile === "performance" ? "tertiary" : "primary",
                "trailingKind": "radio",
                "checked": Platform.activePowerProfile === profile
            })
        }
        return rows
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
    }].concat(PowerBackend.available ? [{
        "id": "battery-health",
        "title": qsTr("Battery health"),
        "subtitle": PowerBackend.healthPercent < 0
                    ? qsTr("Full and design capacity are not reported by this battery")
                    : qsTr("%1% of design capacity · %2 / %3 Wh")
                        .arg(PowerBackend.healthPercent.toFixed(1))
                        .arg(PowerBackend.fullEnergy.toFixed(1))
                        .arg(PowerBackend.designEnergy.toFixed(1)),
        "icon": "health_and_safety",
        "tone": "secondary",
        "interactive": false,
        "trailingKind": "status",
        "trailingText": PowerBackend.healthPercent < 0 ? qsTr("Unknown") : qsTr("Reported")
    }] : [])

    readonly property var policyRows: {
        const rows = []
        for (let index = 0; index < PowerPolicyBackend.policies.length; ++index) {
            const policy = PowerPolicyBackend.policies[index]
            rows.push({
                "id": "power-policy-" + policy.id,
                "policyId": policy.id,
                "title": policy.label,
                "subtitle": root.policySummary(policy),
                "icon": root.policyIcon(policy.id),
                "tone": root.policyTone(policy.id),
                "trailingKind": "navigation",
                "enabled": PowerPolicyBackend.available && !PowerPolicyBackend.busy
            })
        }
        return rows
    }

    readonly property var batteryPolicyRows: [{
        "id": "battery-policy",
        "title": qsTr("Low and critical battery"),
        "subtitle": qsTr("Warn at %1% · Critical at %2% · %3")
                    .arg(PowerPolicyBackend.batteryPolicy.lowPercent)
                    .arg(PowerPolicyBackend.batteryPolicy.criticalPercent)
                    .arg(root.actionTitle(PowerPolicyBackend.batteryPolicy.action)),
        "icon": "battery_alert",
        "tone": "tertiary",
        "trailingKind": "navigation",
        "enabled": PowerPolicyBackend.available
                   && PowerPolicyBackend.batteryPolicy.writable
                   && !PowerPolicyBackend.busy
    }]

    readonly property var sessionRows: {
        const rows = [{
            "id": "lock-screen",
            "title": qsTr("Lock screen now"),
            "subtitle": qsTr("Keep applications open and lock this session"),
            "icon": "lock",
            "tone": "neutral",
            "trailingKind": "action",
            "actionText": qsTr("Lock")
        }]
        if (!SessionActions.scheduled) {
            rows.push({
                "id": "schedule-logout",
                "title": qsTr("Sign out in 30 seconds"),
                "subtitle": qsTr("Show a countdown so you can cancel before the session ends"),
                "icon": "logout",
                "tone": "tertiary",
                "trailingKind": "action",
                "actionText": qsTr("Schedule"),
                "enabled": SessionActions.available
            })
        }
        return rows
    }

    MeoSettingsTaskSheet {
        id: confirmPolicy
        popupParent: Overlay.overlay
        title: qsTr("Apply automatic power behavior?")
        subtitle: qsTr("The desktop will use this action automatically. Save work before choosing shutdown; doing nothing at critical battery may lose work when power runs out.")
        acceptText: qsTr("Apply")
        rejectText: qsTr("Cancel")
        onAccepted: {
            const request = root.pendingPolicy
            if (!request)
                return
            if (request.kind === "lid")
                PowerPolicyBackend.setLidPolicy(request.profile, request.action, request.inhibit)
            else
                PowerPolicyBackend.setBatteryPolicy(request.low, request.critical, request.action)
            root.pendingPolicy = null
        }
        onRejected: root.pendingPolicy = null
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 820 * MeoTheme.globalScale
        expandedWidth: 900 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Power & battery")
        subtitle: qsTr("Battery status, power modes, screen sleep, lid behavior, and session actions")

        Column {
            width: parent.width
            visible: Platform.lastError !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("This power setting could not be applied")
                text: qsTr("Check the device power state, then try again.")
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

        MeoBanner {
            width: parent.width
            visible: PowerPolicyBackend.error !== ""
            title: qsTr("Power policy needs attention")
            text: PowerPolicyBackend.error
            icon: "error"
            tone: "error"
        }

        MeoCard {
            width: parent.width
            type: "filled"

            Row {
                width: parent.width
                spacing: 16 * MeoTheme.globalScale

                MeoIcon {
                    icon: PowerBackend.available
                          ? (PowerBackend.charging ? "battery_charging_full" : "battery_full")
                          : "bolt"
                    size: 32
                    color: MeoTheme.primary
                }

                Column {
                    width: parent.width - 48 * MeoTheme.globalScale
                    spacing: 4 * MeoTheme.globalScale

                    MeoText {
                        width: parent.width
                        text: PowerBackend.available
                              ? PowerBackend.summary
                              : (Platform.powerProfilesAvailable
                                 ? qsTr("Current mode: %1").arg(root.profileTitle(Platform.activePowerProfile))
                                 : qsTr("Power controls"))
                        typeRole: "title"
                        typeSize: "medium"
                        emphasized: true
                        wrapMode: Text.WordWrap
                    }

                    MeoText {
                        width: parent.width
                        text: PowerPolicyBackend.busy
                              ? qsTr("Applying power settings…")
                              : (Platform.powerProfilesAvailable
                                 ? qsTr("Power mode: %1").arg(root.profileTitle(Platform.activePowerProfile))
                                 : qsTr("Power modes are not exposed by this device"))
                        typeRole: "body"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: Platform.powerProfilesAvailable
            title: qsTr("Power mode")
            subtitle: qsTr("Choose how the system balances responsiveness and energy use")
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
                    text: qsTr("This device does not expose system power modes. Battery, screen, sleep, and session controls can still be available.")
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
            subtitle: qsTr("Current state and battery capacity reported by the device")
            model: root.batteryRows
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Screen, sleep & lid")
            subtitle: PowerPolicyBackend.available
                      ? qsTr("Choose how the computer behaves when plugged in, on battery, or low on battery")
                      : qsTr("The desktop power service is unavailable")
            model: root.policyRows
            onRowActivated: (index, row) => {
                if (row.policyId)
                    root.openPolicyEditor(row.policyId)
            }
        }

        MeoCard {
            width: parent.width
            visible: root.selectedPolicy !== null
            type: "filled"

            Column {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    spacing: 8 * MeoTheme.globalScale

                    MeoButton {
                        text: qsTr("Back")
                        type: "text"
                        size: "s"
                        icon.name: "arrow_back"
                        onClicked: root.closePolicyEditor()
                    }

                    Item {
                        width: Math.max(0, parent.width - policyEditorTitle.implicitWidth - 96 * MeoTheme.globalScale)
                        height: 1
                    }

                    MeoText {
                        id: policyEditorTitle
                        text: root.selectedPolicy ? root.selectedPolicy.label : ""
                        typeRole: "label"
                        typeSize: "medium"
                        emphasized: true
                        color: MeoTheme.contentOnSurfaceVariant
                    }
                }

                MeoText {
                    width: parent.width
                    text: qsTr("Automatic behavior")
                    typeRole: "title"
                    typeSize: "medium"
                    emphasized: true
                }

                MeoText {
                    width: parent.width
                    text: root.selectedPolicy ? root.policySummary(root.selectedPolicy) : ""
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoExposedDropdown {
                    width: parent.width
                    label: qsTr("Turn off screen after")
                    model: root.timeoutChoices
                    text: root.selectedPolicy ? root.timeoutTitle(root.selectedPolicy.screenMinutes) : ""
                    enabled: root.selectedPolicy
                             && PowerPolicyBackend.available
                             && root.selectedPolicy.screenWritable
                             && !PowerPolicyBackend.busy
                    onSelected: (index, value) => {
                        if (root.selectedPolicy)
                            PowerPolicyBackend.setTimeout(root.selectedPolicy.id, "screen", root.timeoutMinutes[index])
                    }
                }

                MeoExposedDropdown {
                    width: parent.width
                    label: qsTr("Sleep when idle after")
                    model: root.timeoutChoices
                    text: root.selectedPolicy ? root.timeoutTitle(root.selectedPolicy.sleepMinutes) : ""
                    enabled: root.selectedPolicy
                             && PowerPolicyBackend.available
                             && root.selectedPolicy.sleepWritable
                             && PowerPolicyBackend.canSuspend
                             && !PowerPolicyBackend.busy
                    onSelected: (index, value) => {
                        if (root.selectedPolicy)
                            PowerPolicyBackend.setTimeout(root.selectedPolicy.id, "sleep", root.timeoutMinutes[index])
                    }
                }

                Column {
                    width: parent.width
                    visible: PowerPolicyBackend.lidPresent
                    spacing: 10 * MeoTheme.globalScale

                    MeoExposedDropdown {
                        width: parent.width
                        label: qsTr("When closing the lid")
                        model: root.lidActions.map(value => root.actionTitle(value))
                        text: root.pendingLidAction < 0
                              ? qsTr("System default")
                              : root.actionTitle(root.pendingLidAction)
                        enabled: root.selectedPolicy
                                 && root.selectedPolicy.lidWritable
                                 && !PowerPolicyBackend.busy
                        onSelected: (index, value) => root.pendingLidAction = root.lidActions[index]
                    }

                    MeoSettingsGroup {
                        width: parent.width
                        model: [{
                            "title": qsTr("Keep running with an external monitor"),
                            "subtitle": qsTr("Ignore the lid action while an external display is connected"),
                            "trailingKind": "toggle",
                            "checked": root.pendingLidInhibit,
                            "enabled": root.selectedPolicy
                                       && root.selectedPolicy.lidWritable
                                       && !PowerPolicyBackend.busy
                        }]
                        onRowToggled: (index, checked, row) => root.pendingLidInhibit = checked
                    }

                    MeoButton {
                        text: qsTr("Apply lid behavior")
                        type: "tonal"
                        enabled: root.selectedPolicy
                                 && root.selectedPolicy.lidWritable
                                 && !PowerPolicyBackend.busy
                                 && root.lidActions.indexOf(root.pendingLidAction) >= 0
                        onClicked: {
                            root.pendingPolicy = {
                                "kind": "lid",
                                "profile": root.selectedPolicy.id,
                                "action": root.pendingLidAction,
                                "inhibit": root.pendingLidInhibit
                            }
                            confirmPolicy.open()
                        }
                    }
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: PowerBackend.available
            title: qsTr("Battery protection")
            subtitle: qsTr("Low-battery warning and automatic action at the critical level")
            model: root.batteryPolicyRows
            onRowActivated: (index, row) => {
                if (row.id === "battery-policy")
                    root.openBatteryEditor()
            }
        }

        MeoCard {
            width: parent.width
            visible: root.batteryEditorOpen && PowerBackend.available
            type: "filled"

            Column {
                id: batteryPolicy
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                Row {
                    width: parent.width
                    MeoButton {
                        text: qsTr("Back")
                        type: "text"
                        size: "s"
                        icon.name: "arrow_back"
                        onClicked: root.batteryEditorOpen = false
                    }
                }

                MeoText {
                    width: parent.width
                    text: qsTr("Low and critical battery")
                    typeRole: "title"
                    typeSize: "medium"
                    emphasized: true
                }

                MeoText {
                    width: parent.width
                    text: qsTr("The critical level must be lower than the low-battery warning level.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                Grid {
                    width: parent.width
                    columns: root.isCompact ? 1 : 2
                    columnSpacing: 12 * MeoTheme.globalScale
                    rowSpacing: 12 * MeoTheme.globalScale

                    MeoTextField {
                        id: lowLevel
                        width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                        label: qsTr("Low-battery warning (%)")
                        text: String(PowerPolicyBackend.batteryPolicy.lowPercent)
                        enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                        validator: IntValidator { bottom: 2; top: 100 }
                    }

                    MeoTextField {
                        id: criticalLevel
                        width: root.isCompact ? parent.width : (parent.width - 12 * MeoTheme.globalScale) / 2
                        label: qsTr("Critical battery (%)")
                        text: String(PowerPolicyBackend.batteryPolicy.criticalPercent)
                        enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                        validator: IntValidator { bottom: 1; top: 99 }
                    }
                }

                MeoExposedDropdown {
                    width: parent.width
                    label: qsTr("At critical battery")
                    model: root.batteryActions.map(value => root.actionTitle(value))
                    text: root.actionTitle(root.pendingBatteryAction)
                    enabled: PowerPolicyBackend.batteryPolicy.writable && !PowerPolicyBackend.busy
                    onSelected: (index, value) => root.pendingBatteryAction = root.batteryActions[index]
                }

                MeoBanner {
                    width: parent.width
                    visible: lowLevel.text !== ""
                             && criticalLevel.text !== ""
                             && lowLevel.acceptableInput
                             && criticalLevel.acceptableInput
                             && Number(criticalLevel.text) >= Number(lowLevel.text)
                    title: qsTr("Critical battery must be lower")
                    text: qsTr("Choose a critical percentage below the low-battery warning percentage.")
                    tone: "tonal"
                    icon: "info"
                }

                MeoButton {
                    text: qsTr("Apply battery behavior")
                    type: "tonal"
                    enabled: PowerPolicyBackend.available
                             && PowerPolicyBackend.batteryPolicy.writable
                             && !PowerPolicyBackend.busy
                             && lowLevel.acceptableInput
                             && criticalLevel.acceptableInput
                             && Number(criticalLevel.text) < Number(lowLevel.text)
                             && root.batteryActions.indexOf(root.pendingBatteryAction) >= 0
                    onClicked: {
                        root.pendingPolicy = {
                            "kind": "battery",
                            "low": Number(lowLevel.text),
                            "critical": Number(criticalLevel.text),
                            "action": root.pendingBatteryAction
                        }
                        confirmPolicy.open()
                    }
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
            title: qsTr("Session")
            subtitle: qsTr("Lock or sign out without mixing session actions with power policies")
            model: root.sessionRows
            onRowActionTriggered: (index, row) => {
                if (row.id === "lock-screen")
                    Platform.lockScreen()
                else if (row.id === "schedule-logout")
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
            visible: KcmBridge.isAvailable("kcm_powerdevilprofilesconfig")
            title: qsTr("Advanced")
            subtitle: qsTr("Device-specific and less common PowerDevil options")
            model: [{
                "title": qsTr("Advanced Power Management"),
                "subtitle": qsTr("Charge limits, sleep modes, and device-specific power options"),
                "icon": "tune",
                "tone": "neutral",
                "route": "kcm:kcm_powerdevilprofilesconfig",
                "trailingKind": "navigation"
            }]
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }
    }
}
