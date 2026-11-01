import QtQuick
import QtQuick.Effects

Item {
    id: root
    property string source: ""
    property int side: 0
    property bool mirror: false
    property bool outline: false

    Image {
        id: pic
        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        mirror: root.mirror
        visible: false
    }

    // Flat white copy, scaled past the figure so the rim stays visible.
    MultiEffect {
        id: rim
        anchors.fill: parent
        visible: root.outline && root.source !== ""
        source: pic
        colorization: 1
        colorizationColor: "#ffffff"
        brightness: 1
        saturation: 0
        contrast: -1
        transform: Scale {
            origin.x: rim.width / 2
            origin.y: rim.height / 2
            xScale: 1.14
            yScale: 1.1
        }
    }

    MultiEffect {
        anchors.fill: parent
        z: 1
        source: pic
        colorization: root.side === 0 ? 0.46 : 0.7
        colorizationColor: root.side === 0 ? "#3c97ff" : "#8a1016"
        brightness: root.side === 0 ? 0.2 : 0.02
        saturation: 0.45
        contrast: 0.08
    }
}
