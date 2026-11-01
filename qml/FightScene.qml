import QtQuick
import "fight.js" as Duel
import "board3d.js" as Board

Item {
    id: scene
    property string fontFamily: "Liberation Serif"
    property real phase: 0
    property bool closed: false
    property bool struck: false
    property bool feint: false
    property bool reel: false
    property int reelIndex: 0

    readonly property var reelCards: [
        { as: 0, at: 1, ds: 1, dt: 1 },
        { as: 0, at: 2, ds: 1, dt: 1 },
        { as: 1, at: 4, ds: 0, dt: 2 },
        { as: 0, at: 2, ds: 1, dt: 4 },
        { as: 1, at: 6, ds: 0, dt: 6 },
        { as: 0, at: 5, ds: 1, dt: 3 },
        { as: 1, at: 3, ds: 0, dt: 5 },
        { as: 0, at: 4, ds: 1, dt: 1 },
        { as: 1, at: 2, ds: 0, dt: 2 }
    ]
    readonly property var reelCard: reelCards[reelIndex]

    function reelName(side, type) {
        var alliance = ["", "Footman", "Paladin", "Priest", "Cannon Keep", "Archmage", "Storm King"]
        var horde = ["", "Grunt", "Wolf Rider", "Witch Doctor", "Demolisher", "Far Seer", "Warchief"]
        var row = side === 0 ? alliance : horde
        return row[type] || "piece"
    }
    function reelUrl(side, type) {
        var who = side === 0 ? "alliance" : "horde"
        var kind = ["", "pawn", "knight", "bishop", "rook", "queen", "king"]
        return "qrc:/assets/pieces/" + who + "_" + kind[type] + ".png"
    }

    readonly property var reelClash: ({
        attackerType: reelCard.at,
        defenderType: reelCard.dt,
        attackerSide: reelCard.as,
        defenderSide: reelCard.ds,
        attackerName: reelName(reelCard.as, reelCard.at),
        defenderName: reelName(reelCard.ds, reelCard.dt),
        attackerUrl: reelUrl(reelCard.as, reelCard.at),
        defenderUrl: reelUrl(reelCard.ds, reelCard.dt)
    })
    readonly property var clash: reel ? reelClash : Game.fight
    opacity: reel ? Math.max(0, Math.min(1, Math.min(phase / 10, (200 - phase) / 14))) : 1
    readonly property int atkType: clash.attackerType || 0
    readonly property int defType: clash.defenderType || 0
    readonly property int atkSide: clash.attackerSide || 0
    readonly property var pose: Duel.makePose(phase, atkType, defType, atkSide, clash.defenderSide || 0)
    readonly property string moveLine: (atkType && defType) ? Duel.moveLine(atkType, defType) : ""

    function closeFight() {
        if (closed || !Game.fightActive) return
        closed = true
        clock.stop()
        Game.fightFinished()
    }

    onVisibleChanged: {
        if (visible) {
            closed = false
            struck = false
            feint = false
            phase = 0
            clock.start()
        } else {
            clock.stop()
        }
    }

    Component.onCompleted: {
        if (reel && visible) clock.start()
    }

    Timer {
        id: clock
        interval: 40
        repeat: true
        onTriggered: {
            phase = Math.min(200, phase + 0.62)
            var hitAt = Duel.strikeAt(scene.atkType, scene.defType)
            if (!scene.reel && !feint && phase >= 56 && (scene.atkType === 1 || scene.atkType === 6)) {
                feint = true
                Game.playSound("melee")
            }
            if (!scene.reel && !feint && phase >= 80 && scene.atkType === 4) {
                feint = true
                Game.playSound("siege")
            }
            if (phase >= hitAt && !struck) {
                struck = true
                if (!scene.reel) Game.playSound("hit")
            }
            if (phase >= 200) {
                if (scene.reel) {
                    reelIndex = (reelIndex + 1) % reelCards.length
                    phase = 0
                    struck = false
                    feint = false
                } else {
                    scene.closeFight()
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#12141c" }
            GradientStop { position: 1.0; color: "#05060a" }
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 28
        spacing: 8
        width: parent.width * 0.8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (clash.attackerName || "") + "   against   " + (clash.defenderName || "")
            color: "#f2f2f6"
            font.family: scene.fontFamily
            font.pixelSize: 30
            font.letterSpacing: 1
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: scene.moveLine
            color: "#d5d5de"
            font.family: scene.fontFamily
            font.pixelSize: 18
            opacity: Math.max(0, Math.min(1, (phase - 8) / 12))
        }
    }

    Item {
        id: court
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 100
        anchors.bottomMargin: 16
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        readonly property int defSide: clash.defenderSide === 1 ? 1 : 0
        readonly property var atkCamp: Board.camp(scene.atkSide)
        readonly property var defCamp: Board.camp(defSide)

        function tileSource(file, rank) {
            var turn = ((file * 3 + rank) % 4) * 90
            var kindName = (file + rank) % 2 === 0 ? "marble_black_" : "marble_grey_"
            return "qrc:/assets/ui/" + kindName + turn + ".png"
        }

        SlabMesh {
            id: mesh
            anchors.fill: parent
            files: 8
            ranks: 3
            padTop: 0.26
            padBottom: 0.05
            frontColor: "#3a3228"
            frontDeep: "#1a1612"
            backColor: "#2c261e"
            backDeep: "#120e0c"
            leftColor: court.atkCamp.face
            leftDeep: court.atkCamp.deep
            rightColor: court.defCamp.face
            rightDeep: court.defCamp.deep
            lowerFront: "#1a1612"
            lowerBack: "#120e0c"
            lowerSide: "#100e0c"
            lowerTop: "#241e18"
            lipFront: "#e2c27a"
            lipBack: "#8a7560"
            lipSide: "#f4ead4"
        }

        WarpQuad {
            z: 8
            corners: mesh.view3d ? Board.topQuad(mesh.view3d, -mesh.view3d.margin * 0.72, -0.04, 0.12, mesh.view3d.ranks - 0.12, 0.02) : null
            Rectangle { anchors.fill: parent; color: court.atkCamp.inlay }
        }
        WarpQuad {
            z: 8
            corners: mesh.view3d ? Board.topQuad(mesh.view3d, mesh.view3d.files + 0.04, mesh.view3d.files + mesh.view3d.margin * 0.72, 0.12, mesh.view3d.ranks - 0.12, 0.02) : null
            Rectangle { anchors.fill: parent; color: court.defCamp.inlay }
        }

        Repeater {
            model: 24
            WarpQuad {
                id: tile
                required property int index
                property int file: index % 8
                property int rank: Math.floor(index / 8)
                z: 10 + rank
                corners: mesh.view3d ? Board.tileQuad(mesh.view3d, file, rank) : null
                Image {
                    anchors.fill: parent
                    source: court.tileSource(tile.file, tile.rank)
                    fillMode: Image.Stretch
                    smooth: true
                    mipmap: true
                }
                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: 0.05
                }
                Rectangle {
                    anchors.fill: parent
                    color: tile.file < 4 ? court.atkCamp.wash : court.defCamp.wash
                    opacity: 0.22
                }
            }
        }

        Item {
            id: stage
            z: 40
            anchors.fill: parent
            transform: Translate {
                x: Math.sin(scene.phase * 3.4) * scene.pose.shake
                y: Math.cos(scene.phase * 4.2) * scene.pose.shake * 0.35
            }

            Rectangle {
                x: attacker.stand ? attacker.stand.footX - width / 2 : 0
                y: attacker.stand ? attacker.stand.footY - height * 0.45 : 0
                width: attacker.stand ? attacker.stand.w * 0.72 : 0
                height: Math.max(10, width * 0.36)
                radius: width / 2
                color: scene.atkSide === 0 ? Board.camp(0).glow : Board.camp(1).glow
                opacity: 0.7 * scene.pose.ao
            }
            Rectangle {
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.stand ? defender.stand.footY - height * 0.45 : 0
                width: defender.stand ? defender.stand.w * 0.72 : 0
                height: Math.max(10, width * 0.36)
                radius: width / 2
                color: court.defSide === 1 ? Board.camp(1).glow : Board.camp(0).glow
                opacity: 0.7 * scene.pose.dop
            }

            TeamFigure {
                id: attacker
                property var stand: mesh.view3d
                    ? Board.actorStand(mesh.view3d, scene.pose.ax, scene.pose.ay, scene.atkType, scene.atkSide, 1, 0.12)
                    : null
                x: stand ? stand.x : 0
                y: stand ? stand.y : 0
                width: stand ? stand.w : 1
                height: stand ? stand.h : 1
                source: clash.attackerUrl || ""
                side: scene.atkSide
                opacity: scene.pose.ao
                transformOrigin: Item.Bottom
                transform: [
                    Scale {
                        origin.x: attacker.width / 2
                        origin.y: attacker.height
                        xScale: scene.pose.ascale
                        yScale: scene.pose.ascale
                    },
                    Rotation {
                        origin.x: attacker.width / 2
                        origin.y: attacker.height
                        angle: scene.pose.ar
                    }
                ]
            }

            TeamFigure {
                id: defender
                property var stand: mesh.view3d
                    ? Board.actorStand(mesh.view3d, scene.pose.dx, scene.pose.dy, scene.defType, court.defSide, 1, 0.88)
                    : null
                x: stand ? stand.x : 0
                y: stand ? stand.y : 0
                width: stand ? stand.w : 1
                height: stand ? stand.h : 1
                source: clash.defenderUrl || ""
                side: court.defSide
                mirror: true
                opacity: scene.pose.dop
                transformOrigin: Item.Bottom
                transform: [
                    Scale {
                        origin.x: defender.width / 2
                        origin.y: defender.height
                        xScale: scene.pose.dscale
                        yScale: scene.pose.dscale
                    },
                    Rotation {
                        origin.x: defender.width / 2
                        origin.y: defender.height
                        angle: scene.pose.dr
                    }
                ]
            }

            Rectangle {
                id: bolt
                visible: scene.pose.boltOn
                property real x1: attacker.x + attacker.width * 0.74
                property real y1: attacker.y + attacker.height * 0.46
                property real x2: defender.x + defender.width * 0.28
                property real y2: defender.y + defender.height * 0.46
                x: x1
                y: y1 - height / 2
                width: Math.max(8, Math.hypot(x2 - x1, y2 - y1)) * Math.max(0.05, scene.pose.bolt)
                height: 8
                radius: 4
                color: scene.pose.tint
                opacity: 0.85
                transformOrigin: Item.Left
                rotation: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
            }

            Rectangle {
                visible: scene.pose.boltOn
                property real x1: bolt.x1
                property real y1: bolt.y1
                property real x2: defender.x + defender.width * 0.24
                property real y2: defender.y + defender.height * 0.46
                width: 28
                height: 28
                radius: 14
                color: scene.pose.tint
                x: x1 + (x2 - x1) * scene.pose.bolt - width / 2
                y: y1 + (y2 - y1) * scene.pose.bolt - height / 2
                border.color: "#ffffff"
                border.width: 2
            }

            Rectangle {
                id: lance
                visible: scene.pose.lance > 0.12
                property real x1: attacker.x + attacker.width * 0.62
                property real y1: attacker.y + attacker.height * 0.5
                property real x2: defender.x + defender.width * 0.22
                property real y2: defender.y + defender.height * 0.5
                property real reach: Math.min(1, scene.pose.lance)
                x: x1
                y: y1 - height / 2
                width: Math.max(18, Math.hypot(x2 - x1, y2 - y1) * reach)
                height: 7
                radius: 2
                color: "#e7eef8"
                border.color: "#8aa0bc"
                border.width: 1
                transformOrigin: Item.Left
                rotation: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
            }
            Rectangle {
                visible: scene.pose.lance > 0.12
                width: 16
                height: 13
                color: "#d7e2f2"
                x: lance.x1 + (lance.x2 - lance.x1) * lance.reach - width
                y: lance.y1 + (lance.y2 - lance.y1) * lance.reach - height / 2
                rotation: lance.rotation
            }
            Rectangle {
                visible: scene.pose.muzzle > 0.08
                width: 26 + scene.pose.muzzle * 30
                height: width
                radius: width / 2
                color: "#f2d48a"
                opacity: scene.pose.muzzle * 0.85
                x: attacker.x + attacker.width * 0.78 - width / 2
                y: attacker.y + attacker.height * 0.38 - height / 2
            }
            Rectangle {
                visible: scene.pose.cannon > 0.02 && scene.pose.cannon < 0.98
                property real x1: attacker.x + attacker.width * 0.78
                property real y1: attacker.y + attacker.height * 0.42
                property real x2: defender.x + defender.width * 0.32
                property real y2: defender.y + defender.height * 0.42
                width: 20
                height: 20
                radius: 10
                color: "#241c16"
                border.color: "#e2b657"
                border.width: 2
                x: x1 + (x2 - x1) * scene.pose.cannon - width / 2
                y: y1 + (y2 - y1) * scene.pose.cannon
                    - Math.sin(scene.pose.cannon * Math.PI) * stage.height * 0.08
                    - height / 2
            }

            Image {
                visible: scene.pose.dslash > 0.35
                x: defender.x + defender.width * 0.08 - width * 0.2
                y: defender.y + defender.height * 0.16
                width: stage.width * 0.16
                height: width
                source: "qrc:/assets/fx/slash.png"
                fillMode: Image.PreserveAspectFit
                opacity: scene.pose.dslash * 0.8
                mirror: true
            }

            Image {
                visible: !scene.pose.boltOn && (scene.pose.slash > 0.35 || scene.pose.lance > 0.85)
                x: attacker.x + attacker.width * 0.82 + (defender.x - (attacker.x + attacker.width * 0.82)) * 0.42 - width / 2
                y: attacker.y + attacker.height * 0.18
                width: stage.width * 0.18
                height: width
                source: scene.pose.fx
                fillMode: Image.PreserveAspectFit
                opacity: Math.max(scene.pose.slash, scene.pose.lance) * 0.92
            }

            Rectangle {
                visible: scene.pose.ring > 0.05
                x: defender.x + defender.width / 2 - width / 2
                y: defender.y + defender.height * 0.42 - height / 2
                width: 18 + scene.pose.ring * stage.width * 0.12
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 5
                border.color: scene.pose.ringTint
                opacity: 0.35 + scene.pose.ring * 0.5
            }

            Rectangle {
                visible: scene.pose.dust > 0.05
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.stand ? defender.stand.footY - height * 0.5 : 0
                width: (defender.stand ? defender.stand.w : 80) * 1.3 * Math.max(0.4, scene.pose.dust)
                height: width * 0.28
                radius: height / 2
                color: "#AA9a9aa4"
                opacity: scene.pose.dust * 0.85
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: scene.pose.tint
        opacity: scene.pose.flash
    }

    Rectangle {
        visible: !reel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        width: 150
        height: 40
        color: "#14161c"
        border.color: "#d5d5de"
        Text {
            anchors.centerIn: parent
            text: "Skip the clash"
            color: "#f2f2f6"
            font.family: scene.fontFamily
            font.pixelSize: 14
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                Game.playSound("click")
                scene.closeFight()
            }
        }
    }
}
