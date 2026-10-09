import QtQuick
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Date & time")
        subtitle: qsTr("Clock changes are authorized by the system time service")
        MeoBanner {
            width: parent.width
            visible: RegionalBackend.error !== ""
            title: qsTr("Time setting needs attention")
            text: RegionalBackend.error
            tone: "error"
        }
        MeoEmptyState {
            width: parent.width
            visible: !RegionalBackend.available
            icon: "schedule"
            title: qsTr("System time service unavailable")
            description: qsTr("Refresh after the system time service becomes available.")
        }
        MeoSettingsGroup {
            width: parent.width
            title: qsTr("Synchronization")
            model: [{"id": "ntp", "title": qsTr("Set time automatically"),
                "subtitle": RegionalBackend.clock.NTPSynchronized ? qsTr("Synchronized") : qsTr("Not synchronized"),
                "trailingKind": "toggle", "checked": Boolean(RegionalBackend.clock.NTP),
                "enabled": RegionalBackend.available && Boolean(RegionalBackend.clock.CanNTP) && !RegionalBackend.busy}]
            onRowToggled: (index, checked, row) => RegionalBackend.setNetworkTime(checked)
        }
        MeoTextField { id: timezoneSearch; width: parent.width; label: qsTr("Find a time zone"); placeholder: qsTr("Search city or region") }
        MeoExposedDropdown {
            width: parent.width
            label: qsTr("Time zone")
            model: RegionalBackend.timezones.filter(zone => zone.toLowerCase().includes(timezoneSearch.text.toLowerCase()))
            text: RegionalBackend.clock.Timezone || ""
            enabled: RegionalBackend.available && !RegionalBackend.busy
            onSelected: (index, value) => RegionalBackend.setTimezone(value)
        }
        MeoTextField {
            id: manualTime
            width: parent.width
            label: qsTr("Date and time")
            placeholder: "2026-10-06T14:30:00"
            helperText: qsTr("Enter an ISO date and time; include an offset for a specific time zone.")
            enabled: RegionalBackend.available && !RegionalBackend.busy && !RegionalBackend.clock.NTP
        }
        MeoButton {
            text: qsTr("Set date and time")
            type: "tonal"
            enabled: manualTime.enabled && manualTime.text.trim() !== ""
            onClicked: RegionalBackend.setTime(manualTime.text.trim())
        }
        MeoButton { text: qsTr("Refresh"); type: "text"; enabled: !RegionalBackend.busy; onClicked: RegionalBackend.refresh() }
    }
}
