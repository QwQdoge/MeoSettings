import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

// A data-only, in-session preview. It never loads a credential model, changes
// KScreenLocker, or writes a display-manager file.
Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    property bool previewWeather: false
    property bool previewWeatherLocation: false
    property bool previewMedia: false
    property bool previewArtwork: false
    property string previewNotificationPrivacy: "count"
    property bool editingLockScreenLayout: sessionEntryLayoutEditorMode

    function restorePreviewDefaults() {
        previewWeather = false
        previewWeatherLocation = false
        previewMedia = false
        previewArtwork = false
        previewNotificationPrivacy = "count"
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
        subtitle: qsTr("Preview the Meo session entry without changing authentication or current system configuration.")

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
                    text: qsTr("Preview-only draft. A validated MeoKDE writer is required before presentation preferences can be saved. Passwords, PAM, fingerprints, display-manager selection, and screen layout remain with KDE.")
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
                    height: Math.max(380 * MeoTheme.globalScale, root.isCompact ? 420 * MeoTheme.globalScale : 400 * MeoTheme.globalScale)
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
                        anchors.left: parent.left
                        anchors.leftMargin: MeoTheme.space16
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(260 * MeoTheme.globalScale, parent.width * 0.44)
                        privacyLevel: root.previewNotificationPrivacy
                        notificationCount: 2
                        applicationName: qsTr("Messages")
                        summary: qsTr("New message")
                        body: qsTr("Meet at the library after class")
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: root.previewMedia ? 148 * MeoTheme.globalScale : MeoTheme.space20
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
                        showVolume: false
                        showSecondaryActions: false
                    }
                }
            }
        }

        MeoSettingsGroup {
            visible: !root.editingLockScreenLayout
            width: parent.width
            title: qsTr("Preview content")
            subtitle: qsTr("All optional content starts hidden on the actual lock screen.")
            model: [
                { "id": "weather", "title": qsTr("Weather"), "subtitle": qsTr("Show only cached weather; it never blocks authentication"), "icon": "partly_cloudy_day", "tone": "secondary", "trailingKind": "toggle", "checked": root.previewWeather },
                { "id": "weather-location", "title": qsTr("Precise weather location"), "subtitle": qsTr("Independent privacy choice"), "icon": "location_on", "tone": "secondary", "trailingKind": "toggle", "checked": root.previewWeatherLocation, "enabled": root.previewWeather },
                { "id": "media", "title": qsTr("Media controls"), "subtitle": qsTr("Current-session MPRIS controls only"), "icon": "music_note", "tone": "tertiary", "trailingKind": "toggle", "checked": root.previewMedia },
                { "id": "artwork", "title": qsTr("Album artwork"), "subtitle": qsTr("Independent media privacy choice"), "icon": "album", "tone": "tertiary", "trailingKind": "toggle", "checked": root.previewArtwork, "enabled": root.previewMedia }
            ]
            onRowToggled: (index, checked, row) => {
                if (row.id === "weather") { root.previewWeather = checked; if (!checked) root.previewWeatherLocation = false }
                else if (row.id === "weather-location") root.previewWeatherLocation = checked
                else if (row.id === "media") { root.previewMedia = checked; if (!checked) root.previewArtwork = false }
                else if (row.id === "artwork") root.previewArtwork = checked
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
                MeoText { Layout.fillWidth: true; text: qsTr("The lock-screen default is a count only. This preview does not read current notifications."); typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant; wrapMode: Text.WordWrap }
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

        Flow {
            width: parent.width
            spacing: MeoTheme.space8
            MeoButton {
                text: root.editingLockScreenLayout ? qsTr("Finish editing") : qsTr("Edit lock-screen layout")
                icon.name: root.editingLockScreenLayout ? "done" : "dashboard_customize"
                type: "tonal"
                onClicked: root.editingLockScreenLayout = !root.editingLockScreenLayout
            }
            MeoButton { text: qsTr("Restore preview defaults"); type: "text"; onClicked: root.restorePreviewDefaults() }
            MeoButton {
                visible: KcmBridge.isAvailable("kcm_screenlocker")
                text: qsTr("Open KDE screen lock controls")
                icon.name: "open_in_new"
                type: "tonal"
                onClicked: root.navigateTo("kcm:kcm_screenlocker")
            }
        }

        MeoLockScreenLayoutEditor {
            visible: root.editingLockScreenLayout
            width: parent.width
        }
    }
}
