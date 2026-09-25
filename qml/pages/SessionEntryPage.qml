import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

// In-session presentation settings and preview. It never loads a credential
// model or writes authentication/display-manager policy; the narrow backend
// only persists allowlisted Look & Feel keys under KScreenLocker Greeter/LnF.
Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    property bool previewWeather: true
    property bool previewWeatherLocation: false
    property bool previewMedia: true
    property bool previewArtwork: true
    property bool previewAudio: true
    property bool previewPerformance: true
    property bool previewSystemSummary: true
    property bool previewSessionControls: true
    property string previewNotificationPrivacy: "count"
    readonly property bool editingLockScreenLayout: sessionEntryLayoutEditorMode

    function loadSavedPreferences() {
        const values = LockScreenPresentationBackend.settings || {}
        previewWeather = values.showWeather === undefined ? true : values.showWeather
        previewWeatherLocation = values.showWeatherLocation === undefined ? false : values.showWeatherLocation
        previewMedia = values.showMediaControls === undefined ? true : values.showMediaControls
        previewArtwork = values.showAlbumArtwork === undefined ? true : values.showAlbumArtwork
        previewAudio = values.showAudioControls === undefined ? true : values.showAudioControls
        previewPerformance = values.showPerformance === undefined ? true : values.showPerformance
        previewSystemSummary = values.showSystemSummary === undefined ? true : values.showSystemSummary
        previewSessionControls = values.showSessionControls === undefined ? true : values.showSessionControls
        previewNotificationPrivacy = values.notificationVisibility || "count"
    }

    function savePreferences() {
        LockScreenPresentationBackend.save({
            "showWeather": previewWeather,
            "showWeatherLocation": previewWeatherLocation,
            "showMediaControls": previewMedia,
            "showAlbumArtwork": previewArtwork,
            "showAudioControls": previewAudio,
            "showPerformance": previewPerformance,
            "showSystemSummary": previewSystemSummary,
            "showSessionControls": previewSessionControls,
            "notificationVisibility": previewNotificationPrivacy
        })
    }

    function restorePresentationDefaults() {
        LockScreenPresentationBackend.resetToDefaults()
        loadSavedPreferences()
    }

    readonly property var privacyOptions: [
        { "text": qsTr("Hide notifications"), "value": "hidden" },
        { "text": qsTr("Show count"), "value": "count" },
        { "text": qsTr("Show app name"), "value": "app-name" },
        { "text": qsTr("Show full content"), "value": "full-content" }
    ]

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Lock screen & login")
        subtitle: qsTr("Customize the Caelestia-style Meo lock-screen presentation. Authentication and lock policy remain with KDE.")

        MeoCard {
            visible: !root.editingLockScreenLayout
            width: parent.width
            type: "outlined"
            RowLayout {
                width: parent.width
                spacing: MeoTheme.space12
                MeoIcon { icon: "info"; size: 24; color: MeoTheme.primary }
                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Only presentation preferences are saved here. Passwords, PAM, fingerprints, lock timing, display-manager selection, and secure input remain controlled by KDE.")
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap
                }
            }
        }

        MeoCard {
            visible: !root.editingLockScreenLayout
            width: parent.width
            type: "elevated"
            interactive: false
            Column {
                width: parent.width
                spacing: MeoTheme.space12
                MeoText { text: qsTr("Safe visual preview"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                Rectangle {
                    width: parent.width
                    // Keep the preview's independent regions in reading order.
                    // The rich profile can show the date, weather, a notification,
                    // a non-interactive unlock hint, and media at once. A fixed
                    // canvas height made the middle content collide with the
                    // bottom preview controls at normal desktop widths.
                    height: Math.max((root.isCompact ? 640 : 560) * MeoTheme.globalScale,
                                     parent.width * (root.isCompact ? 1.10 : 0.65))
                    radius: MeoTheme.shapeExtraLarge
                    color: MeoTheme.surfaceContainerLow
                    border.width: MeoTheme.strokeWidthThin
                    border.color: MeoTheme.outlineVariant
                    clip: true

                    MeoAmbientClock {
                        id: previewClock
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: MeoTheme.space24
                        showDate: true
                    }
                    MeoWeatherStatus {
                        id: previewWeatherStatus
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: previewClock.bottom
                        anchors.topMargin: MeoTheme.space8
                        available: root.previewWeather
                        showLocation: root.previewWeatherLocation
                        location: qsTr("Singapore")
                        temperatureText: "28°C"
                        condition: qsTr("Partly cloudy")
                        iconName: "weather-partly-cloudy"
                    }
                    MeoPrivacyNotificationSummary {
                        id: previewNotification
                        anchors.left: parent.left
                        anchors.leftMargin: MeoTheme.space16
                        anchors.top: previewWeatherStatus.visible ? previewWeatherStatus.bottom : previewClock.bottom
                        anchors.topMargin: MeoTheme.space20
                        width: Math.min(260 * MeoTheme.globalScale, parent.width * 0.44)
                        privacyLevel: root.previewNotificationPrivacy
                        notificationCount: 2
                        applicationName: qsTr("Messages")
                        summary: qsTr("New message")
                        body: qsTr("Meet at the library after class")
                    }
                    Rectangle {
                        id: previewUnlock
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: root.previewMedia ? previewMediaController.top : parent.bottom
                        anchors.bottomMargin: root.previewMedia ? MeoTheme.space16 : MeoTheme.space20
                        width: Math.min(400 * MeoTheme.globalScale, parent.width - 2 * MeoTheme.space20)
                        height: 104 * MeoTheme.globalScale
                        radius: MeoTheme.shapeExtraLarge
                        color: MeoTheme.surfaceContainerHigh
                        border.width: MeoTheme.strokeWidthThin
                        border.color: MeoTheme.outlineVariant

                        Column {
                            anchors.fill: parent
                            anchors.margins: MeoTheme.space16
                            spacing: MeoTheme.space4
                            MeoText { text: qsTr("Unlock"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                            MeoText { text: qsTr("KDE continues to authenticate this screen"); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant }
                            MeoText { text: qsTr("Password entry is not interactive in preview"); typeRole: "label"; typeSize: "small"; color: MeoTheme.primary }
                        }
                    }
                    MeoMediaController {
                        id: previewMediaController
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: MeoTheme.space16
                        visible: root.previewMedia
                        width: Math.min(400 * MeoTheme.globalScale, parent.width - 2 * MeoTheme.space16)
                        height: 116 * MeoTheme.globalScale
                        presentation: "lockScreen"
                        title: qsTr("Night Drive")
                        artist: qsTr("Meo Sessions")
                        sourceName: qsTr("Preview")
                        showArtwork: root.previewArtwork
                        canSeek: false
                        canSkipPrevious: false
                        canSkipNext: false
                        showVolume: root.previewAudio
                        showSecondaryActions: false
                    }
                }
            }
        }

        MeoSettingsGroup {
            visible: !root.editingLockScreenLayout
            width: parent.width
            title: qsTr("Lock-screen content")
            subtitle: qsTr("Choose which real cached or current-session surfaces appear around the fixed Caelestia-style center layout.")
            model: [
                { "id": "weather", "title": qsTr("Weather"), "subtitle": qsTr("Show only cached weather; it never blocks authentication"), "icon": "partly_cloudy_day", "tone": "secondary", "trailingKind": "toggle", "checked": root.previewWeather },
                { "id": "weather-location", "title": qsTr("Precise weather location"), "subtitle": qsTr("Independent privacy choice"), "icon": "location_on", "tone": "secondary", "trailingKind": "toggle", "checked": root.previewWeatherLocation, "enabled": root.previewWeather },
                { "id": "media", "title": qsTr("Media controls"), "subtitle": qsTr("Current-session MPRIS controls only"), "icon": "music_note", "tone": "tertiary", "trailingKind": "toggle", "checked": root.previewMedia },
                { "id": "artwork", "title": qsTr("Album artwork"), "subtitle": qsTr("Independent media privacy choice"), "icon": "album", "tone": "tertiary", "trailingKind": "toggle", "checked": root.previewArtwork, "enabled": root.previewMedia },
                { "id": "audio", "title": qsTr("Output volume"), "subtitle": qsTr("Volume and mute only; no output-device or microphone controls"), "icon": "volume_up", "tone": "tertiary", "trailingKind": "toggle", "checked": root.previewAudio },
                { "id": "system-summary", "title": qsTr("System summary"), "subtitle": qsTr("Show OS, desktop, uptime, battery, connection state, and dynamic palette"), "icon": "terminal", "tone": "primary", "trailingKind": "toggle", "checked": root.previewSystemSummary },
                { "id": "performance", "title": qsTr("Performance"), "subtitle": qsTr("Show aggregate CPU, memory, storage, and temperature only"), "icon": "monitoring", "tone": "primary", "trailingKind": "toggle", "checked": root.previewPerformance },
                { "id": "session-controls", "title": qsTr("Session controls"), "subtitle": qsTr("Reveal KDE sleep, hibernate, restart, and shut-down actions when hovering Resources"), "icon": "power_settings_new", "tone": "primary", "trailingKind": "toggle", "checked": root.previewSessionControls, "enabled": root.previewPerformance }
            ]
            onRowToggled: (index, checked, row) => {
                if (row.id === "weather") { root.previewWeather = checked; if (!checked) root.previewWeatherLocation = false }
                else if (row.id === "weather-location") root.previewWeatherLocation = checked
                else if (row.id === "media") { root.previewMedia = checked; if (!checked) root.previewArtwork = false }
                else if (row.id === "artwork") root.previewArtwork = checked
                else if (row.id === "audio") root.previewAudio = checked
                else if (row.id === "system-summary") root.previewSystemSummary = checked
                else if (row.id === "performance") { root.previewPerformance = checked; if (!checked) root.previewSessionControls = false }
                else if (row.id === "session-controls") root.previewSessionControls = checked
            }
        }

        MeoCard {
            visible: !root.editingLockScreenLayout
            width: parent.width
            type: "outlined"
            ColumnLayout {
                width: parent.width
                spacing: MeoTheme.space12
                MeoText { Layout.fillWidth: true; text: qsTr("Weather city"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                MeoText { Layout.fillWidth: true; text: qsTr("The locker reads a six-hour cache only. Choosing a city never sends a request until you explicitly refresh it."); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                MeoTextField {
                    id: weatherCityField
                    Layout.fillWidth: true
                    label: qsTr("City")
                    leadingIcon: "location_on"
                    maxLength: 96
                    text: WeatherBackend.city
                    enabled: !WeatherBackend.busy
                    onAccepted: WeatherBackend.setCity(text)
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: WeatherBackend.error !== ""
                    spacing: MeoTheme.space4
                    MeoBanner {
                        Layout.fillWidth: true
                        title: qsTr("Weather status is unavailable")
                        text: qsTr("Check the weather service, then refresh the cached weather.")
                        icon: "error"
                        tone: "error"
                    }
                    MeoText {
                        Layout.fillWidth: true
                        text: qsTr("Technical details: %1").arg(WeatherBackend.error)
                        Accessible.name: text
                        typeRole: "label"
                        typeSize: "small"
                        color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space8
                    MeoButton { text: qsTr("Save city"); type: "tonal"; onClicked: WeatherBackend.setCity(weatherCityField.text) }
                    MeoButton { text: WeatherBackend.busy ? qsTr("Refreshing…") : qsTr("Refresh cached weather"); icon.name: "refresh"; type: "filled"; enabled: WeatherBackend.available && !WeatherBackend.busy; onClicked: WeatherBackend.refreshNow() }
                }
                MeoText { Layout.fillWidth: true; visible: !WeatherBackend.available; text: qsTr("Install the MeoKDE weather refresher to fetch this cache. The lock screen remains offline either way."); typeRole: "label"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
            }
        }

        MeoCard {
            visible: !root.editingLockScreenLayout
            width: parent.width
            type: "outlined"
            ColumnLayout {
                width: parent.width
                spacing: MeoTheme.space12
                MeoText { Layout.fillWidth: true; text: qsTr("Notification privacy"); typeRole: "title"; typeSize: "small"; emphasized: true; color: MeoTheme.contentOnSurface }
                MeoText { Layout.fillWidth: true; text: qsTr("The privacy-first default shows only the notification count. App names or full content are opt-in."); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
                MeoExposedDropdown {
                    Layout.fillWidth: true
                    label: qsTr("Visibility")
                    model: root.privacyOptions
                    textRole: "text"
                    valueRole: "value"
                    currentValue: root.previewNotificationPrivacy
                    onSelected: (index, value) => root.previewNotificationPrivacy = root.privacyOptions[index].value
                }
            }
        }

        MeoBanner {
            visible: !root.editingLockScreenLayout && LockScreenPresentationBackend.error !== ""
            width: parent.width
            title: qsTr("Lock-screen presentation settings could not be saved")
            text: LockScreenPresentationBackend.error
            icon: "error"
            tone: "error"
        }

        Flow {
            visible: !root.editingLockScreenLayout
            width: parent.width
            spacing: MeoTheme.space8

            MeoButton {
                text: qsTr("Save lock-screen appearance")
                icon.name: "save"
                type: "filled"
                onClicked: root.savePreferences()
            }
            MeoButton {
                text: qsTr("Restore defaults")
                type: "text"
                onClicked: root.restorePresentationDefaults()
            }
            MeoButton {
                visible: KcmBridge.isAvailable("kcm_screenlocker")
                text: qsTr("Open KDE screen lock controls")
                icon.name: "open_in_new"
                type: "tonal"
                onClicked: root.navigateTo("kcm:kcm_screenlocker")
            }
        }

        MeoText {
            visible: !root.editingLockScreenLayout
            width: parent.width
            text: qsTr("Saved presentation changes are picked up the next time the lock screen is created. Authentication settings are unchanged.")
            typeRole: "label"
            typeSize: "small"
            color: MeoTheme.contentOnSurfaceVariant
            wrapMode: Text.WordWrap
        }

        MeoLockScreenLayoutEditor {
            visible: root.editingLockScreenLayout
            width: parent.width
        }
    }

    Connections {
        target: LockScreenPresentationBackend
        function onChanged() {
            if (!root.editingLockScreenLayout)
                root.loadSavedPreferences()
        }
    }

    Component.onCompleted: root.loadSavedPreferences()
}
