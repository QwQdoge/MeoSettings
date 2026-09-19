import QtQuick
import MeoUI

Column {
    id: root

    required property string category
    required property string entryTitle

    width: parent ? parent.width : implicitWidth
    spacing: MeoTheme.space4
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Help and diagnostics")
    Accessible.description: qsTr("Find safe next steps for this part of your system.")

    MeoSettingsGroup {
        id: repairGroup
        width: parent.width
        title: qsTr("Get help")
        subtitle: qsTr("Find safe next steps for this part of your system.")
        model: [{
            "id": "open-repair",
            "title": root.entryTitle,
            "subtitle": RepairLauncher.error !== ""
                        ? qsTr("A diagnostic could not start. Check the service, then try again.")
                        : (RepairLauncher.available
                           ? qsTr("Open a guided, one-question-at-a-time diagnosis")
                           : qsTr("MeoArch Repair is not installed on this system")),
            "icon": "build",
            "tone": RepairLauncher.error !== "" ? "error" : "secondary",
            "enabled": RepairLauncher.available,
            "trailingKind": "action",
            "actionText": RepairLauncher.available ? qsTr("Start diagnosis") : qsTr("Unavailable")
        }]

        onRowActivated: (index, row) => root.openRepair()
        onRowActionTriggered: (index, row) => root.openRepair()
    }

    MeoText {
        width: parent.width
        visible: RepairLauncher.error !== ""
        text: qsTr("Technical details: %1").arg(RepairLauncher.error)
        Accessible.name: text
        typeRole: "label"
        typeSize: "small"
        color: MeoTheme.contentOnSurfaceVariant
        wrapMode: Text.WordWrap
    }

    function openRepair() {
        if (RepairLauncher.available)
            RepairLauncher.open(root.category)
    }

}
