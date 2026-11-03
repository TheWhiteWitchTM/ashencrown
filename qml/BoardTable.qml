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
    property real sway: 0
    property int hoverSq: -1
    property int aimSq: -1
    property int gazeSq: -1
    property int gazeSide: -1
    property var gazeMarks: []
    property var coach: ({})
    property string coachLine: ""
    property int coachSev: 0
    property int coachSq: -1

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

    function isGaze(sq) {
        var list = gazeMarks
        if (!list) return false
        for (var i = 0; i < list.length; ++i)
            if (list[i] === sq) return true
        return false
    }

    function pieceAt(mx, my) {
        var bodySq = -1
        var best = -1
        for (var i = 0; i < men.count; ++i) {
            var it = men.itemAt(i)
            if (!it || !it.visible) continue
            var top = it.y + it.height * 0.1
            if (mx < it.x + it.width * 0.1 || mx > it.x + it.width * 0.9) continue
            if (my < top || my > it.y + it.height) continue
            if (it.z >= best) {
                best = it.z
                bodySq = it.index
            }
        }
        return bodySq
    }

    function clearGaze() {
        hoverSq = -1
        aimSq = -1
        gazeSq = -1
        gazeSide = -1
        gazeMarks = []
        coach = ({})
        coachLine = ""
        coachSev = 0
        coachSq = -1
    }

    function coachMark(sq) {
        var list = coach.marks
        if (!list) return false
        for (var i = 0; i < list.length; ++i)
            if (list[i] === sq) return true
        return false
    }

    function refreshCoach() {
        var sq = -1
        if (Game.selected >= 0 && aimSq >= 0 && isTarget(aimSq)) sq = aimSq
        else if (hoverSq >= 0) sq = hoverSq
        var next = sq >= 0 ? Game.coachAt(sq) : null
        if (!next || !next.show) {
            coach = ({})
            coachLine = ""
            coachSev = 0
            coachSq = -1
            return
        }
        coach = next
        coachLine = next.line || ""
        coachSev = next.severity || 0
        coachSq = next.sq === undefined ? sq : next.sq
    }

    function applyGaze(sq) {
        var pc = sq >= 0 ? Game.squares[sq] : null
        if (!pc || pc.type === 0 || Number(pc.side) === Number(Game.turn)) {
            if (gazeSq !== -1) {
                gazeSq = -1
                gazeSide = -1
                gazeMarks = []
            }
            return
        }
        if (sq === gazeSq) return
        gazeSq = sq
        gazeSide = pc.side
        gazeMarks = Game.gazeTargets(sq)
    }

    function trackGaze(mx, my) {
        var sq = pieceAt(mx, my)
        hoverSq = sq
        aimSq = Board.hit(mesh.view3d, mx, my, Game.flipped)
        applyGaze(sq)
        refreshCoach()
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
        function onTick() {
            if (table.gazeSq >= 0) {
                var sq = table.gazeSq
                table.gazeSq = -1
                table.applyGaze(sq)
            }
            if (table.hoverSq >= 0 || table.aimSq >= 0) table.refreshCoach()
        }
        function onGlide(from, to, rookFromSq, rookToSq) {
            table.clearGaze()
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
        duration: 840
        easing.type: Easing.Linear
        onFinished: table.glideOn = false
    }
    NumberAnimation {
        id: rookAnim
        target: table
        property: "rookT"
        from: 0
        to: 1
        duration: 840
        easing.type: Easing.Linear
        onFinished: table.rookOn = false
    }

    SequentialAnimation {
        running: Game.selected >= 0 && !table.glideOn
        loops: Animation.Infinite
        onRunningChanged: if (!running) table.sway = 0
        NumberAnimation {
            target: table
            property: "sway"
            to: 1
            duration: 780
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: table
            property: "sway"
            to: 0
            duration: 780
            easing.type: Easing.InOutSine
        }
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
            Item {
                id: gazeMark
                anchors.fill: parent
                visible: table.isGaze(tile.index) || tile.index === table.gazeSq
                property color ink: table.gazeSide === 0 ? "#e7f2ff" : "#ffe4e0"
                property color wash: table.gazeSide === 0 ? "#3c97ff" : "#e0454c"
                Rectangle {
                    anchors.fill: parent
                    color: gazeMark.wash
                    opacity: tile.index === table.gazeSq ? 0.22 : 0.34
                }
                Repeater {
                    model: table.isGaze(tile.index) ? 4 : 0
                    Item {
                        required property int index
                        x: (index % 2) * (gazeMark.width - width)
                        y: (index < 2 ? 0 : 1) * (gazeMark.height - height)
                        width: gazeMark.width * 0.28
                        height: gazeMark.height * 0.28
                        Rectangle {
                            width: parent.width
                            height: Math.max(2, parent.height * 0.22)
                            y: index < 2 ? 0 : parent.height - height
                            color: gazeMark.ink
                        }
                        Rectangle {
                            height: parent.height
                            width: Math.max(2, parent.width * 0.22)
                            x: (index % 2) ? parent.width - width : 0
                            color: gazeMark.ink
                        }
                    }
                }
                Rectangle {
                    visible: table.isGaze(tile.index) && (!tile.piece || tile.piece.type === 0)
                    width: parent.width * 0.18
                    height: parent.height * 0.18
                    anchors.centerIn: parent
                    rotation: 45
                    antialiasing: true
                    color: gazeMark.ink
                    border.color: table.gazeSide === 0 ? "#072a5c" : "#4a090c"
                    border.width: 1
                }
                Rectangle {
                    visible: table.isGaze(tile.index) && tile.piece && tile.piece.type > 0
                    anchors.fill: parent
                    anchors.margins: parent.width * 0.12
                    radius: width / 2
                    color: "transparent"
                    border.color: gazeMark.ink
                    border.width: Math.max(2, parent.width * 0.05)
                    antialiasing: true
                }
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
            Rectangle {
                visible: Game.looseMarks.length === 64 && Game.looseMarks[index] > 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.max(3, parent.height * 0.08)
                color: Game.looseMarks[index] >= 2 ? "#ff5a62" : "#e2b657"
            }
            Rectangle {
                anchors.fill: parent
                visible: table.coachSq === index && table.coachLine !== ""
                color: table.coachSev >= 2 ? "#55c44a3a" : "#44e2b657"
            }
            Item {
                id: coachCorners
                anchors.fill: parent
                visible: table.coachMark(tile.index)
                property color ink: table.coachSev >= 2 ? "#ffd2cc" : "#ffe3ad"
                Repeater {
                    model: coachCorners.visible ? 4 : 0
                    Item {
                        required property int index
                        x: (index % 2) * (coachCorners.width - width)
                        y: (index < 2 ? 0 : 1) * (coachCorners.height - height)
                        width: coachCorners.width * 0.26
                        height: coachCorners.height * 0.26
                        Rectangle {
                            width: parent.width
                            height: Math.max(2, parent.height * 0.24)
                            y: index < 2 ? 0 : parent.height - height
                            color: coachCorners.ink
                        }
                        Rectangle {
                            height: parent.height
                            width: Math.max(2, parent.width * 0.24)
                            x: (index % 2) ? parent.width - width : 0
                            color: coachCorners.ink
                        }
                    }
                }
            }
            Rectangle {
                visible: tile.marked && Game.cautionMarks.length === 64 && Game.cautionMarks[index] > 0
                anchors.fill: parent
                color: "transparent"
                border.color: Game.cautionMarks[index] >= 2 ? "#ff5a62" : "#e2b657"
                border.width: Math.max(2, parent.width * 0.055)
            }
        }
    }

    component Marcher: Item {
        id: walker
        property bool marching: false
        property real travel: 0
        property int fromSq: -1
        property int toSq: -1
        property int side: 0
        property int type: 0

        function poseAt(t) {
            if (!marching || !mesh.view3d || fromSq < 0) return null
            return Board.piecePose(mesh.view3d, fromSq, toSq, t, Game.flipped, side, type, false, 0)
        }

        property var pose: poseAt(travel)
        readonly property var step: pose ? pose : ({
            x: 0, y: 0, w: 1, h: 1, footX: 0, footY: 0,
            hop: 0, lean: 0, shadow: 1, dust: 0, trail: 0, squashX: 1, squashY: 1
        })
        visible: pose !== null
        x: pose ? pose.x : 0
        y: pose ? pose.y : 0
        width: pose ? pose.w : 1
        height: pose ? pose.h : 1
        z: pose ? pose.z : 0

        Rectangle {
            x: walker.step.footX - walker.x - width / 2
            y: walker.step.footY - walker.y - height * 0.35
            width: walker.step.w * 0.92 * walker.step.shadow
            height: Math.max(8, width * 0.3)
            radius: height / 2
            color: "#070605"
            opacity: 0.55 * walker.step.shadow
        }
        Rectangle {
            x: walker.step.footX - walker.x - width / 2
            y: walker.step.footY - walker.y - height * 0.55
            width: walker.step.w * 0.7
            height: Math.max(8, width * 0.32)
            radius: height / 2
            color: walker.side === 0 ? Board.camp(0).glow : Board.camp(1).glow
            opacity: 0.55 + 0.4 * walker.step.dust
        }
        Rectangle {
            x: walker.step.footX - walker.x - width / 2
            y: walker.step.footY - walker.y - height * 0.7
            width: walker.step.w * 0.42
            height: Math.max(4, width * 0.28)
            radius: height / 2
            color: walker.side === 0 ? "#e7f2ff" : "#ffd5d0"
            opacity: 0.34 * walker.step.shadow
        }
        Rectangle {
            visible: walker.step.trail > 0.15
            property var tail: walker.poseAt(Math.max(0, walker.travel - 0.14))
            property real x1: tail ? tail.footX : walker.step.footX
            property real y1: tail ? tail.footY : walker.step.footY
            property real x2: walker.step.footX
            property real y2: walker.step.footY
            x: x1 - walker.x
            y: y1 - walker.y - height / 2
            width: Math.max(1, Math.hypot(x2 - x1, y2 - y1))
            height: Math.max(3, walker.step.w * 0.07)
            radius: height / 2
            color: walker.side === 0 ? "#9ec8ff" : "#ff6a62"
            opacity: 0.55 * walker.step.trail
            transformOrigin: Item.Left
            rotation: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
        }
        Repeater {
            model: 4
            Rectangle {
                required property int index
                visible: walker.step.dust > 0.12
                property real spread: (index - 1.5) * walker.step.w * 0.2
                width: walker.step.w * (0.2 + index * 0.04) * Math.max(0.35, walker.step.dust)
                height: width * 0.34
                radius: height / 2
                x: walker.step.footX - walker.x + spread - width / 2
                y: walker.step.footY - walker.y - height * (0.25 + index * 0.18)
                color: index % 2 === 0 ? "#d9d3c8" : (walker.side === 0 ? "#b9d6ff" : "#ff8d86")
                opacity: walker.step.dust * (0.4 - index * 0.06)
            }
        }
        Repeater {
            model: 3
            TeamFigure {
                required property int index
                property real lag: (index + 1) * 0.075
                property var ghost: walker.poseAt(Math.max(0, walker.travel - lag))
                visible: ghost !== null && walker.step.trail > 0.05
                x: ghost ? ghost.x - walker.x : 0
                y: ghost ? ghost.y - walker.y : 0
                width: ghost ? ghost.w : 0
                height: ghost ? ghost.h : 0
                opacity: (0.26 - index * 0.06) * walker.step.trail
                source: table.pieceSource(walker.side, walker.type)
                side: walker.side
                depth: 0.4
            }
        }
        TeamFigure {
            anchors.fill: parent
            source: table.pieceSource(walker.side, walker.type)
            side: walker.side
            outline: Game.figureOutline
            depth: 1 + walker.step.hop * 0.85
            transform: [
                Scale {
                    origin.x: walker.width / 2
                    origin.y: walker.height
                    xScale: walker.step.squashX
                    yScale: walker.step.squashY
                },
                Rotation {
                    origin.x: walker.width / 2
                    origin.y: walker.height
                    angle: walker.step.lean
                }
            ]
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
                ? Board.piecePose(mesh.view3d, index, index, 0, Game.flipped, pc.side, pc.type, index === Game.selected, table.sway)
                : null
            visible: pose !== null
            x: pose ? pose.x : 0
            y: pose ? pose.y : 0
            width: pose ? pose.w : 0
            height: pose ? pose.h : 0
            z: pose ? pose.z : 0

            Rectangle {
                x: man.pose ? man.pose.footX - man.x - width / 2 : 0
                y: man.pose ? man.pose.footY - man.y - height * 0.32 : 0
                width: man.width * 0.9 * (man.pose ? man.pose.shadow : 1)
                height: Math.max(7, width * 0.28)
                radius: height / 2
                color: "#070605"
                opacity: 0.5 * (man.pose ? man.pose.shadow : 1)
            }
            Rectangle {
                x: man.pose ? man.pose.footX - man.x - width / 2 : 0
                y: man.pose ? man.pose.footY - man.y - height * 0.55 : 0
                width: man.width * (man.index === table.gazeSq ? 0.96 : 0.7)
                height: Math.max(8, width * 0.32)
                radius: height / 2
                color: man.pc.side === 0 ? Board.camp(0).glow : Board.camp(1).glow
                opacity: man.index === table.gazeSq ? 0.98 : (0.62 + (man.index === Game.selected ? 0.28 * table.sway : 0))
            }
            Rectangle {
                x: man.pose ? man.pose.footX - man.x - width / 2 : 0
                y: man.pose ? man.pose.footY - man.y - height * 0.72 : 0
                width: man.width * 0.4
                height: Math.max(4, width * 0.28)
                radius: height / 2
                color: man.pc.side === 0 ? "#e7f2ff" : "#ffd5d0"
                opacity: 0.32
            }
            TeamFigure {
                anchors.fill: parent
                source: man.live ? table.pieceSource(man.pc.side, man.pc.type) : ""
                side: man.pc ? man.pc.side : 0
                outline: Game.figureOutline
                depth: 1.15 + (man.pose ? man.pose.hop * 0.9 : 0)
                transform: [
                    Scale {
                        origin.x: man.width / 2
                        origin.y: man.height
                        xScale: man.pose ? man.pose.squashX : 1
                        yScale: man.pose ? man.pose.squashY : 1
                    },
                    Rotation {
                        origin.x: man.width / 2
                        origin.y: man.height
                        angle: man.pose ? man.pose.lean : 0
                    }
                ]
            }
        }
    }

    Marcher {
        marching: table.glideOn
        travel: table.glideT
        fromSq: table.glideFrom
        toSq: table.glideTo
        side: table.glideSide
        type: table.glideType
    }
    Marcher {
        marching: table.rookOn
        travel: table.rookT
        fromSq: table.rookFrom
        toSq: table.rookTo
        side: table.rookSide
        type: table.rookType
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
    Rectangle {
        visible: table.coachLine !== ""
        z: 950
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        width: Math.min(parent.width * 0.78, 680)
        height: coachText.implicitHeight + 16
        radius: 3
        color: table.coachSev >= 2 ? "#ee140e0c" : "#ee1c160c"
        border.color: table.coachSev >= 2 ? "#e07070" : "#e2b657"
        border.width: 1
        Text {
            id: coachText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            text: table.coachLine
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: "#f6efe2"
            font.family: table.fontFamily
            font.pixelSize: 15
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 900
        hoverEnabled: true
        cursorShape: (Game.inputEnabled && table.hoverSq >= 0) ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPositionChanged: function (mouse) { table.trackGaze(mouse.x, mouse.y) }
        onExited: table.clearGaze()
        onClicked: function (mouse) {
            if (!Game.inputEnabled) return
            var boardSq = Board.hit(mesh.view3d, mouse.x, mouse.y, Game.flipped)
            var bodySq = table.pieceAt(mouse.x, mouse.y)
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
