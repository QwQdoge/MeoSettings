import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    function rows(options) {
        return options.map(option => ({"id": option[0], "title": option[1], "subtitle": option[2] || "", "trailingKind": "toggle", "checked": Boolean(AccessibilityBackend.values[option[0]]), "enabled": Boolean(AccessibilityBackend.capabilities[option[0]]) && !AccessibilityBackend.busy}))
    }
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics; title: qsTr("Accessibility")
        MeoBanner { width: parent.width; visible: AccessibilityBackend.error !== ""; title: qsTr("Accessibility needs attention"); text: AccessibilityBackend.error; tone: "error" }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Motion and vision")
            model: root.rows([["reduceMotion", qsTr("Reduce motion")], ["zoom", qsTr("Enable screen zoom"), qsTr("Use the configured zoom shortcuts")], ["magnifier", qsTr("Enable magnifier"), qsTr("Use the configured magnifier shortcut")], ["invert", qsTr("Enable inverted colors"), qsTr("Use the configured color inversion shortcut")], ["shakePointer", qsTr("Enlarge pointer when shaken")]])
            onRowToggled: (index, checked, row) => AccessibilityBackend.setOption(row.id, checked)
        }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Keyboard assistance")
            model: root.rows([["stickyKeys", qsTr("Sticky modifier keys")], ["stickyLatch", qsTr("Lock modifiers when pressed twice")], ["stickyAutoOff", qsTr("Disable sticky keys when two keys are pressed together")], ["mouseKeys", qsTr("Move pointer with numeric keypad")]])
            onRowToggled: (index, checked, row) => AccessibilityBackend.setOption(row.id, checked)
        }
        MeoButton { text: qsTr("Fonts and text sizes"); type: "tonal"; onClicked: root.navigateTo("fonts") }
        MeoButton { text: qsTr("Advanced assistive technology"); type: "text"; onClicked: root.navigateTo("kcm:kcm_access") }
    }
}
