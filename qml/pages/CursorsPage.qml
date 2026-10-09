import QtQuick
import MeoUI
import Meo.System 1.0

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property string selectedTheme: InputDevices.cursorSettings.theme
    property int selectedSize: InputDevices.cursorSettings.size
    readonly property var selected: InputDevices.cursorThemes.find(theme => theme.id === selectedTheme) || ({"sizes": [], "preview": ""})
    Component.onCompleted: InputDevices.refreshCursorThemes()
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics
        title: qsTr("Cursors")
        subtitle: qsTr("Choose an installed pointer theme and size for this desktop session")
        MeoBanner {
            width: parent.width; visible: InputDevices.error !== ""
            title: qsTr("Cursor preference needs attention"); text: InputDevices.error; tone: "error"
        }
        MeoBanner {
            width: parent.width; visible: !InputDevices.cursorSettings.available
            title: qsTr("Cursor controls unavailable")
            text: qsTr("These controls require the KDE Wayland input service. Installed themes can still be reviewed.")
        }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Pointer theme")
            model: InputDevices.cursorThemes.map(theme => theme.label)
            text: (InputDevices.cursorThemes.find(theme => theme.id === root.selectedTheme) || {}).label || root.selectedTheme || qsTr("System default")
            enabled: InputDevices.cursorThemes.length > 0
            onSelected: (index, value) => {
                root.selectedTheme = InputDevices.cursorThemes[index].id
                const sizes = root.selected.sizes
                if (sizes.indexOf(root.selectedSize) < 0) root.selectedSize = sizes[0] || -1
            }
        }
        Image {
            width: 96; height: 96; fillMode: Image.PreserveAspectFit
            source: root.selected.preview; visible: source.toString() !== ""
            Accessible.name: qsTr("Selected cursor theme preview")
        }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Pointer size")
            model: root.selected.sizes.map(size => qsTr("%1 pixels").arg(size))
            text: root.selectedSize > 0 ? qsTr("%1 pixels").arg(root.selectedSize) : qsTr("System default")
            enabled: root.selected.sizes.length > 0
            onSelected: (index, value) => root.selectedSize = root.selected.sizes[index]
        }
        MeoButton {
            text: qsTr("Apply pointer preferences")
            enabled: InputDevices.cursorSettings.available && InputDevices.cursorSettings.writable && root.selected.sizes.indexOf(root.selectedSize) >= 0
            onClicked: InputDevices.configureCursor(root.selectedTheme, root.selectedSize)
        }
        MeoText {
            width: parent.width; wrapMode: Text.WordWrap
            text: qsTr("Some applications may need to be reopened to adopt the new pointer. The preview shows the first frame of the installed theme; animation follows the theme itself.")
        }
        MeoButton { text: qsTr("Refresh installed themes"); type: "tonal"; onClicked: InputDevices.refreshCursorThemes() }
        MeoButton { text: qsTr("Advanced cursor settings"); type: "text"; enabled: KcmBridge.isAvailable("kcm_cursortheme"); onClicked: root.navigateTo("kcm:kcm_cursortheme") }
    }
}
