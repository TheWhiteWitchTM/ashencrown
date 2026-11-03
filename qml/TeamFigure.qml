import QtQuick
import QtQuick.Effects

Item {
    id: root
    property string source: ""
    property int side: 0
    property bool mirror: false
    property bool outline: false
    // 1 is a standing token. Callers raise it while the figure is up in the air.
    property real depth: 1

    readonly property color ink: side === 0 ? "#3c97ff" : "#9a1219"
    readonly property color deep: side === 0 ? "#0c4ec4" : "#c01822"
    readonly property color mid: side === 0 ? "#1a6adf" : "#8f1218"
    readonly property color lit: side === 0 ? "#e7f2ff" : "#ffd5d0"
    // The optional white halo grows about 7% from the center. Push the slab
    // past that so the colored edge still shows.
    readonly property real reach: outline ? 1.7 : 1
    readonly property real slab: Math.max(0, depth) * reach

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

    // Contact shade, then three bevel steps. Light comes from the upper left,
    // so the thickness steps down toward the board.
    MultiEffect {
        anchors.fill: parent
        visible: root.source !== "" && root.slab > 0.02
        source: pic
        colorization: 1
        colorizationColor: "#14080a"
        brightness: -0.35
        saturation: 0
        opacity: 0.55
        transform: Translate {
            x: root.width * 0.125 * root.slab
            y: root.height * 0.155 * root.slab
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.source !== "" && root.slab > 0.02
        source: pic
        colorization: 1
        colorizationColor: root.deep
        brightness: -0.08
        saturation: 0.4
        contrast: 0.08
        transform: Translate {
            x: root.width * 0.092 * root.slab
            y: root.height * 0.112 * root.slab
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.source !== "" && root.slab > 0.02
        source: pic
        colorization: 1
        colorizationColor: root.mid
        brightness: -0.02
        saturation: 0.2
        transform: Translate {
            x: root.width * 0.055 * root.slab
            y: root.height * 0.068 * root.slab
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.source !== "" && root.slab > 0.02
        source: pic
        colorization: 1
        colorizationColor: root.ink
        brightness: 0.12
        saturation: 0.15
        opacity: 0.92
        transform: Translate {
            x: root.width * 0.024 * root.slab
            y: root.height * 0.03 * root.slab
        }
    }

    // Lit edge. The body covers the middle, so only the rim shows.
    MultiEffect {
        anchors.fill: parent
        visible: root.source !== "" && !root.outline
        source: pic
        colorization: 0.95
        colorizationColor: root.lit
        brightness: 0.55
        saturation: 0.12
        opacity: 0.95
        transform: Translate {
            x: -root.width * 0.014 * Math.max(0.45, root.depth)
            y: -root.height * 0.018 * Math.max(0.45, root.depth)
        }
    }

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
        colorization: root.side === 0 ? 0.38 : 0.55
        colorizationColor: root.ink
        brightness: root.side === 0 ? 0.16 : 0.07
        saturation: 0.28
        contrast: 0.24
    }
}
