import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    property url chosenImage
    property int chosenScreen: -1
    property string fillMode: "preserveAspectCrop"
    FileDialog { id: picker; title: qsTr("Choose a wallpaper image"); fileMode: FileDialog.OpenFile; nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg)")]; onAccepted: root.chosenImage = selectedFile }
    MeoSettingsTaskSheet {
        id: removeImage; popupParent: Overlay.overlay
        title: qsTr("Remove imported wallpaper?"); subtitle: qsTr("Remove this copy from the wallpaper library. The original file is preserved.")
        acceptText: qsTr("Remove"); rejectText: qsTr("Cancel")
        onAccepted: { WallpaperBackend.removeImported(root.chosenImage); if (WallpaperBackend.error === "") root.chosenImage = "" }
    }
    MeoPageLayout {
        anchors.fill: parent; metricsOverride: root.rootMetrics
        title: qsTr("Wallpaper")
        subtitle: qsTr("Choose an image for all screens or one desktop screen")
        MeoBanner { width: parent.width; visible: WallpaperBackend.error !== ""; title: qsTr("Wallpaper needs attention"); text: WallpaperBackend.error; tone: "error" }
        MeoSettingsGroup {
            width: parent.width; title: qsTr("Current desktops")
            model: WallpaperBackend.wallpapers.map(wallpaper => ({"title": qsTr("Screen %1").arg(wallpaper.screen + 1), "subtitle": wallpaper.image || wallpaper.plugin, "interactive": false}))
        }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Desktop screen")
            readonly property var screens: [...new Set(WallpaperBackend.wallpapers.map(wallpaper => wallpaper.screen).filter(screen => screen >= 0))]
            model: [qsTr("All screens")].concat(screens.map(screen => qsTr("Screen %1").arg(screen + 1)))
            text: root.chosenScreen < 0 ? model[0] : qsTr("Screen %1").arg(root.chosenScreen + 1)
            onSelected: (index, value) => root.chosenScreen = index === 0 ? -1 : screens[index - 1]
        }
        Flow {
            width: parent.width; spacing: MeoTheme.space8
            Repeater {
                model: WallpaperBackend.images
                delegate: Column {
                    required property var modelData
                    width: Math.max(120, (parent.width - MeoTheme.space16) / 3)
                    Image { width: parent.width; height: width * 0.65; source: parent.modelData.image; sourceSize.width: 320; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                    MeoButton { width: parent.width; text: parent.modelData.imported ? qsTr("Imported image") : parent.modelData.name; type: "text"; onClicked: root.chosenImage = parent.modelData.image }
                }
            }
        }
        Image { width: parent.width; height: source.toString() !== "" ? 220 : 0; source: root.chosenImage; fillMode: Image.PreserveAspectFit; asynchronous: true }
        MeoButton { text: qsTr("Choose image"); type: "tonal"; enabled: !WallpaperBackend.busy; onClicked: picker.open() }
        MeoButton {
            text: qsTr("Import selected image into library"); type: "text"
            enabled: !WallpaperBackend.busy && root.chosenImage.toString() !== ""
            onClicked: { const imported = WallpaperBackend.importImage(root.chosenImage); if (imported.toString() !== "") root.chosenImage = imported }
        }
        MeoButton {
            text: qsTr("Remove imported image"); type: "text"
            visible: WallpaperBackend.images.some(image => image.imported && image.image.toString() === root.chosenImage.toString())
            enabled: !WallpaperBackend.busy
            onClicked: removeImage.open()
        }
        MeoExposedDropdown {
            width: parent.width; label: qsTr("Image layout")
            model: [qsTr("Fill screen"), qsTr("Fit image"), qsTr("Stretch"), qsTr("Center")]
            text: model[["preserveAspectCrop", "preserveAspectFit", "stretch", "pad"].indexOf(root.fillMode)]
            onSelected: (index, value) => root.fillMode = ["preserveAspectCrop", "preserveAspectFit", "stretch", "pad"][index]
        }
        MeoButton { text: qsTr("Apply wallpaper"); enabled: WallpaperBackend.available && !WallpaperBackend.busy && root.chosenImage.toString() !== ""; onClicked: WallpaperBackend.applyImage(root.chosenImage, root.fillMode, root.chosenScreen) }
        MeoButton { text: qsTr("Advanced wallpaper plugins and slideshows"); type: "text"; onClicked: root.navigateTo("kcm:kcm_wallpaper") }
    }
}
