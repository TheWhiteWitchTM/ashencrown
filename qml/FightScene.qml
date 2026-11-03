import QtQuick
import "fight.js" as Duel
import "board3d.js" as Board

Item {
    id: scene
    property string fontFamily: "Liberation Serif"
    property real phase: 0
    property bool closed: false
    property bool struck: false
    property bool doomed: false
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
        if (phase < 200) Game.stopFightSounds()
        if (!reel) Game.setMusicBed(1)
        Game.fightFinished()
    }

    onVisibleChanged: {
        if (visible) {
            closed = false
            struck = false
            doomed = false
            feint = false
            phase = 0
            if (!reel) Game.setMusicBed(0.32)
            clock.start()
        } else {
            clock.stop()
            if (!reel) Game.setMusicBed(1)
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
            var fallAt = Duel.deathAt(scene.atkType, scene.defType)
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
                if (!scene.reel) {
                    Game.playSound("hit")
                    Game.setMusicBed(0.16)
                }
            }
            if (!scene.reel && !doomed && phase >= fallAt + 8) {
                doomed = true
                Game.setMusicBed(0.08)
                Game.playSound("sting")
                Game.playSound(Duel.deathSound(scene.defType))
            }
            if (phase >= 200) {
                if (scene.reel) {
                    reelIndex = (reelIndex + 1) % reelCards.length
                    phase = 0
                    struck = false
                    doomed = false
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
            opacity: 0.66 + 0.34 * (0.5 + 0.5 * Math.sin(scene.phase * 0.1))
            corners: mesh.view3d ? Board.topQuad(mesh.view3d, -mesh.view3d.margin * 0.72, -0.04, 0.12, mesh.view3d.ranks - 0.12, 0.02) : null
            Rectangle { anchors.fill: parent; color: court.atkCamp.inlay }
        }
        WarpQuad {
            z: 8
            opacity: 0.66 + 0.34 * (0.5 + 0.5 * Math.sin(scene.phase * 0.1 + 1.2))
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
            transform: [
                Translate {
                    x: Math.sin(scene.phase * 3.4) * scene.pose.shake
                    y: Math.cos(scene.phase * 4.2) * scene.pose.shake * 0.35
                },
                Scale {
                    origin.x: stage.width / 2
                    origin.y: stage.height * 0.62
                    xScale: 1 + scene.pose.flash * 0.04
                    yScale: 1 + scene.pose.flash * 0.04
                }
            ]

            Rectangle {
                x: attacker.stand ? attacker.stand.footX - width / 2 : 0
                y: attacker.stand ? attacker.stand.footY - height * 0.32 : 0
                width: attacker.stand ? attacker.stand.shadeW * 0.5 * attacker.stand.shadow : 0
                height: Math.max(8, width * 0.28)
                radius: height / 2
                color: "#000000"
                opacity: 0.48 * scene.pose.ao
            }
            Rectangle {
                x: attacker.stand ? attacker.stand.footX - width / 2 : 0
                y: attacker.stand ? attacker.stand.footY - height * 0.5 : 0
                width: attacker.stand ? attacker.stand.shadeW * 0.9 : 0
                height: Math.max(10, width * 0.4)
                radius: width / 2
                color: scene.atkSide === 0 ? Board.camp(0).glow : Board.camp(1).glow
                opacity: (0.5 + 0.22 * Math.abs(Math.sin(scene.phase * 0.16))) * scene.pose.ao
            }
            Rectangle {
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.stand ? defender.stand.footY - height * 0.32 : 0
                width: defender.stand ? defender.stand.shadeW * 0.5 * defender.stand.shadow : 0
                height: Math.max(8, width * 0.28)
                radius: height / 2
                color: "#000000"
                opacity: 0.48 * scene.pose.dop
            }
            Rectangle {
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.stand ? defender.stand.footY - height * 0.5 : 0
                width: defender.stand ? defender.stand.shadeW * 0.9 : 0
                height: Math.max(10, width * 0.4)
                radius: width / 2
                color: court.defSide === 1 ? Board.camp(1).glow : Board.camp(0).glow
                opacity: (0.5 + 0.22 * Math.abs(Math.sin(scene.phase * 0.16 + 0.8))) * scene.pose.dop
            }

            Rectangle {
                x: attacker.x + attacker.width * 0.12
                y: attacker.y + attacker.height * 0.05
                width: attacker.width * 0.76
                height: attacker.height * 0.9
                radius: width * 0.48
                color: scene.atkSide === 0 ? "#3c97ff" : "#9a1219"
                opacity: (0.14 + 0.08 * Math.sin(scene.phase * 0.12)) * scene.pose.ao
            }
            Rectangle {
                x: defender.x + defender.width * 0.12
                y: defender.y + defender.height * 0.05
                width: defender.width * 0.76
                height: defender.height * 0.9
                radius: width * 0.48
                color: court.defSide === 0 ? "#3c97ff" : "#9a1219"
                opacity: (0.14 + 0.08 * Math.sin(scene.phase * 0.12 + 1)) * scene.pose.dop
            }

            Repeater {
                model: 14
                Rectangle {
                    required property int index
                    property bool attackerSide: index < 7
                    property int camp: attackerSide ? scene.atkSide : court.defSide
                    property real seed: index * 1.63
                    property real u: (scene.phase * 0.012 + seed) % 1
                    width: 3 + (index % 3) * 2
                    height: width
                    radius: width / 2
                    color: camp === 0 ? "#c5dcff" : "#ff8d7a"
                    opacity: (0.28 + 0.4 * Math.sin(scene.phase * 0.18 + seed)) * (attackerSide ? scene.pose.ao : scene.pose.dop)
                    x: (attackerSide ? attacker.x : defender.x) + ((index * 41) % 80) / 100 * (attackerSide ? attacker.width : defender.width)
                    y: (attackerSide ? attacker.y : defender.y) + (1 - u) * (attackerSide ? attacker.height : defender.height) * 0.92
                }
            }

            TeamFigure {
                visible: scene.pose.slash > 0.25 || scene.pose.lance > 0.55 || scene.pose.bolt > 0.35
                x: attacker.x - attacker.width * 0.08
                y: attacker.y + attacker.height * 0.015
                width: attacker.width
                height: attacker.height
                source: clash.attackerUrl || ""
                side: scene.atkSide
                depth: 0.35
                opacity: 0.34 * Math.max(scene.pose.slash, scene.pose.lance * 0.85, scene.pose.bolt * 0.65) * scene.pose.ao
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
                depth: 1.08 + (stand ? stand.rise * 2.4 : 0)
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
                visible: scene.pose.dslash > 0.28
                x: defender.x + defender.width * 0.07
                y: defender.y
                width: defender.width
                height: defender.height
                source: clash.defenderUrl || ""
                side: court.defSide
                mirror: true
                depth: 0.35
                opacity: 0.3 * scene.pose.dslash * scene.pose.dop
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
                depth: 1.08 + (stand ? stand.rise * 2.4 : 0)
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

            Item {
                id: bolt
                visible: scene.pose.boltOn
                property real x1: attacker.x + attacker.width * 0.74
                property real y1: attacker.y + attacker.height * 0.42
                property real x2: defender.x + defender.width * 0.28
                property real y2: defender.y + defender.height * 0.42
                property real span: Math.max(8, Math.hypot(x2 - x1, y2 - y1)) * Math.max(0.05, scene.pose.bolt)
                property real ang: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
                x: x1
                y: y1
                Rectangle {
                    y: -11
                    width: bolt.span
                    height: 22
                    radius: 11
                    color: scene.pose.tint
                    opacity: 0.32
                    transformOrigin: Item.Left
                    rotation: bolt.ang
                }
                Rectangle {
                    y: -4
                    width: bolt.span
                    height: 8
                    radius: 4
                    color: scene.pose.tint
                    opacity: 0.92
                    transformOrigin: Item.Left
                    rotation: bolt.ang
                }
                Rectangle {
                    y: -1.5
                    width: bolt.span
                    height: 3
                    radius: 1
                    color: "#fffaf2"
                    transformOrigin: Item.Left
                    rotation: bolt.ang
                }
            }

            Repeater {
                model: scene.pose.boltOn ? 5 : 0
                Rectangle {
                    required property int index
                    property real along: (index + 1) / 6 * scene.pose.bolt
                    width: 8 + (index % 2) * 5
                    height: width
                    radius: width / 2
                    color: index % 2 === 0 ? scene.pose.tint : "#fff6ea"
                    opacity: 0.75
                    x: bolt.x1 + (bolt.x2 - bolt.x1) * along - width / 2
                    y: bolt.y1 + (bolt.y2 - bolt.y1) * along - height / 2
                        + Math.sin(scene.phase * 0.5 + index) * 6
                }
            }

            Rectangle {
                visible: scene.pose.boltOn
                property real x1: bolt.x1
                property real y1: bolt.y1
                property real x2: defender.x + defender.width * 0.24
                property real y2: defender.y + defender.height * 0.42
                width: 36
                height: 36
                radius: 18
                color: scene.pose.tint
                opacity: 0.45
                x: x1 + (x2 - x1) * scene.pose.bolt - width / 2
                y: y1 + (y2 - y1) * scene.pose.bolt - height / 2
            }
            Rectangle {
                visible: scene.pose.boltOn
                property real x1: bolt.x1
                property real y1: bolt.y1
                property real x2: defender.x + defender.width * 0.24
                property real y2: defender.y + defender.height * 0.42
                width: 16
                height: 16
                radius: 8
                color: "#fffaf2"
                border.color: scene.pose.tint
                border.width: 2
                x: x1 + (x2 - x1) * scene.pose.bolt - width / 2
                y: y1 + (y2 - y1) * scene.pose.bolt - height / 2
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
                color: scene.atkSide === 0 ? "#d7e4f6" : "#f0d2c8"
                border.color: scene.atkSide === 0 ? "#6e93c4" : "#a84840"
                border.width: 1
                transformOrigin: Item.Left
                rotation: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
            }
            Rectangle {
                visible: scene.pose.lance > 0.2
                x: lance.x
                y: lance.y1 - height / 2
                width: lance.width
                height: 16
                radius: 8
                color: scene.atkSide === 0 ? "#8ec2ff" : "#ff5a4a"
                opacity: 0.28 * scene.pose.lance
                transformOrigin: Item.Left
                rotation: lance.rotation
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
                width: 40 + scene.pose.muzzle * 46
                height: width
                radius: width / 2
                color: scene.atkSide === 0 ? "#9ec4ff" : "#ff6a3a"
                opacity: scene.pose.muzzle * 0.35
                x: attacker.x + attacker.width * 0.78 - width / 2
                y: attacker.y + attacker.height * 0.38 - height / 2
            }
            Rectangle {
                visible: scene.pose.muzzle > 0.08
                width: 18 + scene.pose.muzzle * 22
                height: width
                radius: width / 2
                color: "#fff1c8"
                opacity: scene.pose.muzzle * 0.9
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
                width: (defender.stand ? defender.stand.shadeW : 80) * 1.35 * Math.max(0.4, scene.pose.dust)
                height: width * 0.28
                radius: height / 2
                color: "#AA9a9aa4"
                opacity: scene.pose.dust * 0.85
            }
            Repeater {
                model: 3
                Rectangle {
                    required property int index
                    visible: scene.pose.dust > 0.08 && defender.stand
                    width: defender.stand ? defender.stand.shadeW * (0.55 + index * 0.22) * scene.pose.dust : 0
                    height: width * 0.3
                    radius: height / 2
                    color: index === 1 ? (scene.atkSide === 0 ? "#9ec2ff" : "#ff7a68") : "#c8c4bc"
                    opacity: scene.pose.dust * (0.45 - index * 0.08)
                    x: defender.stand ? defender.stand.footX + (index - 1) * width * 0.35 - width / 2 : 0
                    y: defender.stand ? defender.stand.footY - height * (0.7 + index * 0.35) : 0
                }
            }

            Image {
                visible: scene.pose.flash > 0.05
                property real boom: 1 - Math.min(1, scene.pose.flash / 0.36)
                x: defender.x + defender.width * 0.38 - width / 2
                y: defender.y + defender.height * 0.4 - height / 2
                width: stage.width * (0.14 + boom * 0.16)
                height: width
                source: "qrc:/assets/fx/impact.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: Math.min(1, scene.pose.flash * 2.6)
                rotation: scene.phase * 3
            }
            Rectangle {
                visible: scene.pose.flash > 0.04 && defender.stand
                property real boom: 1 - Math.min(1, scene.pose.flash / 0.36)
                width: defender.stand ? defender.stand.shadeW * (0.7 + boom * 2.6) : 0
                height: width * 0.34
                radius: width / 2
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.stand ? defender.stand.footY - height * 0.5 : 0
                color: "transparent"
                border.width: 4
                border.color: scene.pose.tint
                opacity: (1 - boom) * 0.9
                antialiasing: true
            }
            Repeater {
                model: 8
                Rectangle {
                    required property int index
                    visible: scene.pose.flash > 0.03
                    property real boom: 1 - Math.min(1, scene.pose.flash / 0.36)
                    property real ang: index * 0.785 + 0.25
                    property real dist: (0.15 + boom) * (defender.stand ? defender.stand.shadeW * 0.9 : 48)
                    width: 4 + (index % 3) * 3
                    height: width * (index % 2 ? 2.6 : 1.1)
                    radius: 2
                    color: index % 2 === 0 ? scene.pose.tint : "#fff6e4"
                    opacity: Math.min(1, scene.pose.flash * 3.2) * (1 - boom * 0.35)
                    x: (defender.x + defender.width * 0.42) + Math.cos(ang) * dist - width / 2
                    y: (defender.y + defender.height * 0.4) + Math.sin(ang) * dist * 0.62 - height / 2
                    rotation: ang * 57.3
                }
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
