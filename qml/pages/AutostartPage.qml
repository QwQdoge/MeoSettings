import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property string applicationId: ""
    property var pendingRemoval: null
    MeoSettingsTaskSheet {
        id: removal; popupParent: Overlay.overlay
        title: root.pendingRemoval && root.pendingRemoval.systemEntry ? qsTr("Restore system startup preference?") : qsTr("Remove personal startup entry?")
        subtitle: root.pendingRemoval && root.pendingRemoval.systemEntry ? qsTr("Removing this personal override restores the system entry, which may enable the application at next sign-in.") : qsTr("This removes the sign-in entry only. The application remains installed and running applications are unchanged.")
        acceptText: qsTr("Remove personal entry"); rejectText: qsTr("Cancel")
        onAccepted: { if (root.pendingRemoval) AutostartBackend.removeUserEntry(root.pendingRemoval.id); root.pendingRemoval = null }
        onRejected: root.pendingRemoval = null
    }
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics
        title: qsTr("Autostart")
        subtitle: qsTr("Choose applications allowed to start at your next sign-in")
        MeoBanner { width: parent.width; visible: AutostartBackend.error !== ""; title: qsTr("Startup preference needs attention"); text: AutostartBackend.error; tone: "error" }
        MeoText {
            width: parent.width; wrapMode: Text.WordWrap
            text: qsTr("Switches control the desktop entry's startup permission. Desktop restrictions and provider conditions still apply. Session restoration and independently enabled background services are separate.")
        }
        Repeater {
            model: AutostartBackend.entries
            delegate: Column {
                required property var modelData
                width: parent.width; spacing: MeoTheme.space8
                MeoSettingsGroup {
                    width: parent.width
                    model: [{"title": modelData.label, "subtitle": modelData.detail, "trailingKind": "toggle", "checked": modelData.enabled, "enabled": modelData.editable}]
                    onRowToggled: (index, checked, row) => AutostartBackend.setEnabled(modelData.id, checked)
                }
                MeoButton {
                    visible: modelData.local
                    text: modelData.systemEntry ? qsTr("Restore system default") : qsTr("Remove personal entry")
                    type: "text"
                    onClicked: { root.pendingRemoval = modelData; removal.open() }
                }
            }
        }
        MeoEmptyState { width: parent.width; visible: AutostartBackend.entries.length === 0; icon: "play_circle"; title: qsTr("No desktop startup entries"); description: qsTr("Add an installed application below to start it when you sign in.") }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Installed application")
            model: AutostartBackend.applications.map(app => app.label)
            text: (AutostartBackend.applications.find(app => app.id === root.applicationId) || {}).label || qsTr("Choose an application")
            onSelected: (index, value) => root.applicationId = AutostartBackend.applications[index].id
        }
        MeoButton { text: qsTr("Add to startup"); enabled: root.applicationId !== ""; onClicked: AutostartBackend.addApplication(root.applicationId) }
        MeoButton { text: qsTr("Refresh"); type: "tonal"; onClicked: AutostartBackend.refresh() }
        MeoButton { text: qsTr("Advanced startup and scripts"); type: "text"; enabled: KcmBridge.isAvailable("kcm_autostart"); onClicked: root.navigateTo("kcm:kcm_autostart") }
    }
}
