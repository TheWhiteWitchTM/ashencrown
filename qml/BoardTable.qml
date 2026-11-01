import QtQuick
import "board3d.js" as Board

Item {
    id: table
    property string fontFamily: "Liberation Serif"

    readonly property var kind: ["", "pawn", "knight", "bishop", "rook", "queen", "king"]
    readonly property var nearCamp: Board.camp(Board.nearSide(Game.flipped))
    readonly property var farCamp: Board.camp(Board.farSide(Game.flipped))
    readonly property real labelPx: {
        if (!mesh.view3d) return 14
        var a = Board.project(mesh.view3d, 3, 0, 7.6)
        var b = Board.project(mesh.view3d, 4, 0, 7.6)
        return Math.max(12, Math.min(22, Math.hypot(b.x - a.x, b.y - a.y) * 0.2))
    }

    property bool glideOn: false
    property real glideT: 0
    property int glideFrom: -1
    property int glideTo: -1
    property int glideSide: 0
    property int glideType: 0

    property bool rookOn: false
    property real rookT: 0
    property int rookFrom: -1
    property int rookTo: -1
    property int rookSide: 0
    property int rookType: 0

    function pieceSource(side, type) {
        if (type < 1) return ""
        var who = side === 0 ? "alliance" : "horde"
        return "qrc:/assets/pieces/" + who + "_" + kind[type] + ".png"
    }

    function marbleSource(file, rank) {
        var turn = ((file * 3 + rank) % 4) * 90
        var kindName = (file + rank) % 2 === 0 ? "marble_black_" : "marble_grey_"
        return "qrc:/assets/ui/" + kindName + turn + ".png"
    }

    function isTarget(sq) {
        var list = Game.legalTargets
        for (var i = 0; i < list.length; ++i)
            if (list[i] === sq) return true
        return false
    }

    function hiddenSq(sq) {
        return (glideOn && sq === glideTo) || (rookOn && sq === rookTo)
    }

    function campStrip(alliance) {
        if (!mesh.view3d) return null
        var view = mesh.view3d
        var rankIndex = alliance ? 0 : 7
        var row = Game.flipped ? rankIndex : (7 - rankIndex)
        if (row === 7)
            return Board.topQuad(view, 0.12, view.files - 0.12, view.ranks + 0.05, view.ranks + view.margin * 0.62, 0.02)
        return Board.topQuad(view, 0.12, view.files - 0.12, -view.margin * 0.62, -0.05, 0.02)
    }

    Connections {
        target: Game
        function onGlide(from, to, rookFromSq, rookToSq) {
            var dest = Game.squares[to]
            table.glideSide = dest.side
            table.glideType = dest.type
            table.glideFrom = from
            table.glideTo = to
            table.glideOn = true
            table.glideT = 0
            glideAnim.restart()
            if (rookFromSq >= 0) {
                var rook = Game.squares[rookToSq]
                table.rookSide = rook.side
                table.rookType = rook.type
                table.rookFrom = rookFromSq
                table.rookTo = rookToSq
                table.rookOn = true
                table.rookT = 0
                rookAnim.restart()
            }
        }
    }

    NumberAnimation {
        id: glideAnim
        target: table
        property: "glideT"
        from: 0
        to: 1
        duration: 480
        easing.type: Easing.InOutCubic
        onFinished: table.glideOn = false
    }
    NumberAnimation {
        id: rookAnim
        target: table
        property: "rookT"
        from: 0
        to: 1
        duration: 480
        easing.type: Easing.InOutCubic
        onFinished: table.rookOn = false
    }

    Rectangle {
        z: 0
        anchors.centerIn: parent
        width: parent.width * 0.92
        height: parent.height * 0.62
        radius: width / 2
        color: "#c47a32"
        opacity: 0.16
    }

    SlabMesh {
        id: mesh
        anchors.fill: parent
        files: 8
        ranks: 8
        padTop: 0.12
        padBottom: 0.04
        frontColor: table.nearCamp.face
        frontDeep: table.nearCamp.deep
        lipFront: table.nearCamp.lip
        lowerFront: table.nearCamp.base
        backColor: table.farCamp.face
        backDeep: table.farCamp.deep
        lipBack: table.farCamp.lip
        lowerBack: table.farCamp.base
        leftColor: "#5a4c3a"
        leftDeep: "#241c16"
        rightColor: "#4a3e30"
        rightDeep: "#1a1512"
        lowerSide: "#16120e"
        lowerTop: "#2a231c"
        lipSide: "#e2c27a"
    }

    WarpQuad {
        z: 8
        corners: table.campStrip(true)
        Rectangle {
            anchors.fill: parent
            color: Board.camp(0).inlay
        }
    }
    WarpQuad {
        z: 8
        corners: table.campStrip(false)
        Rectangle {
            anchors.fill: parent
            color: Board.camp(1).inlay
        }
    }

    Repeater {
        model: 64
        WarpQuad {
            id: tile
            required property int index
            property int file: index % 8
            property int rank: Math.floor(index / 8)
            property int col: Game.flipped ? (7 - file) : file
            property int row: Game.flipped ? rank : (7 - rank)
            property var piece: Game.squares[index]
            property bool marked: table.isTarget(index)
            property bool showMan: piece && piece.type > 0 && !table.hiddenSq(index)
            z: 10 + row
            corners: mesh.view3d ? Board.tileQuad(mesh.view3d, col, row) : null

            Image {
                anchors.fill: parent
                source: table.marbleSource(tile.file, tile.rank)
                fillMode: Image.Stretch
                smooth: true
                mipmap: true
            }
            Rectangle {
                anchors.fill: parent
                color: "#000000"
                opacity: 0.04
            }
            Rectangle {
                anchors.fill: parent
                visible: tile.rank === 0 || tile.rank === 7
                color: tile.rank === 0 ? Board.camp(0).wash : Board.camp(1).wash
            }
            Rectangle {
                width: parent.width
                height: parent.height * 0.09
                color: "#66000000"
            }
            Rectangle {
                y: parent.height * 0.9
                width: parent.width
                height: parent.height * 0.1
                color: "#55ffffff"
            }
            Rectangle {
                width: parent.width * 0.07
                height: parent.height
                color: "#33ffffff"
            }
            Rectangle {
                x: parent.width * 0.93
                width: parent.width * 0.07
                height: parent.height
                color: "#55000000"
            }
            Rectangle {
                anchors.fill: parent
                visible: index === Game.lastFrom || index === Game.lastTo
                color: "#66e2b657"
            }
            Rectangle {
                anchors.fill: parent
                visible: index === Game.checkSquare
                color: "#88c44a3a"
            }
            Rectangle {
                anchors.fill: parent
                visible: index === Game.selected
                color: "transparent"
                border.color: "#f7f1e2"
                border.width: parent.width * 0.045
            }
            Rectangle {
                visible: tile.marked && (!tile.piece || tile.piece.type === 0)
                width: parent.width * 0.24
                height: parent.height * 0.24
                radius: width / 2
                anchors.centerIn: parent
                color: Game.turn === 0 ? "#f3f7ff" : "#fff3f1"
                border.color: Game.turn === 0 ? Board.camp(0).face : Board.camp(1).face
                border.width: 2
            }
            Rectangle {
                visible: tile.marked && tile.piece && tile.piece.type > 0
                anchors.fill: parent
                anchors.margins: parent.width * 0.08
                radius: width / 2
                color: "transparent"
                border.color: Game.turn === 0 ? "#d6e6ff" : "#ffb0a6"
                border.width: parent.width * 0.065
            }
        }
    }

    Repeater {
        id: men
        model: 64
        Item {
            id: man
            required property int index
            property var pc: Game.squares[index]
            property bool live: pc && pc.type > 0 && !table.hiddenSq(index)
            property var pose: (live && mesh.view3d)
                ? Board.piecePose(mesh.view3d, index, index, 0, Game.flipped, pc.side, pc.type, index === Game.selected)
                : null
            visible: pose !== null
            x: pose ? pose.x : 0
            y: pose ? pose.y : 0
            width: pose ? pose.w : 0
            height: pose ? pose.h : 0
            z: pose ? pose.z : 0

            Rectangle {
                width: man.width * 0.7
                height: Math.max(7, width * 0.22)
                x: (man.width - width) / 2
                y: man.height - height * 0.42
                radius: width / 2
                color: "#000000"
                opacity: 0.5
            }
            Rectangle {
                width: man.width * 0.78
                height: Math.max(8, width * 0.34)
                x: (man.width - width) / 2
                y: man.height - height * 0.62
                radius: width / 2
                color: man.pc.side === 0 ? Board.camp(0).glow : Board.camp(1).glow
                opacity: 0.95
            }
            TeamFigure {
                anchors.fill: parent
                source: man.live ? table.pieceSource(man.pc.side, man.pc.type) : ""
                side: man.pc ? man.pc.side : 0
                outline: Game.figureOutline
            }
        }
    }

    TeamFigure {
        id: glideMan
        property var pose: (table.glideOn && mesh.view3d)
            ? Board.piecePose(mesh.view3d, table.glideFrom, table.glideTo, table.glideT, Game.flipped, table.glideSide, table.glideType, false)
            : null
        visible: pose !== null
        x: pose ? pose.x : 0
        y: pose ? pose.y : 0
        width: pose ? pose.w : 0
        height: pose ? pose.h : 0
        z: pose ? pose.z : 0
        source: table.pieceSource(table.glideSide, table.glideType)
        side: table.glideSide
        outline: Game.figureOutline
    }
    Rectangle {
        visible: glideMan.pose !== null
        x: glideMan.pose ? glideMan.pose.footX - width / 2 : 0
        y: glideMan.pose ? glideMan.pose.footY - height * 0.45 : 0
        width: glideMan.pose ? glideMan.pose.w * 0.62 : 0
        height: width * 0.3
        radius: width / 2
        color: table.glideSide === 0 ? Board.camp(0).glow : Board.camp(1).glow
        opacity: 0.8
        z: glideMan.z - 1
    }
    TeamFigure {
        id: rookMan
        property var pose: (table.rookOn && mesh.view3d)
            ? Board.piecePose(mesh.view3d, table.rookFrom, table.rookTo, table.rookT, Game.flipped, table.rookSide, table.rookType, false)
            : null
        visible: pose !== null
        x: pose ? pose.x : 0
        y: pose ? pose.y : 0
        width: pose ? pose.w : 0
        height: pose ? pose.h : 0
        z: pose ? pose.z : 0
        source: table.pieceSource(table.rookSide, table.rookType)
        side: table.rookSide
        outline: Game.figureOutline
    }
    Rectangle {
        visible: rookMan.pose !== null
        x: rookMan.pose ? rookMan.pose.footX - width / 2 : 0
        y: rookMan.pose ? rookMan.pose.footY - height * 0.45 : 0
        width: rookMan.pose ? rookMan.pose.w * 0.62 : 0
        height: width * 0.3
        radius: width / 2
        color: table.rookSide === 0 ? Board.camp(0).glow : Board.camp(1).glow
        opacity: 0.8
        z: rookMan.z - 1
    }

    Repeater {
        model: 8
        Text {
            required property int index
            property var spot: mesh.view3d
                ? Board.project(mesh.view3d, index + 0.5, -mesh.view3d.thick * 0.55, mesh.view3d.ranks + mesh.view3d.margin * 0.35)
                : null
            visible: spot !== null
            x: spot ? spot.x - width / 2 : 0
            y: spot ? spot.y - height / 2 : 0
            z: 40
            text: String.fromCharCode(97 + (Game.flipped ? (7 - index) : index))
            color: "#f6efe2"
            font.pixelSize: table.labelPx
            font.family: "DejaVu Sans Mono"
            style: Text.Outline
            styleColor: "#140e0c"
        }
    }
    MouseArea {
        anchors.fill: parent
        z: 900
        enabled: Game.inputEnabled
        onClicked: function (mouse) {
            var boardSq = Board.hit(mesh.view3d, mouse.x, mouse.y, Game.flipped)
            var bodySq = -1
            var best = -1
            for (var i = 0; i < men.count; ++i) {
                var it = men.itemAt(i)
                if (!it || !it.visible) continue
                var top = it.y + it.height * 0.16
                if (mouse.x < it.x + it.width * 0.14 || mouse.x > it.x + it.width * 0.86)
                    continue
                if (mouse.y < top || mouse.y > it.y + it.height)
                    continue
                if (it.z >= best) {
                    best = it.z
                    bodySq = it.index
                }
            }
            var sq = boardSq
            if (Game.selected < 0) {
                if (bodySq >= 0) sq = bodySq
            } else if (!(boardSq >= 0 && table.isTarget(boardSq)) && bodySq >= 0) {
                sq = bodySq
            }
            if (sq >= 0) Game.clickSquare(sq)
        }
    }
}
