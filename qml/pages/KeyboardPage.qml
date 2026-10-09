import QtQuick
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedIds: []
    property var choices: InputDevices.layoutChoices.filter(choice => (choice.label + " " + choice.id).toLowerCase().includes(layoutSearch.text.toLowerCase()))
    function syncSelection() { selectedIds = InputDevices.configuredLayouts.slice() }
    Component.onCompleted: syncSelection()
    property string configuredKey: JSON.stringify(InputDevices.configuredLayouts)
    onConfiguredKeyChanged: syncSelection()
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Keyboard")
        MeoBanner { width: parent.width; visible: InputDevices.error !== ""; title: qsTr("Keyboard setting needs attention"); text: InputDevices.error; tone: "error" }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("Active layout")
            model: InputDevices.keyboardLayouts.map(layout => layout.label)
            text: InputDevices.keyboardLayouts[InputDevices.activeKeyboardLayout]?.label || ""
            enabled: !InputDevices.busy && InputDevices.keyboardLayouts.length > 0
            onSelected: (index, value) => InputDevices.activateKeyboardLayout(index)
        }
        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Configured layouts")
            model: root.selectedIds.map(id => ({"id": id, "title": InputDevices.layoutChoices.find(choice => choice.id === id)?.label || id, "trailingKind": "chevron", "subtitle": qsTr("Select to remove")}))
            onRowActivated: (index, row) => { const ids = root.selectedIds.slice(); ids.splice(index, 1); root.selectedIds = ids }
        }
        MeoTextField { id: layoutSearch; width: parent.width; label: qsTr("Find a layout"); placeholder: qsTr("Search language or layout") }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("Add layout")
            model: root.choices.map(choice => choice.label)
            enabled: !InputDevices.busy && root.selectedIds.length < 4
            onSelected: (index, value) => {
                const id = root.choices[index].id
                if (root.selectedIds.indexOf(id) < 0) root.selectedIds = root.selectedIds.concat([id])
            }
        }
        MeoButton { text: qsTr("Apply layouts"); enabled: !InputDevices.busy && root.selectedIds.length > 0; onClicked: InputDevices.configureKeyboardLayouts(root.selectedIds) }
        MeoButton { text: qsTr("Advanced keyboard settings"); type: "text"; onClicked: root.navigateTo("kcm:kcm_keyboard") }
        MeoButton { text: qsTr("Keyboard shortcuts"); type: "text"; onClicked: root.navigateTo("kcm:kcm_keys") }
    }
}
