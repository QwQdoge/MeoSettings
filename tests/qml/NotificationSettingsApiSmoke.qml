import QtQuick
import org.kde.notificationmanager as NotificationManager

Item {
    id: root

    // CTest supplies an isolated XDG_CONFIG_HOME, so this verifies the actual
    // Plasma API's load/dirty/save path without changing the live session.
    NotificationManager.Settings {
        id: notificationSettings
        live: false
    }

    Component.onCompleted: {
        const initialTimeout = notificationSettings.popupTimeout
        if (initialTimeout < 0)
            throw new Error("NotificationManager.Settings returned an invalid popup timeout")

        const requestedTimeout = initialTimeout === 7000 ? 8000 : 7000
        notificationSettings.popupTimeout = requestedTimeout
        if (!notificationSettings.dirty)
            throw new Error("NotificationManager.Settings did not report a dirty change")

        notificationSettings.save()
        if (notificationSettings.dirty)
            throw new Error("NotificationManager.Settings did not clear dirty state after save")

        notificationSettings.load()
        if (notificationSettings.popupTimeout !== requestedTimeout)
            throw new Error("NotificationManager.Settings did not round-trip the popup timeout")

        Qt.quit()
    }
}
