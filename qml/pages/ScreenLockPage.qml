import QtQuick
import QtQuick.Controls
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    readonly property var policy: Platform.screenLockPolicy
    property bool automatic: policy.automatic
    property bool onResume: policy.onResume
    property bool onStart: policy.onStart
    function applyPolicy() {
        Platform.configureScreenLock(automatic, Number(timeout.text), onResume, onStart, Number(grace.text))
    }
    Connections {
        target: Platform
        function onScreenLockPolicyChanged() {
            if (Platform.screenLockPolicyBusy) return
            const current = Platform.screenLockPolicy
            root.automatic = current.automatic; root.onResume = current.onResume; root.onStart = current.onStart
            timeout.text = String(current.minutes); grace.text = String(current.graceSeconds)
        }
    }
    MeoSettingsTaskSheet {
        id: reducedProtection; popupParent: Overlay.overlay
        title: qsTr("Change screen lock protection?")
        subtitle: qsTr("These preferences may leave the session unlocked for longer. Apply the idle, resume and grace settings shown on this page?")
        acceptText: qsTr("Apply"); rejectText: qsTr("Cancel")
        onAccepted: root.applyPolicy()
    }
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics
        title: qsTr("Screen lock")
        subtitle: qsTr("Choose when the current desktop session locks")
        MeoBanner {
            width: parent.width; visible: Platform.lastError !== ""
            title: qsTr("Screen lock needs attention"); text: Platform.lastError; tone: "error"
        }
        MeoBanner {
            width: parent.width; visible: !root.policy.passwordRequired || !root.policy.lockAfterGrace
            title: qsTr("The current configuration allows unlocking without authentication")
            text: qsTr("Changing idle timing does not change the authentication policy. Review the advanced screen lock settings to require authentication.")
        }
        MeoEmptyState {
            width: parent.width; visible: !root.policy.available
            icon: "lock"; title: qsTr("Screen locker unavailable")
            description: qsTr("The desktop must provide its screen-lock service before these preferences can be applied.")
        }
        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Automatic protection")
            model: [
                {"id": "automatic", "title": qsTr("Lock after inactivity"), "checked": root.automatic, "trailingKind": "toggle", "enabled": root.policy.available && root.policy.writable && !Platform.screenLockPolicyBusy},
                {"id": "onResume", "title": qsTr("Lock when resuming from sleep"), "checked": root.onResume, "trailingKind": "toggle", "enabled": root.policy.available && root.policy.writable && !Platform.screenLockPolicyBusy},
                {"id": "onStart", "title": qsTr("Lock when the desktop session starts"), "checked": root.onStart, "trailingKind": "toggle", "enabled": root.policy.available && root.policy.writable && !Platform.screenLockPolicyBusy}
            ]
            onRowToggled: (index, checked, row) => {
                if (row.id === "automatic") root.automatic = checked
                else if (row.id === "onResume") root.onResume = checked
                else root.onStart = checked
            }
        }
        MeoTextField {
            id: timeout; width: parent.width; label: qsTr("Idle time before locking (minutes)")
            text: String(root.policy.minutes)
            enabled: root.automatic && root.policy.writable && !Platform.screenLockPolicyBusy
            validator: DoubleValidator { bottom: 0.1; top: 240; decimals: 1; locale: "C" }
        }
        MeoTextField {
            id: grace; width: parent.width; label: qsTr("Authentication grace time (seconds)")
            text: String(root.policy.graceSeconds)
            enabled: root.policy.writable && !Platform.screenLockPolicyBusy
            validator: IntValidator { bottom: 0; top: 300 }
        }
        MeoText {
            width: parent.width; text: qsTr("Zero grace time requests authentication immediately after the automatic lock. Explicit lock and suspend behavior remain with the desktop screen locker.")
            wrapMode: Text.WordWrap
        }
        MeoButton {
            text: Platform.screenLockPolicyBusy ? qsTr("Applying…") : qsTr("Apply screen lock preferences")
            enabled: root.policy.available && root.policy.writable && !Platform.screenLockPolicyBusy && timeout.acceptableInput && grace.acceptableInput
            onClicked: {
                if ((!root.automatic && root.policy.automatic) || (!root.onResume && root.policy.onResume) || Number(timeout.text) > root.policy.minutes || Number(grace.text) > root.policy.graceSeconds)
                    reducedProtection.open()
                else root.applyPolicy()
            }
        }
        MeoButton { text: qsTr("Lock-screen appearance"); type: "tonal"; onClicked: root.navigateTo("session-entry") }
        MeoButton { text: qsTr("Advanced screen lock settings"); type: "text"; enabled: KcmBridge.isAvailable("kcm_screenlocker"); onClicked: root.navigateTo("kcm:kcm_screenlocker") }
    }
}
