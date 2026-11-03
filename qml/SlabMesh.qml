import QtQuick
import "board3d.js" as Board

Item {
    id: root
    property int files: 8
    property int ranks: 8
    property real padTop: 0.11
    property real padBottom: 0.035
    property real elev: 48

    property color frontColor: "#1c4c8c"
    property color frontDeep: "#0c2448"
    property color backColor: "#922c24"
    property color backDeep: "#4c1410"
    property color leftColor: "#4a4034"
    property color leftDeep: "#1c1814"
    property color rightColor: "#4a4034"
    property color rightDeep: "#1c1814"
    property color lowerFront: "#14325e"
    property color lowerBack: "#5c1c16"
    property color lowerSide: "#14110e"
    property color lowerTop: "#241e18"
    property color lipFront: "#d5e6ff"
    property color lipBack: "#ffd0c4"
    property color lipSide: "#e2c27a"

    readonly property var view3d: Board.makeView(width, height, files, ranks, padTop, padBottom, elev)
    readonly property var form: view3d ? Board.slab(view3d) : null
    readonly property var shade: view3d ? Board.shadowBox(view3d) : null

    component Face: WarpQuad {
        id: face
        property color topColor: "#888888"
        property color botColor: "#222222"
        property real stone: 0
        property real wash: 1
        Image {
            anchors.fill: parent
            visible: face.stone > 0.01
            source: "qrc:/assets/ui/marble_grey.png"
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
            opacity: Math.min(1, face.stone)
        }
        Rectangle {
            anchors.fill: parent
            antialiasing: true
            opacity: face.wash
            gradient: Gradient {
                GradientStop { position: 0.0; color: face.topColor }
                GradientStop { position: 0.22; color: face.topColor }
                GradientStop { position: 1.0; color: face.botColor }
            }
        }
    }

    Rectangle {
        visible: root.shade !== null
        x: root.shade ? root.shade.x - root.shade.w * 0.04 : 0
        y: root.shade ? root.shade.y + root.shade.h * 0.42 : 0
        width: root.shade ? root.shade.w * 1.16 : 0
        height: root.shade ? root.shade.h * 1.2 : 0
        radius: height / 2
        color: "#000000"
        opacity: 0.28
        z: 0
    }
    Rectangle {
        visible: root.shade !== null
        x: root.shade ? root.shade.x : 0
        y: root.shade ? root.shade.y : 0
        width: root.shade ? root.shade.w : 0
        height: root.shade ? root.shade.h : 0
        radius: height / 2
        color: "#000000"
        opacity: 0.42
        z: 0
    }

    Face { z: 1; corners: root.form ? root.form.lowerBack : null; topColor: root.lowerBack; botColor: "#0a0806"; stone: 0.8; wash: 0.8 }
    Face { z: 1; corners: root.form ? root.form.lowerLeft : null; topColor: root.lowerSide; botColor: "#0a0806"; stone: 0.9; wash: 0.72 }
    Face { z: 1; corners: root.form ? root.form.lowerRight : null; topColor: root.lowerSide; botColor: "#0a0806"; stone: 0.9; wash: 0.78 }
    Face { z: 2; corners: root.form ? root.form.lowerFront : null; topColor: root.lowerFront; botColor: "#0a0806"; stone: 0.85; wash: 0.74 }

    WarpQuad {
        z: 3
        corners: root.form ? root.form.lowerTop : null
        Image {
            anchors.fill: parent
            source: "qrc:/assets/ui/marble_black.png"
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }
        Rectangle {
            anchors.fill: parent
            color: root.lowerTop
            opacity: 0.72
        }
    }

    Face { z: 4; corners: root.form ? root.form.upperBack : null; topColor: root.backColor; botColor: root.backDeep; stone: 1; wash: 0.66 }
    Face { z: 4; corners: root.form ? root.form.upperLeft : null; topColor: root.leftColor; botColor: root.leftDeep; stone: 1; wash: 0.58 }
    Face { z: 4; corners: root.form ? root.form.upperRight : null; topColor: root.rightColor; botColor: root.rightDeep; stone: 1; wash: 0.64 }
    Face { z: 5; corners: root.form ? root.form.upperFront : null; topColor: root.frontColor; botColor: root.frontDeep; stone: 1; wash: 0.62 }

    WarpQuad {
        z: 6
        corners: root.form ? root.form.backLip : null
        Rectangle { anchors.fill: parent; color: root.lipBack }
    }
    WarpQuad {
        z: 6
        corners: root.form ? root.form.leftLip : null
        Rectangle { anchors.fill: parent; color: root.lipSide }
    }
    WarpQuad {
        z: 6
        corners: root.form ? root.form.rightLip : null
        Rectangle { anchors.fill: parent; color: root.lipSide }
    }
    WarpQuad {
        z: 6
        corners: root.form ? root.form.frontLip : null
        Rectangle { anchors.fill: parent; color: root.lipFront }
    }

    WarpQuad {
        z: 7
        corners: root.form ? root.form.upperTop : null
        Image {
            anchors.fill: parent
            source: "qrc:/assets/ui/marble_black.png"
            fillMode: Image.Stretch
            smooth: true
            mipmap: true
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1affffff" }
                GradientStop { position: 0.32; color: "#00000000" }
                GradientStop { position: 1.0; color: "#2a000000" }
            }
        }
    }
}
