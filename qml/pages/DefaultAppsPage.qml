import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property bool fileTypesOnly: false
    property string selectedMime: "text/plain"
    property var candidates: []
    property string preferred: ""
    property bool dataReady: false

    function refreshSelection() {
        if (!dataReady)
            return
        candidates = DefaultAppsBackend.applicationsFor(selectedMime)
        preferred = DefaultAppsBackend.preferredApplication(selectedMime)
    }

    onSelectedMimeChanged: refreshSelection()

    Connections {
        target: DefaultAppsBackend
        function onChanged() {
            root.dataReady = DefaultAppsBackend.mimeTypes.length > 0
            root.refreshSelection()
        }
    }

    Timer {
        interval: 50
        repeat: false
        running: true
        onTriggered: DefaultAppsBackend.refresh()
    }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.fileTypesOnly ? qsTr("File associations") : qsTr("Default apps")

        MeoCard {
            width: parent.width
            type: "filled"
            visible: !root.dataReady
            Column {
                width: parent.width
                spacing: 10 * MeoTheme.globalScale
                MeoText {
                    width: parent.width
                    text: qsTr("Loading application associations")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("The page is ready. Installed application and file-type data is loading in the background.")
                    typeRole: "body"
                    typeSize: "medium"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
                MeoProgressBar {
                    width: parent.width
                    type: "linear"
                    indeterminate: true
                }
            }
        }

        MeoBanner {
            width: parent.width
            visible: DefaultAppsBackend.error !== ""
            title: qsTr("Application association needs attention")
            text: DefaultAppsBackend.error
            tone: "error"
        }

        Repeater {
            model: !root.dataReady || root.fileTypesOnly ? [] : DefaultAppsBackend.roles
            delegate: MeoExposedDropdown {
                required property var modelData
                width: parent.width
                label: modelData.label
                model: modelData.applications.map(app => app.label)
                text: modelData.applications.find(app => app.id === modelData.preferred)?.label || qsTr("No default application")
                enabled: root.dataReady && !DefaultAppsBackend.busy && modelData.applications.length > 0
                onSelected: (index, value) => DefaultAppsBackend.setPreferredApplication(modelData.mime, modelData.applications[index].id)
            }
        }

        MeoTextField {
            id: mimeSearch
            width: parent.width
            visible: root.dataReady
            label: qsTr("Find a file type")
            placeholder: "text, image, application…"
        }

        MeoExposedDropdown {
            width: parent.width
            visible: root.dataReady
            label: qsTr("File or link type")
            model: DefaultAppsBackend.mimeTypes.filter(mime => mime.toLowerCase().includes(mimeSearch.text.toLowerCase()))
            text: root.selectedMime
            enabled: root.dataReady && !DefaultAppsBackend.busy
            onSelected: (index, value) => root.selectedMime = value
        }

        MeoExposedDropdown {
            width: parent.width
            visible: root.dataReady
            label: qsTr("Open with")
            model: root.candidates.map(app => app.label)
            text: root.candidates.find(app => app.id === root.preferred)?.label || qsTr("No default application")
            enabled: root.dataReady && !DefaultAppsBackend.busy && root.candidates.length > 0
            onSelected: (index, value) => DefaultAppsBackend.setPreferredApplication(root.selectedMime, root.candidates[index].id)
        }

        MeoEmptyState {
            width: parent.width
            visible: root.dataReady && root.candidates.length === 0
            icon: "apps"
            title: qsTr("No compatible applications")
            description: qsTr("Install an application that supports the selected type.")
        }

        MeoButton {
            text: qsTr("Advanced associations")
            type: "text"
            onClicked: root.navigateTo("kcm:kcm_filetypes")
        }
    }
}
