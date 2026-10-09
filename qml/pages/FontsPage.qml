import QtQuick
import QtQuick.Layouts
import MeoUI

Item {
    id: root
    property var navigateTo: function(route) {}
    property var rootMetrics: null
    MeoPageLayout {
        anchors.fill: parent
        metricsOverride: root.rootMetrics
        title: qsTr("Fonts")
        MeoBanner { width: parent.width; visible: FontSettingsBackend.error !== ""; title: qsTr("Font setting needs attention"); text: FontSettingsBackend.error; tone: "error" }
        Repeater {
            model: FontSettingsBackend.fonts
            delegate: Column {
                id: entry
                required property var modelData
                width: parent.width
                spacing: MeoTheme.space12
                property string chosenFamily: modelData.family
                property string chosenStyle: modelData.style
                property var availableStyles: FontSettingsBackend.styles(chosenFamily)
                MeoExposedDropdown {
                    width: parent.width; label: entry.modelData.label
                    model: FontSettingsBackend.families; text: entry.chosenFamily
                    onSelected: (index, value) => { entry.chosenFamily = value; entry.chosenStyle = entry.availableStyles[0] || "" }
                }
                MeoExposedDropdown {
                    width: parent.width; label: qsTr("Style")
                    model: entry.availableStyles; text: entry.chosenStyle
                    onSelected: (index, value) => entry.chosenStyle = value
                }
                MeoTextField {
                    id: sizeField; width: parent.width; label: qsTr("Size in points")
                    text: String(entry.modelData.points)
                    validator: DoubleValidator { bottom: 6; top: 72; decimals: 1; locale: "C" }
                }
                MeoButton {
                    text: qsTr("Apply %1 font").arg(entry.modelData.label)
                    type: "tonal"; enabled: sizeField.acceptableInput && entry.chosenStyle !== ""
                    onClicked: FontSettingsBackend.setFont(entry.modelData.id, entry.chosenFamily, entry.chosenStyle, Number(sizeField.text))
                }
            }
        }
        MeoButton { text: qsTr("Advanced font rendering"); type: "text"; onClicked: root.navigateTo("kcm:kcm_fonts") }
    }
}
