import QtQuick
import QtQuick.Controls
import MeoUI

ApplicationWindow {
    id: window
    width: 480
    height: 900
    visible: true
    title: "Meo Settings task sheet compact smoke"

    Timer {
        id: startupTimer
        interval: 120
        onTriggered: taskSheet.open()
    }

    Timer {
        id: verificationTimer
        interval: 220
        onTriggered: {
            if (!taskSheet.isOpen || !taskSheet.useBottomSheet) {
                console.error("Compact settings task sheet did not open as a bottom sheet")
                Qt.exit(1)
                return
            }
            if (taskSheet.dismissible) {
                console.error("Security-sensitive Settings task sheet remained dismissible")
                Qt.exit(1)
                return
            }
            taskSheet.close()
        }
    }

    Timer {
        id: timeoutTimer
        interval: 3000
        onTriggered: {
            console.error("Timed out waiting for compact settings task sheet")
            Qt.exit(1)
        }
    }

    MeoSettingsTaskSheet {
        id: taskSheet
        popupParent: Overlay.overlay
        title: "Volume details"
        subtitle: "A compact Settings window must use the bottom-sheet presentation."
        rejectText: "Close"
        dismissible: false

        content: Component {
            Item {
                implicitHeight: 180

                MeoSettingsGroup {
                    anchors.fill: parent
                    title: "Read-only facts"
                    model: [{
                        "title": "Mount point",
                        "subtitle": "/",
                        "icon": "folder",
                        "tone": "neutral",
                        "trailingKind": "none",
                        "interactive": false
                    }]
                }
            }
        }

        onOpened: verificationTimer.start()
        onClosed: {
            timeoutTimer.stop()
            Qt.quit()
        }
    }

    Component.onCompleted: {
        timeoutTimer.start()
        startupTimer.start()
    }
}
