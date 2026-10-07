import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeoUI
import Meo.System 1.0

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth

    function availableEntry(inputMethodId) {
        const inventory = InputMethods.availableInputMethods || []
        for (let index = 0; index < inventory.length; ++index) {
            if (String(inventory[index].id || "") === inputMethodId)
                return inventory[index]
        }
        return ({})
    }

    function displayName(inputMethodId) {
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

    readonly property var activeRows: {
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
            let details = ""
            if (language !== "" && layout !== "")
                details = qsTr("%1 · keyboard layout %2").arg(language).arg(layout)
            else if (language !== "")
                details = language
            else if (layout !== "")
                details = qsTr("Keyboard layout %1").arg(layout)
            else
                details = qsTr("Available in the current Fcitx group")
            rows.push({
                "id": "input-method-" + inputMethodId,
                "inputMethodId": inputMethodId,
                "title": root.displayName(inputMethodId),
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

    readonly property var groupRows: {
        const rows = []
        const groups = InputMethods.groups || []
        for (let index = 0; index < groups.length; ++index) {
            const group = String(groups[index] || "")
            if (group === "")
                continue
            rows.push({
                "id": "input-group-" + group,
                "groupName": group,
                "title": group,
                "subtitle": group === InputMethods.currentGroup
                            ? qsTr("Current input-method group")
                            : qsTr("Switch to this Fcitx group"),
                "icon": "language",
                "tone": "primary",
                "trailingKind": "radio",
                "checked": group === InputMethods.currentGroup,
                "enabled": InputMethods.available && !InputMethods.busy
            })
        }
        return rows
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        compactWidth: 680 * MeoTheme.globalScale
        mediumWidth: 760 * MeoTheme.globalScale
        expandedWidth: 760 * MeoTheme.globalScale
        title: root.isCompact ? "" : qsTr("Input methods")
        subtitle: qsTr("Switch languages and input engines without leaving Meo Settings")

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
                            text: qsTr("Meo Settings does not start a second input-method process. On Plasma Wayland, KWin owns the Fcitx session. You can refresh after the session runtime becomes available.")
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
            visible: InputMethods.available && root.activeRows.length > 0
            title: qsTr("Current input methods")
            subtitle: qsTr("Choose which installed engine receives keyboard input now")
            model: root.activeRows
            onRowActivated: (index, row) => {
                if (!InputMethods.busy && row.inputMethodId)
                    InputMethods.setCurrentInputMethod(row.inputMethodId)
            }
        }

        MeoSettingsGroup {
            width: parent.width
            visible: InputMethods.available && root.groupRows.length > 1
            title: qsTr("Input-method groups")
            subtitle: qsTr("Fcitx groups keep separate ordered sets of input methods")
            model: root.groupRows
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
                                       ? root.displayName(InputMethods.currentInputMethod)
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
                            text: qsTr("Install optional Pinyin, Rime, Mozc, Hangul, or other engine packages through Meo's app and package flow. Settings never runs pacman directly.")
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

    Component.onCompleted: InputMethods.refresh()
}
