import QtQuick
import MeoUI

// Native page for KDE-owned settings described by KdeConfigBackend's
// whitelisted schema. One page renders every such route, so each route gets
// the same hero, feedback, reset and escape-hatch behavior.
Item {
    id: root

    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property string schemaId: ""

    readonly property var entryInfo: SettingsRegistry.entry(schemaId)
    readonly property bool hasSections: groupsView.hasContent
    readonly property var expertModules: ({
        "window-behavior": "kcm_kwinoptions",
        "desktop-effects": "kcm_kwin_effects",
        "screen-edges": "kcm_kwinscreenedges",
        "virtual-desktops": "kcm_kwin_virtualdesktops",
        "search-indexing": "kcm_baloofile",
        "startup-session": "kcm_smserver",
        "spell-checking": "kcmspellchecking",
        "workspace": "kcm_workspace",
        "automount": "kcm_device_automounter",
        "wallet": "kcm_kwallet5",
        "sound-theme": "kcm_soundtheme"
    })
    readonly property var heroShapes: ({
        "window-behavior": ["Square", "Cookie4Sided"],
        "desktop-effects": ["Sunny", "Flower"],
        "screen-edges": ["Square", "Diamond"],
        "virtual-desktops": ["Clover4Leaf", "Square"],
        "search-indexing": ["Circle", "Cookie6Sided"],
        "startup-session": ["Pill", "Cookie9Sided"],
        "spell-checking": ["Puffy", "Cookie7Sided"],
        "workspace": ["Gem", "Cookie12Sided"],
        "automount": ["Pill", "Square"],
        "wallet": ["Hexagon", "Clover4Leaf"],
        "sound-theme": ["Circle", "SoftBurst"]
    })
    readonly property string expertModule: expertModules[schemaId] || ""

    ApplyToast { id: toast }

    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: root.entryInfo.title || ""
        subtitle: root.entryInfo.description || ""

        PageHero {
            anchors.horizontalCenter: parent.horizontalCenter
            icon: root.entryInfo.icon || "settings"
            tone: root.entryInfo.tone || "neutral"
            shapeA: (root.heroShapes[root.schemaId] || ["SoftBurst", "Cookie9Sided"])[0]
            shapeB: (root.heroShapes[root.schemaId] || ["SoftBurst", "Cookie9Sided"])[1]
            artSize: 112 * MeoTheme.globalScale
        }

        MeoBanner {
            width: parent.width
            visible: KdeConfigBackend.error !== ""
            title: qsTr("Setting needs attention")
            text: KdeConfigBackend.error
            tone: "error"
        }
        MeoBanner {
            width: parent.width
            visible: !KdeConfigBackend.kwinRunning && root.schemaId.match(/^(window-behavior|desktop-effects|screen-edges|virtual-desktops)$/) !== null
            title: qsTr("Window manager is not running")
            text: qsTr("Changes are saved and will apply the next time you sign in.")
        }
        MeoEmptyState {
            width: parent.width
            height: 220 * MeoTheme.globalScale
            visible: !root.hasSections
            icon: "settings"
            title: qsTr("Nothing to configure here")
            description: qsTr("This page has no settings available on this device.")
        }

        ConfigSchemaGroups {
            id: groupsView
            width: parent.width
            schemaId: root.schemaId
            onApplied: (message) => toast.show(message)
        }

        MeoButton {
            visible: root.hasSections
            text: qsTr("Restore defaults")
            type: "tonal"
            onClicked: {
                KdeConfigBackend.resetPage(root.schemaId)
                toast.show(qsTr("Defaults restored"))
            }
        }
        MeoButton {
            visible: root.expertModule !== ""
            text: qsTr("Advanced options")
            type: "text"
            enabled: root.expertModule !== "" && KcmBridge.isAvailable(root.expertModule)
            onClicked: root.navigateTo("kcm:" + root.expertModule)
        }
    }
}
