import QtQuick
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedIds: []
    property string configuredKey: InputMethods.currentGroup + JSON.stringify(InputMethods.configuredMethods)
    property var choices: InputMethods.methods.filter(method => (method.label + " " + method.id + " " + method.language).toLowerCase().includes(methodSearch.text.toLowerCase()))
    function syncSelection() { selectedIds = InputMethods.configuredMethods.map(method => method.id) }
    onConfiguredKeyChanged: syncSelection()
    Component.onCompleted: syncSelection()
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics; title: qsTr("Input methods")
        MeoBanner { width: parent.width; visible: InputMethods.error !== ""; title: qsTr("Input method needs attention"); text: InputMethods.error; tone: "error" }
        MeoEmptyState { width: parent.width; visible: !InputMethods.available; icon: "language"; title: qsTr("Input method service unavailable"); description: qsTr("Fcitx 5 must be running in this desktop session.") }
        MeoExposedDropdown { width: parent.width; label: qsTr("Input group"); model: InputMethods.groups; text: InputMethods.currentGroup; enabled: InputMethods.available && !InputMethods.busy; onSelected: (index, value) => InputMethods.selectGroup(value) }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Active input method")
            model: InputMethods.configuredMethods.map(method => InputMethods.methods.find(choice => choice.id === method.id)?.label || method.id)
            text: InputMethods.methods.find(method => method.id === InputMethods.currentMethod)?.label || InputMethods.currentMethod
            enabled: InputMethods.available && !InputMethods.busy
            onSelected: (index, value) => InputMethods.activateMethod(InputMethods.configuredMethods[index].id)
        }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Methods in this group")
            model: root.selectedIds.map(id => ({"id": id, "title": InputMethods.methods.find(method => method.id === id)?.label || id, "subtitle": qsTr("Select to remove; keep a keyboard layout"), "trailingKind": "chevron"}))
            onRowActivated: (index, row) => { const ids = root.selectedIds.slice(); ids.splice(index, 1); root.selectedIds = ids }
        }
        MeoTextField { id: methodSearch; width: parent.width; label: qsTr("Find an input method"); placeholder: qsTr("Search by language, name or identifier") }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Add input method"); model: root.choices.map(method => method.label)
            enabled: InputMethods.available && !InputMethods.busy
            onSelected: (index, value) => { const id = root.choices[index].id; if (root.selectedIds.indexOf(id) < 0) root.selectedIds = root.selectedIds.concat([id]) }
        }
        MeoButton { text: qsTr("Apply input methods"); enabled: InputMethods.available && !InputMethods.busy && root.selectedIds.length > 0; onClicked: InputMethods.configureMethods(root.selectedIds) }
        MeoButton { text: qsTr("Refresh"); type: "text"; enabled: !InputMethods.busy; onClicked: InputMethods.refresh() }
        MeoButton { text: qsTr("Advanced engine options"); type: "text"; onClicked: root.navigateTo("kcm:kcm_fcitx5") }
    }
}
