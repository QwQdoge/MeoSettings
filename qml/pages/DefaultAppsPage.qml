import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property bool fileTypesOnly: false
    property string selectedMime: "text/plain"
    property var candidates: DefaultAppsBackend.applicationsFor(selectedMime)
    property string preferred: DefaultAppsBackend.preferredApplication(selectedMime)
    function refreshSelection() {
        candidates = DefaultAppsBackend.applicationsFor(selectedMime)
        preferred = DefaultAppsBackend.preferredApplication(selectedMime)
    }
    onSelectedMimeChanged: refreshSelection()
    Connections { target: DefaultAppsBackend; function onChanged() { root.refreshSelection() } }
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.fileTypesOnly ? qsTr("File associations") : qsTr("Default apps")
        MeoBanner { width: parent.width; visible: DefaultAppsBackend.error !== ""; title: qsTr("Application association needs attention"); text: DefaultAppsBackend.error; tone: "error" }
        Repeater {
            model: root.fileTypesOnly ? [] : DefaultAppsBackend.roles
            delegate: MeoExposedDropdown {
                required property var modelData
                width: parent.width
                label: modelData.label
                model: modelData.applications.map(app => app.label)
                text: modelData.applications.find(app => app.id === modelData.preferred)?.label || qsTr("No default application")
                enabled: !DefaultAppsBackend.busy && modelData.applications.length > 0
                onSelected: (index, value) => DefaultAppsBackend.setPreferredApplication(modelData.mime, modelData.applications[index].id)
            }
        }
        MeoTextField { id: mimeSearch; width: parent.width; label: qsTr("Find a file type"); placeholder: "text, image, application…" }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("File or link type")
            model: DefaultAppsBackend.mimeTypes.filter(mime => mime.toLowerCase().includes(mimeSearch.text.toLowerCase()))
            text: root.selectedMime
            enabled: !DefaultAppsBackend.busy
            onSelected: (index, value) => root.selectedMime = value
        }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("Open with")
            model: root.candidates.map(app => app.label)
            text: root.candidates.find(app => app.id === root.preferred)?.label || qsTr("No default application")
            enabled: !DefaultAppsBackend.busy && root.candidates.length > 0
            onSelected: (index, value) => DefaultAppsBackend.setPreferredApplication(root.selectedMime, root.candidates[index].id)
        }
        MeoEmptyState {
            width: parent.width
            visible: root.candidates.length === 0
            icon: "apps"
            title: qsTr("No compatible applications")
            description: qsTr("Install an application that supports the selected type.")
        }
        MeoButton { text: qsTr("Advanced associations"); type: "text"; onClicked: root.navigateTo("kcm:kcm_filetypes") }
    }
}
