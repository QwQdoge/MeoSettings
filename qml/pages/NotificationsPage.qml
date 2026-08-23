import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI
import org.kde.notificationmanager as NotificationManager

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    // NotificationManager.Settings is Plasma's own persisted notification
    // authority (plasmanotifyrc).  It intentionally replaces neither the
    // notification server nor the history model: Plasma keeps those live
    // session responsibilities, while Settings owns the global preferences.
    NotificationManager.Settings {
        id: notificationSettings
        live: true
    }

    // Settings' public property notifications use one shared signal, while
    // individual setters only mark the underlying KConfig skeleton dirty.
    // Keep a local revision so every direct control redraws immediately after
    // a confirmed save as well as when another Plasma surface changes config.
    property int settingsRevision: 0
    property bool saveQueued: false

    readonly property bool manualDndEnabled: {
        settingsRevision
        const until = notificationSettings.notificationsInhibitedUntil
        const time = until instanceof Date ? until.getTime() : new Date(until).getTime()
        return !isNaN(time) && time > Date.now()
    }

    readonly property var dndRows: {
        settingsRevision
        return [
            {
                "id": "manual-dnd",
                "title": qsTr("Do Not Disturb"),
                "subtitle": root.manualDndEnabled
                            ? qsTr("New notifications are collected quietly until you turn this off")
                            : qsTr("Pause notification popups without disabling notification history"),
                "icon": "do_not_disturb_on",
                "tone": "primary",
                "trailingKind": "toggle",
                "checked": root.manualDndEnabled
            },
            {
                "id": "critical-in-dnd",
                "title": qsTr("Allow critical notification popups"),
                "subtitle": qsTr("Safety and urgent system notifications can still interrupt Do Not Disturb"),
                "icon": "priority_high",
                "tone": "error",
                "trailingKind": "toggle",
                "checked": notificationSettings.criticalPopupsInDoNotDisturbMode
            }
        ]
    }

    readonly property var popupRows: {
        settingsRevision
        const seconds = Math.max(1, Math.min(30, Math.round(notificationSettings.popupTimeout / 1000)))
        return [
            {
                "id": "popup-duration",
                "title": qsTr("Default popup duration"),
                "subtitle": qsTr("Used when an app does not provide its own notification timeout"),
                "icon": "timer",
                "tone": "secondary",
                "trailingKind": "slider",
                "from": 1,
                "to": 30,
                "value": seconds,
                "stepSize": 1,
                "discrete": true,
                "valueSuffix": qsTr(" s"),
                "trailingText": qsTr("%1 s").arg(seconds),
                "sliderSize": "s"
            },
            {
                "id": "popup-timeout-indicator",
                "title": qsTr("Show time remaining"),
                "subtitle": qsTr("Show a visual timeout indicator on notification popups"),
                "icon": "hourglass_top",
                "tone": "secondary",
                "trailingKind": "toggle",
                "checked": notificationSettings.showPopupTimeout
            },
            {
                "id": "low-priority-popups",
                "title": qsTr("Show low-priority popups"),
                "subtitle": qsTr("Low-priority notifications remain in history when this is off"),
                "icon": "notifications",
                "tone": "secondary",
                "trailingKind": "toggle",
                "checked": notificationSettings.lowPriorityPopups
            },
            {
                "id": "low-priority-history",
                "title": qsTr("Keep low-priority history"),
                "subtitle": qsTr("Keep low-priority notifications in the notification history"),
                "icon": "history",
                "tone": "secondary",
                "trailingKind": "toggle",
                "checked": notificationSettings.lowPriorityHistory
            },
            {
                "id": "popup-position",
                "title": qsTr("Popup position"),
                "subtitle": qsTr("Choose where Plasma places notification popups"),
                "icon": "crop_landscape",
                "tone": "secondary",
                "trailingKind": "dropdown",
                "options": [
                    qsTr("Near control"),
                    qsTr("Top left"),
                    qsTr("Top center"),
                    qsTr("Top right"),
                    qsTr("Bottom left"),
                    qsTr("Bottom center"),
                    qsTr("Bottom right")
                ],
                "currentIndex": Math.max(0, Math.min(6, Number(notificationSettings.popupPosition)))
            }
        ]
    }

    readonly property var automaticDndRows: {
        settingsRevision
        return [
            {
                "id": "dnd-fullscreen",
                "title": qsTr("During fullscreen apps"),
                "subtitle": qsTr("Automatically enter Do Not Disturb while a fullscreen window is focused"),
                "icon": "fullscreen",
                "tone": "tertiary",
                "trailingKind": "toggle",
                "checked": notificationSettings.inhibitNotificationsWhenFullscreen
            },
            {
                "id": "dnd-mirrored",
                "title": qsTr("When displays are mirrored"),
                "subtitle": qsTr("Automatically enter Do Not Disturb during mirrored presentations"),
                "icon": "present_to_all",
                "tone": "tertiary",
                "trailingKind": "toggle",
                "checked": notificationSettings.inhibitNotificationsWhenScreensMirrored
            }
        ]
    }

    readonly property var backgroundRows: {
        settingsRevision
        return [
            {
                "id": "jobs-in-notifications",
                "title": qsTr("Show background tasks"),
                "subtitle": qsTr("Show file transfers and other app jobs in notification history"),
                "icon": "sync",
                "tone": "primary",
                "trailingKind": "toggle",
                "checked": notificationSettings.jobsInNotifications
            },
            {
                "id": "permanent-job-popups",
                "title": qsTr("Keep task popups visible"),
                "subtitle": qsTr("Keep background-task popups visible until their work finishes"),
                "icon": "pending_actions",
                "tone": "primary",
                "trailingKind": "toggle",
                "checked": notificationSettings.permanentJobPopups,
                "enabled": notificationSettings.jobsInNotifications
            },
            {
                "id": "task-manager-badges",
                "title": qsTr("App notification badges"),
                "subtitle": qsTr("Show unread notification counts on supported taskbar apps"),
                "icon": "notifications_active",
                "tone": "primary",
                "trailingKind": "toggle",
                "checked": notificationSettings.badgesInTaskManager
            }
        ]
    }

    readonly property var applicationRows: {
        settingsRevision
        const applications = notificationSettings.knownApplications || []
        const rows = []
        for (let index = 0; index < applications.length; ++index) {
            const applicationId = applications[index]
            if (!applicationId)
                continue
            const behavior = Number(notificationSettings.applicationBehavior(applicationId))
            const popupAllowed = (behavior & NotificationManager.Settings.ShowPopups) !== 0
            const historyAllowed = (behavior & NotificationManager.Settings.ShowInHistory) !== 0
            rows.push({
                "id": applicationId,
                "title": applicationId,
                "subtitle": popupAllowed && historyAllowed
                            ? qsTr("Popups and history allowed")
                            : (popupAllowed ? qsTr("Popups allowed; history hidden")
                               : (historyAllowed ? qsTr("Popups hidden; history allowed")
                                  : qsTr("Popups and history hidden"))),
                "icon": "notifications",
                "tone": popupAllowed ? "primary" : "neutral",
                "trailingKind": "navigation"
            })
        }
        rows.sort((left, right) => left.title.localeCompare(right.title))
        return rows
    }

    function applicationAllows(applicationId, behavior) {
        return (Number(notificationSettings.applicationBehavior(applicationId)) & behavior) !== 0
    }

    function setApplicationBehavior(applicationId, behavior, enabled) {
        if (!applicationId)
            return
        const current = Number(notificationSettings.applicationBehavior(applicationId))
        const next = enabled ? (current | behavior) : (current & ~behavior)
        if (current === next)
            return
        notificationSettings.setApplicationBehavior(applicationId, next)
        persistNow()
    }

    function openApplicationRules(applicationId) {
        if (!applicationId)
            return
        applicationRules.applicationId = applicationId
        applicationRules.open()
    }

    function persistNow() {
        saveTimer.stop()
        saveQueued = false
        notificationSettings.save()
        settingsRevision += 1
    }

    function persistSoon() {
        saveQueued = true
        saveTimer.restart()
    }

    function setManualDnd(enabled) {
        if (enabled) {
            // Plasma's own notification applet represents manual DND as a
            // far-future time and clears it when the user turns DND off.
            // Use the same persisted contract so Quick Settings and the
            // notification center stay in sync without a private DND state.
            const until = new Date()
            until.setFullYear(until.getFullYear() + 1)
            notificationSettings.notificationsInhibitedUntil = until
        } else {
            notificationSettings.notificationsInhibitedUntil = undefined
        }
        persistNow()
    }

    function setSetting(id, checked) {
        switch (id) {
        case "critical-in-dnd":
            notificationSettings.criticalPopupsInDoNotDisturbMode = checked
            break
        case "popup-timeout-indicator":
            notificationSettings.showPopupTimeout = checked
            break
        case "low-priority-popups":
            notificationSettings.lowPriorityPopups = checked
            break
        case "low-priority-history":
            notificationSettings.lowPriorityHistory = checked
            break
        case "dnd-fullscreen":
            notificationSettings.inhibitNotificationsWhenFullscreen = checked
            break
        case "dnd-mirrored":
            notificationSettings.inhibitNotificationsWhenScreensMirrored = checked
            break
        case "jobs-in-notifications":
            notificationSettings.jobsInNotifications = checked
            break
        case "permanent-job-popups":
            notificationSettings.permanentJobPopups = checked
            break
        case "task-manager-badges":
            notificationSettings.badgesInTaskManager = checked
            break
        default:
            return
        }
        persistNow()
    }

    Timer {
        id: saveTimer
        interval: 220
        repeat: false
        onTriggered: root.persistNow()
    }

    Connections {
        target: notificationSettings
        function onSettingsChanged() {
            root.settingsRevision += 1
        }
    }

    Component.onDestruction: {
        if (notificationSettings.dirty)
            notificationSettings.save()
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Notifications")
        subtitle: qsTr("Control global behavior and per-app rules directly through Plasma’s real notification settings.")

        MeoCard {
            width: parent.width
            type: "outlined"

            Row {
                width: parent.width
                spacing: 12 * MeoTheme.globalScale

                MeoIcon {
                    icon: root.manualDndEnabled ? "do_not_disturb_on" : "notifications"
                    size: 24
                    color: root.manualDndEnabled ? MeoTheme.primary : MeoTheme.contentOnSurfaceVariant
                }
                MeoText {
                    width: parent.width - 36 * MeoTheme.globalScale
                    text: root.manualDndEnabled
                          ? qsTr("Do Not Disturb is on. Notification history stays available in the Meo status center.")
                          : qsTr("Notification preferences apply to the Meo status center, quick settings, and Plasma notification popups.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Focus")
            subtitle: qsTr("Temporarily quiet notifications without deleting or hiding their history")
            model: root.dndRows
            onRowToggled: (index, checked, row) => {
                if (row.id === "manual-dnd")
                    root.setManualDnd(checked)
                else
                    root.setSetting(row.id, checked)
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Popups")
            subtitle: qsTr("Choose how global notification popups appear")
            model: root.popupRows
            onRowToggled: (index, checked, row) => root.setSetting(row.id, checked)
            onRowSliderMoved: (index, value, row) => {
                if (row.id !== "popup-duration")
                    return
                notificationSettings.popupTimeout = Math.round(value) * 1000
                root.persistSoon()
                root.settingsRevision += 1
            }
            onRowDropdownSelected: (index, optionIndex, value, row) => {
                if (row.id !== "popup-position")
                    return
                notificationSettings.popupPosition = optionIndex
                root.persistNow()
            }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Automatic Do Not Disturb")
            subtitle: qsTr("Presentation and fullscreen behavior controlled by the active Plasma session")
            model: root.automaticDndRows
            onRowToggled: (index, checked, row) => root.setSetting(row.id, checked)
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Background tasks")
            subtitle: qsTr("Keep long-running work visible without leaving Meo Settings")
            model: root.backgroundRows
            onRowToggled: (index, checked, row) => root.setSetting(row.id, checked)
        }

        MeoSettingsGroup {
            width: parent.width
            visible: root.applicationRows.length > 0
            title: qsTr("By application")
            subtitle: qsTr("Rules apply to applications Plasma has actually seen; open an app to manage its popups, history, Do Not Disturb exception, and taskbar badge")
            model: root.applicationRows
            onRowActivated: (index, row) => root.openApplicationRules(row.id)
        }

        MeoEmptyState {
            width: parent.width
            height: 180 * MeoTheme.globalScale
            visible: root.applicationRows.length === 0
            icon: "notifications"
            title: qsTr("No applications have sent notifications yet")
            description: qsTr("When Plasma receives an application notification, it will appear here for direct rule management.")
        }

        MeoButton {
            text: qsTr("Open KDE notification module")
            type: "text"
            enabled: KcmBridge.isAvailable("kcm_notifications")
            onClicked: root.navigateTo("kcm:kcm_notifications")
        }
    }

    MeoSettingsTaskSheet {
        id: applicationRules
        popupParent: Overlay.overlay
        property string applicationId: ""
        title: applicationId || qsTr("Application notifications")
        subtitle: qsTr("These are Plasma’s stored notification behaviors for this application. They do not delete notification history or change the application itself.")
        rejectText: qsTr("Close")

        content: Component {
            Flickable {
                clip: true
                contentWidth: width
                contentHeight: rulesContent.implicitHeight + 32 * MeoTheme.globalScale

                MeoSettingsGroup {
                    id: rulesContent
                    width: parent.width - 32 * MeoTheme.globalScale
                    x: 16 * MeoTheme.globalScale
                    y: 16 * MeoTheme.globalScale
                    title: qsTr("Notification behavior")
                    subtitle: qsTr("Changes are saved to Plasma immediately")
                    model: [
                        {
                            "id": "popups",
                            "title": qsTr("Show popups"),
                            "subtitle": qsTr("Allow this app to interrupt with notification popups"),
                            "icon": "notifications", "tone": "primary",
                            "trailingKind": "toggle",
                            "checked": root.applicationAllows(applicationRules.applicationId,
                                                              NotificationManager.Settings.ShowPopups)
                        },
                        {
                            "id": "dnd-popups",
                            "title": qsTr("Allow during Do Not Disturb"),
                            "subtitle": qsTr("This app may show popups even while Do Not Disturb is on"),
                            "icon": "priority_high", "tone": "error",
                            "trailingKind": "toggle",
                            "checked": root.applicationAllows(applicationRules.applicationId,
                                                              NotificationManager.Settings.ShowPopupsInDoNotDisturbMode)
                        },
                        {
                            "id": "history",
                            "title": qsTr("Keep in notification history"),
                            "subtitle": qsTr("Keep this app’s notifications in the Meo status center"),
                            "icon": "history", "tone": "secondary",
                            "trailingKind": "toggle",
                            "checked": root.applicationAllows(applicationRules.applicationId,
                                                              NotificationManager.Settings.ShowInHistory)
                        },
                        {
                            "id": "badges",
                            "title": qsTr("Show taskbar badge"),
                            "subtitle": qsTr("Show unread counts on supported taskbar entries"),
                            "icon": "notifications_active", "tone": "tertiary",
                            "trailingKind": "toggle",
                            "checked": root.applicationAllows(applicationRules.applicationId,
                                                              NotificationManager.Settings.ShowBadges)
                        }
                    ]
                    onRowToggled: (index, checked, row) => {
                        let behavior = 0
                        switch (row.id) {
                        case "popups": behavior = NotificationManager.Settings.ShowPopups; break
                        case "dnd-popups": behavior = NotificationManager.Settings.ShowPopupsInDoNotDisturbMode; break
                        case "history": behavior = NotificationManager.Settings.ShowInHistory; break
                        case "badges": behavior = NotificationManager.Settings.ShowBadges; break
                        default: return
                        }
                        root.setApplicationBehavior(applicationRules.applicationId, behavior, checked)
                    }
                }
            }
        }
    }
}
