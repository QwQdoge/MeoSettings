import QtQuick
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property var selectedIds: []
    property string configuredKey: InputMethods.currentGroup + JSON.stringify(InputMethods.activeInputMethods)
    property var choices: InputMethods.availableInputMethods.filter(method => ((method.nativeName || method.name || method.label) + " " + method.id + " " + method.language).toLowerCase().includes(methodSearch.text.toLowerCase()))
    function syncSelection() { selectedIds = InputMethods.activeInputMethods.map(method => method.id) }
    onConfiguredKeyChanged: syncSelection()
    Component.onCompleted: { syncSelection(); InputMethods.refresh() }
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics; title: qsTr("Input methods")
        MeoBanner { width: parent.width; visible: InputMethods.lastError !== ""; title: qsTr("Input method needs attention"); text: InputMethods.lastError; tone: "error" }
        MeoEmptyState { width: parent.width; visible: !InputMethods.available; icon: "language"; title: qsTr("Input method service unavailable"); description: qsTr("Fcitx 5 must be running in this desktop session.") }
        MeoExposedDropdown { width: parent.width; label: qsTr("Input group"); model: InputMethods.groups; text: InputMethods.currentGroup; enabled: InputMethods.available && !InputMethods.busy; onSelected: (index, value) => InputMethods.switchGroup(value) }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Active input method")
            model: InputMethods.activeInputMethods.map(method => InputMethods.availableInputMethods.find(choice => choice.id === method.id)?.name || method.id)
            text: InputMethods.availableInputMethods.find(method => method.id === InputMethods.currentInputMethod)?.name || InputMethods.currentInputMethod
            enabled: InputMethods.available && !InputMethods.busy
            onSelected: (index, value) => InputMethods.setCurrentInputMethod(InputMethods.activeInputMethods[index].id)
        }
        MeoText { text: qsTr("Methods in this group"); typeRole: "title"; typeSize: "small" }
        Repeater {
            model: root.selectedIds
            delegate: Column {
                required property string modelData
                required property int index
                width: parent.width
                MeoText { text: InputMethods.availableInputMethods.find(method => method.id === parent.modelData)?.name || parent.modelData }
                Flow {
                    width: parent.width; spacing: MeoTheme.space8
                    MeoButton {
                        text: qsTr("Move up"); type: "text"; enabled: parent.parent.index > 0 && !InputMethods.busy
                        onClicked: { const ids = root.selectedIds.slice(); const i = parent.parent.index; const id = ids.splice(i, 1)[0]; ids.splice(i - 1, 0, id); root.selectedIds = ids }
                    }
                    MeoButton {
                        text: qsTr("Move down"); type: "text"; enabled: parent.parent.index < root.selectedIds.length - 1 && !InputMethods.busy
                        onClicked: { const ids = root.selectedIds.slice(); const i = parent.parent.index; const id = ids.splice(i, 1)[0]; ids.splice(i + 1, 0, id); root.selectedIds = ids }
                    }
                    MeoButton {
                        text: qsTr("Remove"); type: "text"; enabled: !InputMethods.busy
                        onClicked: { const ids = root.selectedIds.slice(); ids.splice(parent.parent.index, 1); root.selectedIds = ids }
                    }
                }
            }
        }
        MeoTextField { id: methodSearch; width: parent.width; label: qsTr("Find an input method"); placeholder: qsTr("Search by language, name or identifier") }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Add input method"); model: root.choices.map(method => method.nativeName || method.name || method.label || method.id)
            enabled: InputMethods.available && !InputMethods.busy
            onSelected: (index, value) => { const id = root.choices[index].id; if (root.selectedIds.indexOf(id) < 0) root.selectedIds = root.selectedIds.concat([id]) }
        }
        MeoButton { text: qsTr("Apply input methods"); enabled: InputMethods.available && !InputMethods.busy && root.selectedIds.length > 0; onClicked: InputMethods.configureMethods(root.selectedIds) }
        MeoButton { text: qsTr("Refresh"); type: "text"; enabled: !InputMethods.busy; onClicked: InputMethods.refresh() }
        MeoButton { text: qsTr("Advanced engine options"); type: "text"; onClicked: root.navigateTo("kcm:kcm_fcitx5") }
    }
}
