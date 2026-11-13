import QtQuick
import MeoUI

// Pixel-style animated page illustration drawn in QML (no image assets): a
// Material shape that morphs between two silhouettes behind the page icon,
// with a smaller companion shape orbiting it. Purely decorative.
//
// Motion is static with "reduce motion" and on software rendering, where a
// continuously repainted shape would only cost CPU in VMs and the fallback
// session. `tone` follows the registry tone names.
Item {
    id: hero

    property string icon: "settings"
    property string tone: "primary"
    property string shapeA: "SoftBurst"
    property string shapeB: "Cookie9Sided"
    property real artSize: 148 * MeoTheme.globalScale

    readonly property bool animated: !MeoTheme.reduceMotion
                                     && GraphicsInfo.api !== GraphicsInfo.Software
                                     && visible && enabled
    readonly property color toneContainer: tone === "secondary" ? MeoTheme.secondaryContainer
                                         : tone === "tertiary" ? MeoTheme.tertiaryContainer
                                         : tone === "neutral" ? MeoTheme.surfaceContainerHighest
                                         : MeoTheme.primaryContainer
    readonly property color toneOn: tone === "secondary" ? MeoTheme.contentOnSecondaryContainer
                                  : tone === "tertiary" ? MeoTheme.contentOnTertiaryContainer
                                  : tone === "neutral" ? MeoTheme.contentOnSurface
                                  : MeoTheme.contentOnPrimaryContainer

    implicitWidth: artSize * 1.5
    implicitHeight: artSize * 1.15
    Accessible.ignored: true

    // 0..1..0 loop drives the morph and the companion orbit.
    property real cycle: 0
    SequentialAnimation on cycle {
        running: hero.animated
        loops: Animation.Infinite
        NumberAnimation { to: 1; duration: 3600; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: 3600; easing.type: Easing.InOutSine }
    }
    property real spin: 0
    NumberAnimation on spin {
        running: hero.animated
        loops: Animation.Infinite
        from: 0; to: 360; duration: 42000
    }

    MeoShapeMorph {
        id: companion
        width: hero.artSize * 0.3
        height: width
        fromShape: "Circle"
        toShape: hero.shapeA
        morphProgress: hero.cycle
        color: hero.toneContainer
        opacity: 0.7
        // Orbit around the main shape centre.
        x: hero.width / 2 + Math.cos((hero.spin * 2 - 40) * Math.PI / 180) * hero.artSize * 0.62 - width / 2
        y: hero.height / 2 + Math.sin((hero.spin * 2 - 40) * Math.PI / 180) * hero.artSize * 0.5 - height / 2
    }

    MeoShapeMorph {
        id: main
        anchors.centerIn: parent
        width: hero.artSize
        height: width
        fromShape: hero.shapeA
        toShape: hero.shapeB
        morphProgress: hero.cycle
        rotationAngle: hero.spin
        color: hero.toneContainer
    }

    // Icon sits above the rotating shape and bobs gently.
    MeoIcon {
        id: glyph
        anchors.centerIn: parent
        anchors.verticalCenterOffset: hero.animated ? Math.sin(hero.cycle * Math.PI) * -4 * MeoTheme.globalScale : 0
        icon: hero.icon
        size: hero.artSize * 0.36
        color: hero.toneOn
    }
}
