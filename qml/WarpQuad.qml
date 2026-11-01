import QtQuick
import "board3d.js" as Board

// One rectangle of the slab, mapped onto a projected quad.
Item {
    id: quad
    property var corners: null
    visible: corners !== null
    transformOrigin: Item.TopLeft
    x: 0
    y: 0
    width: 100
    height: 100

    transform: Matrix4x4 {
        matrix: {
            var c = quad.corners
            if (!c) return Qt.matrix4x4()
            var k = Board.quadMatrix(quad.width, quad.height, c)
            return Qt.matrix4x4(
                k[0], k[1], k[2], k[3],
                k[4], k[5], k[6], k[7],
                k[8], k[9], k[10], k[11],
                k[12], k[13], k[14], k[15])
        }
    }
}
