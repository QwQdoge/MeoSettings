import QtQuick
import QtQuick.Controls
import MeoUI

Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property bool isCompact: rootMetrics && rootMetrics.isCompactWidth
    readonly property var displayFacts: {
        const facts = []
        const systemFacts = SystemInfoBackend.facts
        for (let index = 0; index < systemFacts.length; ++index) {
            const fact = systemFacts[index]
            if (fact.label === qsTr("User") && AccountBackend.signedIn) {
                facts.push({
                    "label": qsTr("Meo Account"),
                    "value": AccountBackend.cloudId !== ""
                             ? qsTr("%1 · %2").arg(AccountBackend.cloudName || qsTr("Meo Account"))
                                                   .arg(AccountBackend.cloudId)
                             : (AccountBackend.cloudName || qsTr("Meo Account"))
                })
            } else {
                facts.push(fact)
            }
        }
        return facts
    }

    MeoPageLayout {
        id: page
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.isCompact ? "" : qsTr("About")
        subtitle: qsTr("Live information from the operating system and Qt/KDE runtime.")

        MeoCard {
            width: parent.width
            type: "elevated"

            Column {
                width: parent.width
                spacing: 0

                Repeater {
                    model: root.displayFacts
                    delegate: MeoListItem {
                        required property var modelData
                        width: parent.width
                        headline: modelData.label
                        supportingText: modelData.value
                        leadingIcon: modelData.label === qsTr("Operating system") ? "computer"
                                     : (modelData.label === qsTr("Memory") ? "memory" : "info")
                        interactive: false
                    }
                }
            }
        }

        MeoButton {
            text: qsTr("Refresh information")
            type: "tonal"
            onClicked: SystemInfoBackend.refresh()
        }

        MeoButton {
            text: qsTr("Advanced system information")
            type: "text"
            enabled: KcmBridge.isAvailable("kcm_about-distro")
            onClicked: root.navigateTo("kcm:kcm_about-distro")
        }

        MeoCard {
            width: parent.width
            type: "outlined"

            Column {
                width: parent.width
                spacing: 8 * MeoTheme.globalScale
                MeoText {
                    text: qsTr("Architecture boundary")
                    typeRole: "title"
                    typeSize: "small"
                    emphasized: true
                    color: MeoTheme.contentOnSurface
                }
                MeoText {
                    width: parent.width
                    text: qsTr("Meo Settings provides MeoUI presentation and thin adapters. NetworkManager, BlueZ, the audio service, KScreen, and KDE configuration modules remain the system authorities.")
                    typeRole: "body"
                    typeSize: "small"
                    color: MeoTheme.contentOnSurfaceVariant
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
