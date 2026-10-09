import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property string deviceFilter: "all"
    property string disableDeviceId: ""
    readonly property var visibleDevices: InputDevices.devices.filter(device =>
        deviceFilter === "touchpad" ? device.touchpad : deviceFilter === "mouse" ? !device.touchpad : true)

    function optionRows(device) {
        const options = [
            ["enabled", qsTr("Enable this input device")],
            ["leftHanded", qsTr("Left-handed buttons")],
            ["naturalScroll", qsTr("Natural scrolling")],
            ["tapToClick", qsTr("Tap to click")],
            ["tapAndDrag", qsTr("Tap and drag")],
            ["tapDragLock", qsTr("Keep dragging after lifting a finger")],
            ["disableWhileTyping", qsTr("Disable while typing")],
            ["disableEventsOnExternalMouse", qsTr("Disable when a mouse is connected")],
            ["middleEmulation", qsTr("Emulate middle click")],
            ["scrollTwoFinger", qsTr("Two-finger scrolling")],
            ["scrollEdge", qsTr("Edge scrolling")],
            ["clickMethodAreas", qsTr("Click using button areas")],
            ["clickMethodClickfinger", qsTr("Click using finger count")],
            ["pointerAccelerationProfileAdaptive", qsTr("Adaptive acceleration")],
            ["pointerAccelerationProfileFlat", qsTr("Flat acceleration")]
        ]
        return options.filter(option => device.capabilities[option[0]]).map(option => ({
            "id": option[0], "title": option[1], "icon": "mouse", "trailingKind": "toggle",
            "checked": Boolean(device.values[option[0]]), "enabled": !InputDevices.busy
        }))
    }

    MeoSettingsTaskSheet {
        id: disableDevice; popupParent: Overlay.overlay
        title: qsTr("Disable this pointing device?")
        subtitle: qsTr("Use another pointing device or keyboard navigation to enable it again. This change takes effect immediately.")
        acceptText: qsTr("Disable"); rejectText: qsTr("Cancel")
        onAccepted: InputDevices.setValue(root.disableDeviceId, "enabled", false)
        onRejected: InputDevices.refresh()
    }
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.deviceFilter === "touchpad" ? qsTr("Touchpad") : qsTr("Mouse & pointing devices")
        MeoBanner {
            width: parent.width
            visible: InputDevices.error !== ""
            title: qsTr("Input setting needs attention")
            text: InputDevices.error
            tone: "error"
        }
        MeoEmptyState {
            width: parent.width
            visible: root.visibleDevices.length === 0
            icon: "mouse"
            title: InputDevices.available ? qsTr("No matching input devices") : qsTr("Input service unavailable")
            description: qsTr("Supported controls appear when KWin reports a compatible device.")
        }
        Repeater {
            model: root.visibleDevices
            delegate: Column {
                required property var modelData
                width: parent.width
                spacing: MeoTheme.space12
                MeoSettingsGroup {
                    width: parent.width
                    title: modelData.name
                    model: root.optionRows(modelData)
                    onRowToggled: (index, checked, row) => {
                        if (row.id === "enabled" && !checked) { root.disableDeviceId = modelData.id; disableDevice.open() }
                        else InputDevices.setValue(modelData.id, row.id, checked)
                    }
                }
                MeoSteppedSlider {
                    width: parent.width
                    visible: modelData.capabilities.pointerAcceleration
                    title: qsTr("Pointer speed")
                    from: -1; to: 1; stepSize: 0.05
                    value: Number(modelData.values.pointerAcceleration)
                    valueText: Number(value).toFixed(2)
                    showValueLabel: true
                    enabled: !InputDevices.busy
                    onMoved: value => InputDevices.setValue(modelData.id, "pointerAcceleration", value)
                }
                MeoSteppedSlider {
                    width: parent.width
                    visible: modelData.capabilities.scrollFactor
                    title: qsTr("Scroll speed")
                    from: 0.1; to: 5; stepSize: 0.1
                    value: Number(modelData.values.scrollFactor)
                    valueText: Number(value).toFixed(1) + "×"
                    showValueLabel: true
                    enabled: !InputDevices.busy
                    onMoved: value => InputDevices.setValue(modelData.id, "scrollFactor", value)
                }
            }
        }
        MeoButton { text: qsTr("Refresh devices"); type: "tonal"; enabled: !InputDevices.busy; onClicked: InputDevices.refresh() }
        MeoButton {
            text: qsTr("Advanced input settings")
            type: "text"
            onClicked: root.navigateTo(root.deviceFilter === "touchpad" ? "kcm:kcm_touchpad" : "kcm:kcm_mouse")
        }
    }
}
