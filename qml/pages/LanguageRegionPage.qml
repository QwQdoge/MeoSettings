import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function moduleRow(moduleId, title, subtitle, icon, tone) {
        const available = KcmBridge.isAvailable(moduleId)
        return {
            "title": title,
            "subtitle": available ? subtitle : qsTr("This system setting is not installed"),
            "icon": icon,
            "tone": tone || "primary",
            "route": "kcm:" + moduleId,
            "enabled": available,
            "trailingKind": "choice",
            "trailingText": qsTr("Advanced")
        }
    }

    readonly property var regionRows: [
        moduleRow("kcm_regionandlang",
                  qsTr("Language & formats"),
                  qsTr("UI language, number, date, currency, and regional formats"),
                  "language", "primary"),
        moduleRow("kcm_clock",
                  qsTr("Date & time"),
                  qsTr("Clock, time zone, and time synchronization"),
                  "schedule", "secondary"),
        moduleRow("kcm_fcitx5",
                  qsTr("Input method"),
                  qsTr("Languages, layouts, Pinyin, and other input engines"),
                  "keyboard", "secondary"),
        moduleRow("kcmspellchecking",
                  qsTr("Spell checking"),
                  qsTr("Dictionaries and spell-check behavior used by KDE applications"),
                  "spellcheck", "tertiary")
    ]

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Language & region")
        subtitle: qsTr("Language, regional formats, input, time, and weather location")

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Regional settings")
            subtitle: ""
            model: root.regionRows
            onRowActivated: (index, row) => root.navigateTo(row.route)
        }

        MeoCard {
            width: parent.width
            type: "filled"

            ColumnLayout {
                anchors.fill: parent
                spacing: MeoTheme.space12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space12

                    MeoIcon {
                        icon: "location_on"
                        size: 28
                        color: MeoTheme.primary
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space2

                        MeoText {
                            Layout.fillWidth: true
                            text: qsTr("Weather location")
                            typeRole: "title"
                            typeSize: "small"
                            emphasized: true
                            color: MeoTheme.contentOnSurface
                        }

                        MeoText {
                            Layout.fillWidth: true
                            text: qsTr("Used by Meo's cached weather surfaces. The lock screen reads the cache offline and never performs its own network request.")
                            typeRole: "body"
                            typeSize: "small"
                            color: MeoTheme.contentOnSurfaceVariant
                            wrapMode: Text.WordWrap
                        }
                    }
                }

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

                Flow {
                    Layout.fillWidth: true
                    spacing: MeoTheme.space8

                    MeoButton {
                        text: qsTr("Save city")
                        type: "tonal"
                        enabled: !WeatherBackend.busy
                        onClicked: WeatherBackend.setCity(weatherCityField.text)
                    }

                    MeoButton {
                        text: WeatherBackend.busy ? qsTr("Refreshing…") : qsTr("Refresh weather")
                        icon.name: "refresh"
                        type: "filled"
                        enabled: WeatherBackend.available && !WeatherBackend.busy
                        onClicked: WeatherBackend.refreshNow()
                    }
                }

                MeoText {
                    Layout.fillWidth: true
                    visible: WeatherBackend.lastResult !== ""
                    text: WeatherBackend.lastResult
                    typeRole: "label"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }

        Column {
            width: parent.width
            visible: WeatherBackend.error !== ""
            spacing: MeoTheme.space4

            MeoBanner {
                width: parent.width
                title: qsTr("Weather location needs attention")
                text: qsTr("The city preference is kept locally, but the weather cache could not be refreshed.")
                icon: "error"
                tone: "error"
            }

            MeoText {
                width: parent.width
                text: qsTr("Technical details: %1").arg(WeatherBackend.error)
                typeRole: "label"
                typeSize: "small"
                color: MeoTheme.contentOnSurfaceVariant
                wrapMode: Text.WordWrap
            }
        }

        MeoCard {
            width: parent.width
            type: "outlined"
            visible: !WeatherBackend.available

            RowLayout {
                anchors.fill: parent
                spacing: MeoTheme.space12

                MeoIcon {
                    icon: "cloud_off"
                    size: 24
                    color: MeoTheme.contentOnSurfaceVariant
                }

                MeoText {
                    Layout.fillWidth: true
                    text: qsTr("The MeoKDE weather refresher is not installed. Your city preference can still be saved, and lock-screen weather simply stays unavailable.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: WeatherBackend.refresh()
}
