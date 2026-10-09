import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property bool inputMethodsExpanded: false
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function moduleRow(moduleId, title, subtitle, icon, tone) {
        const available = KcmBridge.isAvailable(moduleId)
        return {
            "id": "kcm-" + moduleId,
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

    function availableEntry(inputMethodId) {
        const inventory = InputMethods.availableInputMethods || []
        for (let index = 0; index < inventory.length; ++index) {
            if (String(inventory[index].id || "") === inputMethodId)
                return inventory[index]
        }
        return ({})
    }

    function inputMethodName(inputMethodId) {
        const entry = availableEntry(inputMethodId)
        const nativeName = String(entry.nativeName || "").trim()
        const name = String(entry.name || "").trim()
        const label = String(entry.label || "").trim()
        if (nativeName !== "")
            return nativeName
        if (name !== "")
            return name
        if (label !== "")
            return label
        return inputMethodId
    }

    readonly property var regionRows: [
        {
            "id": "input-method",
            "title": qsTr("Input methods"),
            "subtitle": InputMethods.available
                        ? qsTr("%1 · %2").arg(InputMethods.currentGroup || qsTr("Fcitx 5"))
                                             .arg(InputMethods.currentInputMethod
                                                  ? root.inputMethodName(InputMethods.currentInputMethod)
                                                  : qsTr("No active engine"))
                        : qsTr("Fcitx 5 languages, groups, and input engines"),
            "icon": "keyboard",
            "tone": "secondary",
            "enabled": true,
            "trailingKind": "navigation"
        },
        moduleRow("kcmspellchecking",
                  qsTr("Spell checking"),
                  qsTr("Dictionaries and spell-check behavior used by KDE applications"),
                  "spellcheck", "tertiary")
    ]

    readonly property var activeInputRows: {
        const rows = []
        const active = InputMethods.activeInputMethods || []
        for (let index = 0; index < active.length; ++index) {
            const item = active[index]
            const inputMethodId = String(item.id || "")
            if (inputMethodId === "")
                continue
            const inventoryEntry = root.availableEntry(inputMethodId)
            const language = String(inventoryEntry.language || "").trim()
            const layout = String(item.layout || "").trim()
            let details = qsTr("Available in the current Fcitx group")
            if (language !== "" && layout !== "")
                details = qsTr("%1 · keyboard layout %2").arg(language).arg(layout)
            else if (language !== "")
                details = language
            else if (layout !== "")
                details = qsTr("Keyboard layout %1").arg(layout)
            rows.push({
                "id": "input-engine-" + inputMethodId,
                "inputMethodId": inputMethodId,
                "title": root.inputMethodName(inputMethodId),
                "subtitle": details,
                "icon": "keyboard",
                "tone": "secondary",
                "trailingKind": "radio",
                "checked": InputMethods.currentInputMethod === inputMethodId,
                "enabled": InputMethods.available && !InputMethods.busy
            })
        }
        return rows
    }

    readonly property var inputGroupRows: {
        const rows = []
        const groups = InputMethods.groups || []
        for (let index = 0; index < groups.length; ++index) {
            const groupName = String(groups[index] || "")
            if (groupName === "")
                continue
            rows.push({
                "id": "input-group-" + groupName,
                "groupName": groupName,
                "title": groupName,
                "subtitle": groupName === InputMethods.currentGroup
                            ? qsTr("Current input-method group")
                            : qsTr("Switch to this Fcitx group"),
                "icon": "language",
                "tone": "primary",
                "trailingKind": "radio",
                "checked": groupName === InputMethods.currentGroup,
                "enabled": InputMethods.available && !InputMethods.busy
            })
        }
        return rows
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("Language & region")
        subtitle: qsTr("Language, regional formats, input, time, and weather location")

        MeoBanner {
            width: parent.width
            visible: RegionalBackend.error !== ""
            title: qsTr("Regional setting needs attention")
            text: RegionalBackend.error
            tone: "error"
        }
        MeoBanner {
            width: parent.width
            visible: RegionalBackend.restartRequired
            title: qsTr("Language preferences saved")
            text: qsTr("Sign out and sign in to apply system language and formats to all applications.")
        }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("System language")
            model: RegionalBackend.languages
            textRole: "label"
            valueRole: "id"
            currentValue: RegionalBackend.preferences.language
            onSelected: (index, value) => RegionalBackend.setLanguage(value)
        }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("Regional formats")
            model: RegionalBackend.formats
            textRole: "label"
            valueRole: "id"
            currentValue: RegionalBackend.preferences.format
            onSelected: (index, value) => RegionalBackend.setFormats(value)
        }
        Flow {
            width: parent.width
            spacing: MeoTheme.space8
            MeoButton { text: qsTr("Use system language default"); type: "text"; onClicked: RegionalBackend.setLanguage("") }
            MeoButton { text: qsTr("Use system format default"); type: "text"; onClicked: RegionalBackend.setFormats("") }
            MeoButton { text: qsTr("Date & time"); type: "tonal"; onClicked: root.navigateTo("date-time") }
            MeoButton { text: qsTr("Advanced language settings"); type: "text"; onClicked: root.navigateTo("kcm:kcm_regionandlang") }
        }

        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Regional settings")
            subtitle: ""
            model: root.regionRows
            onRowActivated: (index, row) => {
                if (row.id === "input-method") {
                    root.inputMethodsExpanded = !root.inputMethodsExpanded
                    if (root.inputMethodsExpanded)
                        InputMethods.refresh()
                    return
                }
                if (row.route)
                    root.navigateTo(row.route)
            }
        }

        Column {
            width: parent.width
            visible: root.inputMethodsExpanded
            spacing: MeoTheme.space16

            Column {
                width: parent.width
                visible: InputMethods.lastError !== ""
                spacing: MeoTheme.space4

                MeoBanner {
                    width: parent.width
                    title: qsTr("Input method needs attention")
                    text: qsTr("Fcitx could not complete the last request. Refresh the runtime state and try again.")
                    icon: "error"
                    tone: "error"
                }

                MeoText {
                    width: parent.width
                    text: qsTr("Technical details: %1").arg(InputMethods.lastError)
                    typeRole: "label"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }

                MeoButton {
                    text: qsTr("Dismiss")
                    type: "text"
                    size: "s"
                    onClicked: InputMethods.clearError()
                }
            }

            MeoCard {
                width: parent.width
                type: "outlined"
                visible: !InputMethods.available

                ColumnLayout {
                    anchors.fill: parent
                    spacing: MeoTheme.space12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space12

                        MeoIcon {
                            icon: "keyboard_off"
                            size: 28
                            color: MeoTheme.contentOnSurfaceVariant
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MeoTheme.space2

                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Fcitx 5 is not available in this session")
                                typeRole: "title"
                                typeSize: "small"
                                emphasized: true
                                color: MeoTheme.contentOnSurface
                            }

                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Meo Settings does not start a second input-method process. On Plasma Wayland, KWin owns the Fcitx session.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    MeoButton {
                        text: InputMethods.busy ? qsTr("Refreshing…") : qsTr("Refresh")
                        type: "tonal"
                        icon.name: "refresh"
                        enabled: !InputMethods.busy
                        onClicked: InputMethods.refresh()
                    }
                }
            }

            MeoSettingsGroup {
                width: parent.width
                visible: InputMethods.available && root.activeInputRows.length > 0
                title: qsTr("Current input methods")
                subtitle: qsTr("Choose which installed engine receives keyboard input now")
                model: root.activeInputRows
                onRowActivated: (index, row) => {
                    if (!InputMethods.busy && row.inputMethodId)
                        InputMethods.setCurrentInputMethod(row.inputMethodId)
                }
            }

            MeoSettingsGroup {
                width: parent.width
                visible: InputMethods.available && root.inputGroupRows.length > 1
                title: qsTr("Input-method groups")
                subtitle: qsTr("Switch between the groups already configured in Fcitx")
                model: root.inputGroupRows
                onRowActivated: (index, row) => {
                    if (!InputMethods.busy && row.groupName)
                        InputMethods.switchGroup(row.groupName)
                }
            }

            MeoCard {
                width: parent.width
                type: "filled"
                visible: InputMethods.available

                ColumnLayout {
                    anchors.fill: parent
                    spacing: MeoTheme.space12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space12

                        MeoIcon {
                            icon: InputMethods.active ? "check_circle" : "info"
                            size: 28
                            color: InputMethods.active ? MeoTheme.primary : MeoTheme.tertiary
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MeoTheme.space2

                            MeoText {
                                Layout.fillWidth: true
                                text: InputMethods.active ? qsTr("Fcitx is active") : qsTr("Fcitx is available")
                                typeRole: "title"
                                typeSize: "small"
                                emphasized: true
                                color: MeoTheme.contentOnSurface
                            }

                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Group: %1 · Current engine: %2")
                                      .arg(InputMethods.currentGroup || qsTr("Unknown"))
                                      .arg(InputMethods.currentInputMethod
                                           ? root.inputMethodName(InputMethods.currentInputMethod)
                                           : qsTr("None"))
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space8

                        MeoButton {
                            text: qsTr("Refresh")
                            icon.name: "refresh"
                            type: "tonal"
                            enabled: !InputMethods.busy
                            onClicked: InputMethods.refresh()
                        }

                        MeoButton {
                            text: qsTr("Reload configuration")
                            icon.name: "sync"
                            type: "tonal"
                            enabled: InputMethods.available && !InputMethods.busy
                            onClicked: InputMethods.reload()
                        }

                        MeoButton {
                            visible: InputMethods.canRestart
                            text: qsTr("Restart Fcitx")
                            icon.name: "restart_alt"
                            type: "text"
                            enabled: !InputMethods.busy
                            onClicked: InputMethods.restart()
                        }
                    }
                }
            }

            MeoCard {
                width: parent.width
                type: "outlined"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: MeoTheme.space12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space12

                        MeoIcon {
                            icon: "add_circle"
                            size: 28
                            color: MeoTheme.primary
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MeoTheme.space2

                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("More languages and engines")
                                typeRole: "title"
                                typeSize: "small"
                                emphasized: true
                                color: MeoTheme.contentOnSurface
                            }

                            MeoText {
                                Layout.fillWidth: true
                                text: qsTr("Optional Pinyin, Rime, Mozc, Hangul, and other engines are installed through Meo's app and package flow. Settings never runs pacman directly.")
                                typeRole: "body"
                                typeSize: "small"
                                color: MeoTheme.contentOnSurfaceVariant
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MeoTheme.space8

                        MeoButton {
                            text: qsTr("Open Apps")
                            icon.name: "apps"
                            type: "filled"
                            onClicked: root.navigateTo("applications")
                        }

                        MeoButton {
                            text: qsTr("Advanced Fcitx settings")
                            icon.name: "tune"
                            type: "text"
                            onClicked: root.navigateTo("kcm:kcm_fcitx5")
                        }
                    }
                }
            }
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

    Component.onCompleted: {
        WeatherBackend.refresh()
        InputMethods.refresh()
    }
}
