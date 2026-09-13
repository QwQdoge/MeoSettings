import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

// This is intentionally an in-session layout simulator, never a KScreenLocker
// surface. It accepts placement of reviewed Meo widgets only; normal Plasma
// applets and their runtime code do not enter the lock-screen trust boundary.
Item {
    id: root

    property bool guidesVisible: true
    property bool clockVisible: true
    property bool weatherVisible: true
    property bool mediaVisible: true
    property bool notificationsVisible: true
    property string selectedWidget: "clock"

    readonly property real snapUnit: 8 * MeoTheme.globalScale
    readonly property real guideInterval: snapUnit * 4
    implicitHeight: editorCard.implicitHeight

    function snap(value, maximum) {
        return Math.max(0, Math.min(maximum, Math.round(value / snapUnit) * snapUnit))
    }

    function resetLayout() {
        clockWidget.resetPosition()
        weatherWidget.resetPosition()
        mediaWidget.resetPosition()
        notificationWidget.resetPosition()
        selectedWidget = "clock"
    }

    function selectWidget(widgetId) {
        selectedWidget = widgetId
    }

    MeoCard {
        id: editorCard
        width: parent.width
        type: "elevated"

        ColumnLayout {
            width: parent.width
            spacing: MeoTheme.space16

            RowLayout {
                Layout.fillWidth: true
                spacing: MeoTheme.space12

                MeoIcon { icon: "dashboard_customize"; size: 24; color: MeoTheme.primary }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space2
                    MeoText {
                        text: qsTr("Lock-screen layout editor")
                        typeRole: "title"; typeSize: "medium"; emphasized: true
                        color: MeoTheme.contentOnSurface
                    }
                    MeoText {
                        text: qsTr("A simulated secure surface. Drag only reviewed Meo widgets; it cannot unlock, cover a display, or load desktop applets.")
                        typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                id: layoutCanvas
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(420 * MeoTheme.globalScale,
                                                root.width * 0.46)
                radius: MeoTheme.shapeExtraLarge
                color: MeoTheme.surfaceContainerLow
                border.width: MeoTheme.strokeWidthThin
                border.color: MeoTheme.outlineVariant
                clip: true

                Repeater {
                    model: root.guidesVisible ? Math.ceil(layoutCanvas.width / root.guideInterval) + 1 : 0
                    delegate: Rectangle {
                        x: index * root.guideInterval
                        width: MeoTheme.strokeWidthThin
                        height: layoutCanvas.height
                        color: MeoTheme.outlineVariant
                        opacity: 0.28
                    }
                }
                Repeater {
                    model: root.guidesVisible ? Math.ceil(layoutCanvas.height / root.guideInterval) + 1 : 0
                    delegate: Rectangle {
                        y: index * root.guideInterval
                        width: layoutCanvas.width
                        height: MeoTheme.strokeWidthThin
                        color: MeoTheme.outlineVariant
                        opacity: 0.28
                    }
                }
                Rectangle {
                    visible: root.guidesVisible
                    x: layoutCanvas.width / 2 - width / 2
                    width: MeoTheme.strokeWidthThin
                    height: layoutCanvas.height
                    color: MeoTheme.primary
                    opacity: 0.50
                }
                Rectangle {
                    visible: root.guidesVisible
                    y: layoutCanvas.height / 2 - height / 2
                    width: layoutCanvas.width
                    height: MeoTheme.strokeWidthThin
                    color: MeoTheme.primary
                    opacity: 0.50
                }

                MeoText {
                    anchors { left: parent.left; top: parent.top; margins: MeoTheme.space12 }
                    text: qsTr("SIMULATED LOCK SCREEN")
                    typeRole: "label"; typeSize: "small"; emphasized: true
                    color: MeoTheme.contentOnSurfaceVariant
                    opacity: 0.72
                }

                Item {
                    id: clockWidget
                    width: 252 * MeoTheme.globalScale
                    height: 128 * MeoTheme.globalScale
                    visible: root.clockVisible

                    function resetPosition() {
                        x = root.snap((layoutCanvas.width - width) / 2, layoutCanvas.width - width)
                        y = root.snap(56 * MeoTheme.globalScale, layoutCanvas.height - height)
                    }
                    Component.onCompleted: Qt.callLater(resetPosition)

                    Rectangle {
                        anchors.fill: parent
                        radius: MeoTheme.shapeLarge
                        color: root.selectedWidget === "clock" ? MeoTheme.secondaryContainer : "transparent"
                        border.width: root.selectedWidget === "clock" ? 2 * MeoTheme.strokeWidthThin : MeoTheme.strokeWidthThin
                        border.color: root.selectedWidget === "clock" ? MeoTheme.primary : MeoTheme.outlineVariant
                    }
                    MeoAmbientClock { anchors.centerIn: parent; showDate: true }
                    MouseArea {
                        anchors.fill: parent
                        drag.target: clockWidget
                        drag.minimumX: 0; drag.maximumX: layoutCanvas.width - clockWidget.width
                        drag.minimumY: 0; drag.maximumY: layoutCanvas.height - clockWidget.height
                        onPressed: root.selectWidget("clock")
                        onReleased: {
                            clockWidget.x = root.snap(clockWidget.x, layoutCanvas.width - clockWidget.width)
                            clockWidget.y = root.snap(clockWidget.y, layoutCanvas.height - clockWidget.height)
                        }
                    }
                }

                Item {
                    id: weatherWidget
                    width: 226 * MeoTheme.globalScale
                    height: 72 * MeoTheme.globalScale
                    visible: root.weatherVisible

                    function resetPosition() {
                        x = root.snap((layoutCanvas.width - width) / 2, layoutCanvas.width - width)
                        y = root.snap(204 * MeoTheme.globalScale, layoutCanvas.height - height)
                    }
                    Component.onCompleted: Qt.callLater(resetPosition)

                    Rectangle {
                        anchors.fill: parent
                        radius: MeoTheme.shapeLarge
                        color: root.selectedWidget === "weather" ? MeoTheme.secondaryContainer : MeoTheme.surfaceContainerHigh
                        border.width: root.selectedWidget === "weather" ? 2 * MeoTheme.strokeWidthThin : MeoTheme.strokeWidthThin
                        border.color: root.selectedWidget === "weather" ? MeoTheme.primary : MeoTheme.outlineVariant
                    }
                    MeoWeatherStatus {
                        anchors.centerIn: parent
                        available: true
                        showLocation: false
                        temperatureText: "28°C"
                        condition: qsTr("Partly cloudy")
                        iconName: "weather-partly-cloudy"
                    }
                    MouseArea {
                        anchors.fill: parent
                        drag.target: weatherWidget
                        drag.minimumX: 0; drag.maximumX: layoutCanvas.width - weatherWidget.width
                        drag.minimumY: 0; drag.maximumY: layoutCanvas.height - weatherWidget.height
                        onPressed: root.selectWidget("weather")
                        onReleased: {
                            weatherWidget.x = root.snap(weatherWidget.x, layoutCanvas.width - weatherWidget.width)
                            weatherWidget.y = root.snap(weatherWidget.y, layoutCanvas.height - weatherWidget.height)
                        }
                    }
                }

                Item {
                    id: notificationWidget
                    width: Math.min(220 * MeoTheme.globalScale, layoutCanvas.width * 0.42)
                    height: 88 * MeoTheme.globalScale
                    visible: root.notificationsVisible

                    function resetPosition() {
                        x = root.snap(layoutCanvas.width - width - 24 * MeoTheme.globalScale,
                                      layoutCanvas.width - width)
                        y = root.snap(layoutCanvas.height * 0.54, layoutCanvas.height - height)
                    }
                    Component.onCompleted: Qt.callLater(resetPosition)

                    Rectangle {
                        anchors.fill: parent
                        radius: MeoTheme.shapeLarge
                        color: root.selectedWidget === "notifications" ? MeoTheme.secondaryContainer : MeoTheme.surfaceContainerHigh
                        border.width: root.selectedWidget === "notifications" ? 2 * MeoTheme.strokeWidthThin : MeoTheme.strokeWidthThin
                        border.color: root.selectedWidget === "notifications" ? MeoTheme.primary : MeoTheme.outlineVariant
                    }
                    MeoPrivacyNotificationSummary {
                        anchors.centerIn: parent
                        width: parent.width - 2 * MeoTheme.space12
                        privacyLevel: "count"
                        notificationCount: 2
                    }
                    MouseArea {
                        anchors.fill: parent
                        drag.target: notificationWidget
                        drag.minimumX: 0; drag.maximumX: layoutCanvas.width - notificationWidget.width
                        drag.minimumY: 0; drag.maximumY: layoutCanvas.height - notificationWidget.height
                        onPressed: root.selectWidget("notifications")
                        onReleased: {
                            notificationWidget.x = root.snap(notificationWidget.x, layoutCanvas.width - notificationWidget.width)
                            notificationWidget.y = root.snap(notificationWidget.y, layoutCanvas.height - notificationWidget.height)
                        }
                    }
                }

                Item {
                    id: mediaWidget
                    width: Math.min(340 * MeoTheme.globalScale, layoutCanvas.width - 2 * MeoTheme.space16)
                    height: 108 * MeoTheme.globalScale
                    visible: root.mediaVisible

                    function resetPosition() {
                        x = root.snap((layoutCanvas.width - width) / 2, layoutCanvas.width - width)
                        y = root.snap(layoutCanvas.height - height - 24 * MeoTheme.globalScale,
                                      layoutCanvas.height - height)
                    }
                    Component.onCompleted: Qt.callLater(resetPosition)

                    Rectangle {
                        anchors.fill: parent
                        radius: MeoTheme.shapeLarge
                        color: root.selectedWidget === "media" ? MeoTheme.secondaryContainer : MeoTheme.surfaceContainerHigh
                        border.width: root.selectedWidget === "media" ? 2 * MeoTheme.strokeWidthThin : MeoTheme.strokeWidthThin
                        border.color: root.selectedWidget === "media" ? MeoTheme.primary : MeoTheme.outlineVariant
                    }
                    MeoMediaController {
                        anchors.centerIn: parent
                        width: parent.width - 2 * MeoTheme.space8
                        height: parent.height - 2 * MeoTheme.space8
                        // The editor uses the compact MeoUI projection so the
                        // placement tile reflects its bounded lock-screen slot;
                        // the real locker still owns the MPRIS presentation.
                        presentation: "compact"
                        title: qsTr("Night Drive")
                        artist: qsTr("Meo Sessions")
                        sourceName: qsTr("Preview")
                        showArtwork: false
                        canSeek: false
                        canSkipPrevious: false
                        canSkipNext: false
                        showVolume: false
                        showSecondaryActions: false
                    }
                    MouseArea {
                        anchors.fill: parent
                        drag.target: mediaWidget
                        drag.minimumX: 0; drag.maximumX: layoutCanvas.width - mediaWidget.width
                        drag.minimumY: 0; drag.maximumY: layoutCanvas.height - mediaWidget.height
                        onPressed: root.selectWidget("media")
                        onReleased: {
                            mediaWidget.x = root.snap(mediaWidget.x, layoutCanvas.width - mediaWidget.width)
                            mediaWidget.y = root.snap(mediaWidget.y, layoutCanvas.height - mediaWidget.height)
                        }
                    }
                }
            }

            MeoSettingsGroup {
                Layout.fillWidth: true
                title: qsTr("Widgets and guides")
                subtitle: qsTr("Placement snaps to 8 dp. Guides are editor-only and never become a desktop or lock-screen overlay.")
                model: [
                    { "id": "clock", "title": qsTr("Clock"), "subtitle": qsTr("Required Meo ambient clock"), "icon": "schedule", "tone": "primary", "trailingKind": "toggle", "checked": root.clockVisible },
                    { "id": "weather", "title": qsTr("Weather"), "subtitle": qsTr("Cached, optional status"), "icon": "partly_cloudy_day", "tone": "secondary", "trailingKind": "toggle", "checked": root.weatherVisible },
                    { "id": "media", "title": qsTr("Media controls"), "subtitle": qsTr("Current-session MPRIS preview"), "icon": "music_note", "tone": "tertiary", "trailingKind": "toggle", "checked": root.mediaVisible },
                    { "id": "notifications", "title": qsTr("Notification summary"), "subtitle": qsTr("Privacy-filtered presentation"), "icon": "notifications", "tone": "primary", "trailingKind": "toggle", "checked": root.notificationsVisible },
                    { "id": "guides", "title": qsTr("Alignment guides"), "subtitle": qsTr("Show an 8 dp snap grid while editing"), "icon": "grid_on", "tone": "neutral", "trailingKind": "toggle", "checked": root.guidesVisible }
                ]
                onRowToggled: (index, checked, row) => {
                    if (row.id === "clock") root.clockVisible = checked
                    else if (row.id === "weather") root.weatherVisible = checked
                    else if (row.id === "media") root.mediaVisible = checked
                    else if (row.id === "notifications") root.notificationsVisible = checked
                    else if (row.id === "guides") root.guidesVisible = checked
                }
            }

            RowLayout {
                Layout.fillWidth: true
                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("Selected: %1").arg(root.selectedWidget)
                    typeRole: "body"; typeSize: "small"; color: MeoTheme.contentOnSurfaceVariant
                }
                MeoButton {
                    text: qsTr("Reset layout")
                    type: "text"
                    onClicked: root.resetLayout()
                }
            }
        }
    }
}
