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
    property bool reel: false
    property int reelIndex: 0
    property int spin: 0
    property int variant: 0
    property int cueAt: 0
    readonly property bool hear: !reel || Game.titleAudio

    readonly property var reelCards: [
        { as: 0, at: 1, ds: 1, dt: 1 },
        { as: 1, at: 4, ds: 0, dt: 1 },
        { as: 0, at: 5, ds: 1, dt: 3 },
        { as: 0, at: 2, ds: 1, dt: 2 },
        { as: 1, at: 6, ds: 0, dt: 6 },
        { as: 1, at: 3, ds: 0, dt: 5 },
        { as: 0, at: 4, ds: 1, dt: 4 },
        { as: 1, at: 2, ds: 0, dt: 4 },
        { as: 0, at: 1, ds: 1, dt: 6 }
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
    readonly property var pose: Duel.makePose(phase, atkType, defType, atkSide, clash.defenderSide || 0, variant)
    readonly property string moveLine: (atkType && defType) ? Duel.moveLine(atkType, defType, variant) : ""

    function beamPoint(t, wobble) {
        var x1 = attacker.x + attacker.width * 0.62
        var y1 = attacker.y + attacker.height * 0.40
        var x2 = defender.x + defender.width * 0.34
        var y2 = defender.y + defender.height * 0.42
        var u = Math.max(0, Math.min(1, t))
        var x = x1 + (x2 - x1) * u
        var y = y1 + (y2 - y1) * u
        var ang = Math.atan2(y2 - y1, x2 - x1)
        var off = Math.sin(phase * 0.62 + u * 16) * (wobble || 0) * 30
        return {
            x: x + Math.cos(ang + Math.PI / 2) * off,
            y: y + Math.sin(ang + Math.PI / 2) * off,
            ang: ang * 180 / Math.PI,
            span: Math.max(8, Math.hypot(x2 - x1, y2 - y1))
        }
    }

    function shotPoint(t) {
        var x1 = attacker.x + attacker.width * 0.78
        var y1 = attacker.y + attacker.height * 0.40
        var x2 = defender.x + defender.width * (0.32 + pose.overshoot)
        var y2 = defender.y + defender.height * (pose.overshoot > 0.2 ? 0.02 : 0.48)
        var u = Math.max(0, t)
        var along = Math.min(u, 1)
        var arc = Math.sin(along * Math.PI) * court.height * 0.18 * pose.ballArc
        return {
            x: x1 + (x2 - x1) * u,
            y: y1 + (y2 - y1) * along - arc,
            ground: y1 + (y2 - y1) * along
        }
    }

    function handPoint(item, lean, fx, fy, grow) {
        var g = grow || 1
        var ox = item.x + item.width * 0.5
        var oy = item.y + item.height
        var dx = item.width * (fx - 0.5) * g
        var dy = item.height * (fy - 1) * g
        var a = (lean || 0) * Math.PI / 180
        var c = Math.cos(a)
        var s = Math.sin(a)
        return {
            x: ox + dx * c - dy * s,
            y: oy + dx * s + dy * c
        }
    }

    function steelPoint(hand, ang, len, t) {
        var a = (ang || 0) * Math.PI / 180
        var u = t === undefined ? 1 : t
        return {
            x: hand.x + Math.cos(a) * len * u,
            y: hand.y + Math.sin(a) * len * u
        }
    }

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
            cueAt = 0
            phase = 0
            if (reel) variant = (reelIndex + spin) % 3
            else variant = Math.floor(Math.random() * 3)
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
            var hitAt = Duel.strikeAt(scene.atkType, scene.defType, scene.variant)
            var fallAt = Duel.deathAt(scene.atkType, scene.defType, scene.variant)
            if (scene.hear) {
                var list = Duel.cues(scene.atkType, scene.defType, scene.variant, scene.atkSide, court.defSide)
                while (scene.cueAt < list.length && phase >= list[scene.cueAt].at) {
                    Game.playSound(list[scene.cueAt].sound)
                    scene.cueAt += 1
                }
            }
            if (phase >= hitAt && !struck) {
                struck = true
                if (scene.hear) Game.playSound("hit")
                if (!scene.reel) Game.setMusicBed(0.16)
            }
            if (!doomed && phase >= fallAt + 6) {
                doomed = true
                if (!scene.reel) {
                    Game.setMusicBed(0.08)
                    Game.playSound("sting")
                    Game.playSound(Duel.deathSound(scene.defType))
                } else if (Game.titleAudio) {
                    Game.playSound(Duel.deathSound(scene.defType))
                }
            }
            if (phase >= 200) {
                if (scene.reel) {
                    reelIndex = (reelIndex + 1) % reelCards.length
                    if (reelIndex === 0) spin = (spin + 1) % 3
                    variant = (reelIndex + spin) % 3
                    phase = 0
                    struck = false
                    doomed = false
                    cueAt = 0
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
            GradientStop { position: 0.0; color: "#10141e" }
            GradientStop { position: 0.55; color: "#080a12" }
            GradientStop { position: 1.0; color: "#03040a" }
        }
    }

    Repeater {
        model: 36
        Rectangle {
            required property int index
            width: index % 7 === 0 ? 2.4 : 1.4
            height: width
            radius: width
            color: index % 6 === 0 ? "#e2b657" : "#f4f7ff"
            opacity: 0.14 + (index % 5) * 0.07
            x: (index * 149 + 31) % Math.max(1, parent.width)
            y: (index * 97 + 19) % Math.max(1, parent.height)
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
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: (clash.attackerName || "") + "   against   " + (clash.defenderName || "")
            color: "#f2f2f6"
            font.family: scene.fontFamily
            font.pixelSize: 30
            font.letterSpacing: 1
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 16
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
                    xScale: 1 + scene.pose.flash * 0.12
                    yScale: 1 + scene.pose.flash * 0.12
                }
            ]

            Rectangle {
                x: attacker.stand ? attacker.stand.footX - width / 2 + attacker.width * 0.06 : 0
                y: attacker.stand ? attacker.stand.footY - height * 0.18 : 0
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
                x: defender.stand ? defender.stand.footX - width / 2 + defender.width * 0.06 : 0
                y: defender.stand ? defender.stand.footY - height * 0.18 : 0
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

            Image {
                visible: scene.pose.aura > 0.08
                x: attacker.x - attacker.width * 0.22
                y: attacker.y - attacker.height * 0.1
                width: attacker.width * 1.45
                height: attacker.height * 1.2
                source: scene.pose.fx
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: scene.pose.aura * 0.78
                rotation: scene.phase * (scene.atkType === 3 && scene.atkSide === 1 ? 1.8 : 0.4)
                transformOrigin: Item.Center
            }

            Repeater {
                model: scene.pose.aura > 0.15 ? 10 : 0
                Rectangle {
                    required property int index
                    property real ang: scene.phase * 0.08 + index * 0.628
                    property real rad: attacker.width * (0.62 + 0.08 * Math.sin(scene.phase * 0.15 + index))
                    width: index % 2 === 0 ? 7 : 4
                    height: width
                    radius: width / 2
                    color: index % 2 === 0 ? scene.pose.tint : "#fff6ea"
                    opacity: scene.pose.aura * 0.9
                    x: attacker.x + attacker.width * 0.5 + Math.cos(ang) * rad - width / 2
                    y: attacker.y + attacker.height * 0.42 + Math.sin(ang) * rad * 0.46 - height / 2
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
                opacity: 0.16 * Math.max(scene.pose.slash, scene.pose.lance * 0.85, scene.pose.bolt * 0.65) * scene.pose.ao
            }

            TeamFigure {
                id: attacker
                property var stand: mesh.view3d
                    ? Board.actorStand(mesh.view3d, scene.pose.ax, scene.pose.ay, scene.atkType, scene.atkSide, 1.1, scene.pose.adepth)
                    : null
                x: stand ? stand.x : 0
                y: stand ? stand.y : 0
                width: stand ? stand.w : 1
                height: stand ? stand.h : 1
                source: clash.attackerUrl || ""
                side: scene.atkSide
                depth: 1.22 + (stand ? stand.rise * 2.6 : 0)
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
                    ? Board.actorStand(mesh.view3d, scene.pose.dx, scene.pose.dy, scene.defType, court.defSide, 1.1, scene.pose.ddepth)
                    : null
                x: stand ? stand.x : 0
                y: stand ? stand.y : 0
                width: stand ? stand.w : 1
                height: stand ? stand.h : 1
                source: clash.defenderUrl || ""
                side: court.defSide
                mirror: true
                depth: 1.22 + (stand ? stand.rise * 2.6 : 0)
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
                visible: scene.pose.boltOn && scene.pose.bolt > 0.04
                property var origin: scene.beamPoint(0, 0)
                x: origin.x
                y: origin.y - height / 2
                width: Math.max(8, origin.span * scene.pose.bolt)
                height: 36
                radius: 18
                color: scene.pose.tint
                opacity: 0.34
                transformOrigin: Item.Left
                rotation: origin.ang
            }
            Rectangle {
                visible: scene.pose.boltOn && scene.pose.bolt > 0.04
                property var origin: scene.beamPoint(0, 0)
                x: origin.x
                y: origin.y - height / 2
                width: Math.max(8, origin.span * scene.pose.bolt)
                height: 9
                radius: 4
                color: "#fffaf2"
                opacity: 0.95
                transformOrigin: Item.Left
                rotation: origin.ang
            }
            Repeater {
                model: scene.pose.boltOn && scene.pose.bolt > 0.04 ? 14 : 0
                Rectangle {
                    required property int index
                    property real t: (index + 0.5) / 14 * scene.pose.bolt
                    property var at: scene.beamPoint(t, scene.pose.wobble)
                    width: Math.max(10, at.span / 14 * 1.6)
                    height: scene.pose.wobble > 0.7 ? 8 : 12
                    radius: height / 2
                    color: index % 2 === 0 ? scene.pose.tint : "#fff6ea"
                    opacity: 0.82
                    x: at.x
                    y: at.y - height / 2
                    transformOrigin: Item.Left
                    rotation: at.ang
                }
            }
            Image {
                visible: scene.pose.boltOn && scene.pose.bolt > 0.08
                property var at: scene.beamPoint(scene.pose.bolt, scene.pose.wobble * 0.35)
                source: scene.pose.fx
                fillMode: Image.PreserveAspectFit
                smooth: true
                width: stage.width * (0.16 + 0.06 * Math.sin(scene.phase * 0.25))
                height: width
                x: at.x - width * 0.5
                y: at.y - height * 0.5
                opacity: 0.95
                rotation: scene.atkType === 3 && scene.atkSide === 1 ? scene.phase * 2.2 : scene.phase * 0.5
            }
            Rectangle {
                visible: scene.pose.counter > 0.08
                property real x1: defender.x + defender.width * 0.28
                property real y1: defender.y + defender.height * 0.4
                property real x2: attacker.x + attacker.width * 0.64
                property real y2: attacker.y + attacker.height * 0.42
                x: x1
                y: y1 - height / 2
                width: Math.max(8, Math.hypot(x2 - x1, y2 - y1) * scene.pose.counter * 0.55)
                height: 10
                radius: 5
                color: scene.pose.dtint
                opacity: 0.75
                transformOrigin: Item.Left
                rotation: Math.atan2(y2 - y1, x2 - x1) * 180 / Math.PI
            }
            Image {
                visible: scene.pose.counter > 0.2 || scene.pose.ward > 0.45
                x: defender.x + defender.width * 0.08
                y: defender.y + defender.height * 0.08
                width: defender.width * 0.9
                height: defender.height * 0.8
                source: scene.pose.dfx
                fillMode: Image.PreserveAspectFit
                opacity: Math.max(scene.pose.counter, scene.pose.ward * 0.7) * 0.8
                rotation: -scene.phase * 0.6
            }

            Repeater {
                model: scene.pose.hoof > 0.08 && attacker.stand ? 4 : 0
                Rectangle {
                    required property int index
                    property real puff: (index * 0.23 + scene.phase * 0.04) % 1
                    width: attacker.stand.shadeW * (0.22 + puff * 0.32)
                    height: width * 0.32
                    radius: height / 2
                    color: index % 2 === 0 ? "#d9d0c4" : "#8d8680"
                    opacity: scene.pose.hoof * (1 - puff) * 0.6
                    x: attacker.stand.footX - width / 2 - puff * attacker.width * 0.42
                    y: attacker.stand.footY - height * 0.45 - puff * 14
                }
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
            Image {
                visible: scene.pose.muzzle > 0.08
                source: "qrc:/assets/fx/fire.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                width: stage.width * (0.12 + scene.pose.muzzle * 0.16)
                height: width
                opacity: scene.pose.muzzle * 0.95
                x: attacker.x + attacker.width * 0.7 - width * 0.35
                y: attacker.y + attacker.height * 0.22 - height * 0.2
                rotation: 70
            }
            Rectangle {
                visible: scene.pose.muzzle > 0.08
                width: 28 + scene.pose.muzzle * 36
                height: width
                radius: width / 2
                color: "#fff1c8"
                opacity: scene.pose.muzzle * 0.9
                x: attacker.x + attacker.width * 0.78 - width / 2
                y: attacker.y + attacker.height * 0.38 - height / 2
            }
            Repeater {
                model: scene.pose.cannon > 0.08 && scene.pose.cannon < 1.3 ? 4 : 0
                Image {
                    required property int index
                    property real t: scene.pose.cannon - (index + 1) * 0.075
                    visible: t > 0.03
                    property var at: scene.shotPoint(t)
                    source: "qrc:/assets/fx/cannonball.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    width: Math.max(34, stage.width * 0.07) * (1 - index * 0.14)
                    height: width * 0.74
                    x: at.x - width * 0.62
                    y: at.y - height * 0.48
                    opacity: 0.22 * (1 - index * 0.15)
                }
            }
            Item {
                id: ball
                visible: scene.pose.cannon > 0.03 && scene.pose.cannon < 1.32
                property var at: scene.shotPoint(scene.pose.cannon)
                Rectangle {
                    visible: ball.visible
                    width: Math.max(16, stage.width * 0.022)
                    height: width * 0.36
                    radius: height / 2
                    color: "#000000"
                    opacity: 0.4 * (scene.pose.cannon < 1 ? 1 : 0)
                    x: ball.at.x - width / 2
                    y: ball.at.ground - height / 2
                }
                Image {
                    source: "qrc:/assets/fx/cannonball.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    width: Math.max(52, stage.width * 0.1)
                    height: width * 0.74
                    x: ball.at.x - width * 0.62
                    y: ball.at.y - height * 0.48
                    opacity: scene.pose.cannon < 1.05 ? 1 : Math.max(0, 1 - (scene.pose.cannon - 1.05) / 0.22)
                }
            }
            Repeater {
                model: scene.pose.muzzle > 0.05 ? 6 : 0
                Rectangle {
                    required property int index
                    property real age: scene.pose.muzzle - index * 0.12
                    visible: age > 0
                    width: 22 + Math.max(0, age) * 70
                    height: width * 0.62
                    radius: height / 2
                    color: "#c9c3b8"
                    opacity: Math.max(0, 0.45 - age * 0.3)
                    x: attacker.x + attacker.width * 0.72 - width / 2 + age * 10
                    y: attacker.y + attacker.height * 0.28 - height / 2 - age * 48
                }
            }

            Item {
                id: aBlade
                visible: !scene.pose.boltOn && scene.pose.lance < 0.15 && scene.pose.cannon < 0.02
                         && (scene.atkType === 1 || scene.atkType === 6) && scene.pose.ao > 0.25
                property var hand: scene.handPoint(attacker, scene.pose.ar, 0.70, 0.33, scene.pose.ascale)
                property real len: Math.max(36, attacker.width * (0.7 + scene.pose.slash * 0.85))
                property real ang: scene.pose.acut
                x: hand.x
                y: hand.y - height / 2
                width: len
                height: 18
                rotation: ang
                transformOrigin: Item.Left
                opacity: 0.55 + scene.pose.slash * 0.45
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 14
                    radius: 7
                    color: scene.atkSide === 0 ? "#d5e6ff" : "#ffb089"
                    opacity: 0.34
                }
                Rectangle {
                    y: (parent.height - height) / 2
                    width: parent.width
                    height: 5
                    radius: 1
                    color: "#e7eef8"
                }
                Rectangle {
                    y: (parent.height - height) / 2 - 1
                    width: parent.width * 0.94
                    height: 2
                    color: "#ffffff"
                }
                Rectangle {
                    x: -3
                    y: (parent.height - height) / 2
                    width: 6
                    height: 20
                    radius: 1
                    color: scene.atkSide === 0 ? "#e2b657" : "#c45a48"
                }
                Rectangle {
                    x: parent.width - 9
                    y: (parent.height - height) / 2
                    width: 10
                    height: 8
                    color: "#f7fbff"
                }
            }
            Item {
                id: aTrail
                visible: aBlade.visible && Math.abs(scene.pose.alag) > 4 && scene.pose.slash > 0.25
                property var hand: aBlade.hand
                x: hand.x
                y: hand.y - height / 2
                width: aBlade.len * 0.92
                height: 12
                rotation: aBlade.ang - scene.pose.alag
                transformOrigin: Item.Left
                opacity: 0.34 * scene.pose.slash
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 8
                    radius: 4
                    color: scene.atkSide === 0 ? "#d5e6ff" : "#ffb089"
                }
            }
            Item {
                id: dBlade
                visible: !scene.pose.boltOn && (scene.defType === 1 || scene.defType === 6) && scene.pose.dop > 0.25
                property var hand: scene.handPoint(defender, scene.pose.dr, 0.30, 0.33, scene.pose.dscale)
                property real len: Math.max(32, defender.width * (0.72 + scene.pose.dslash * 1.05))
                property real ang: 180 + scene.pose.dcut
                x: hand.x
                y: hand.y - height / 2
                width: len
                height: 16
                rotation: ang
                transformOrigin: Item.Left
                opacity: 0.5 + scene.pose.dslash * 0.5
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 12
                    radius: 6
                    color: court.defSide === 0 ? "#d5e6ff" : "#ffb089"
                    opacity: 0.32
                }
                Rectangle {
                    y: (parent.height - height) / 2
                    width: parent.width
                    height: 5
                    radius: 1
                    color: "#f4f7fb"
                }
                Rectangle {
                    x: -3
                    y: (parent.height - height) / 2
                    width: 6
                    height: 18
                    radius: 1
                    color: court.defSide === 0 ? "#e2b657" : "#c45a48"
                }
            }
            Image {
                visible: aBlade.visible && scene.pose.slash > 0.4
                property var tip: scene.steelPoint(aBlade.hand, aBlade.ang, aBlade.len, 0.78)
                x: tip.x - width * 0.42
                y: tip.y - height * 0.5
                width: stage.width * 0.16
                height: width
                source: "qrc:/assets/fx/slash.png"
                fillMode: Image.PreserveAspectFit
                opacity: scene.pose.slash * 0.8
                rotation: aBlade.ang - 16
            }
            Image {
                visible: dBlade.visible && scene.pose.dslash > 0.4
                property var tip: scene.steelPoint(dBlade.hand, dBlade.ang, dBlade.len, 0.7)
                x: tip.x - width * 0.5
                y: tip.y - height * 0.5
                width: stage.width * 0.13
                height: width
                source: "qrc:/assets/fx/slash.png"
                fillMode: Image.PreserveAspectFit
                opacity: scene.pose.dslash * 0.7
                mirror: true
                rotation: dBlade.ang + 16
            }
            Image {
                visible: scene.pose.lance > 0.8 && !scene.pose.boltOn
                x: attacker.x + attacker.width * 0.62
                y: attacker.y + attacker.height * 0.08
                width: stage.width * 0.2
                height: width
                source: "qrc:/assets/fx/slash.png"
                fillMode: Image.PreserveAspectFit
                opacity: scene.pose.lance * 0.75
                rotation: -18 + scene.pose.ar * 0.2
            }

            Repeater {
                model: scene.pose.sparks > 0.12 ? 16 : 0
                Rectangle {
                    required property int index
                    property real ang: index * 0.393 + 0.15
                    property real dist: scene.pose.sparks * (12 + (index % 5) * 11)
                    property var hit: aBlade.visible
                        ? scene.steelPoint(aBlade.hand, aBlade.ang, aBlade.len, dBlade.visible ? 0.62 : 0.78)
                        : null
                    property real cx: hit ? hit.x : (attacker.x + attacker.width * 0.78 + defender.x + defender.width * 0.22) * 0.5
                    property real cy: hit ? hit.y : (attacker.y + attacker.height * 0.34 + defender.y + defender.height * 0.4) * 0.5
                    width: 2 + (index % 3)
                    height: width * (index % 2 === 0 ? 3.4 : 1.15)
                    radius: 1
                    color: index % 3 === 0 ? "#fff6d0" : (index % 3 === 1 ? "#ffb14a" : "#ffffff")
                    opacity: scene.pose.sparks * (0.55 + (index % 4) * 0.1)
                    x: cx + Math.cos(ang) * dist - width / 2
                    y: cy + Math.sin(ang) * dist * 0.7 - height / 2
                    rotation: ang * 57.3
                }
            }

            Rectangle {
                id: liftCol
                visible: scene.pose.boltOn && defender.stand && defender.stand.rise > 0.22 && scene.pose.dop > 0.12
                x: defender.stand ? defender.stand.footX - width / 2 : 0
                y: defender.y + defender.height * 0.9
                width: Math.max(18, defender.width * 0.18)
                height: defender.stand ? Math.max(0, defender.stand.footY - y) : 0
                color: scene.pose.tint
                opacity: 0.34
            }
            Rectangle {
                visible: liftCol.visible
                x: liftCol.x + liftCol.width * 0.34
                y: liftCol.y
                width: Math.max(4, liftCol.width * 0.28)
                height: liftCol.height
                color: "#fff6ea"
                opacity: 0.72
            }
            Repeater {
                model: liftCol.visible ? 7 : 0
                Rectangle {
                    required property int index
                    property real u: (scene.phase * 0.03 + index * 0.17) % 1
                    width: 5 + (index % 3) * 2
                    height: width
                    radius: width / 2
                    color: index % 2 === 0 ? scene.pose.tint : "#fff6ea"
                    opacity: 0.35 + 0.55 * (1 - u)
                    x: liftCol.x + liftCol.width * 0.5 + Math.sin(scene.phase * 0.2 + index) * 16 - width / 2
                    y: liftCol.y + liftCol.height * (1 - u) - height / 2
                }
            }

            Rectangle {
                visible: scene.pose.ring > 0.05 || scene.pose.ward > 0.05
                property real power: Math.max(scene.pose.ring, scene.pose.ward)
                x: defender.x + defender.width / 2 - width / 2
                y: defender.y + defender.height * 0.42 - height / 2
                width: defender.width * (0.55 + power * 0.7)
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 5
                border.color: scene.pose.ringTint
                opacity: 0.28 + power * 0.65
                rotation: scene.phase * 1.4
            }
            Rectangle {
                visible: scene.pose.ward > 0.12
                x: defender.x + defender.width / 2 - width / 2
                y: defender.y + defender.height * 0.42 - height / 2
                width: defender.width * (0.85 + scene.pose.ward * 0.35)
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: "#fff6ea"
                opacity: scene.pose.ward * 0.8
                rotation: -scene.phase * 2.1
            }
            Repeater {
                model: scene.pose.ward > 0.15 ? 8 : 0
                Rectangle {
                    required property int index
                    property real ang: scene.phase * 0.05 + index * 0.785
                    property real rad: defender.width * (0.48 + scene.pose.ward * 0.2)
                    width: 8
                    height: 3
                    radius: 1
                    color: scene.pose.ringTint
                    opacity: scene.pose.ward
                    x: defender.x + defender.width * 0.5 + Math.cos(ang) * rad - width / 2
                    y: defender.y + defender.height * 0.42 + Math.sin(ang) * rad * 0.42 - height / 2
                    rotation: ang * 57.3
                }
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

            Repeater {
                model: scene.pose.debris > 0.08 ? 7 : 0
                Image {
                    required property int index
                    property real ang: index * 0.9 - 0.5
                    property real dist: scene.pose.debris * (16 + index * 12)
                    source: "qrc:/assets/fx/boulder.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    width: 14 + (index % 3) * 12
                    height: width
                    opacity: scene.pose.debris * (0.9 - index * 0.08)
                    x: defender.x + defender.width * 0.4 + Math.cos(ang) * dist - width / 2
                    y: defender.y + defender.height * 0.32 - Math.abs(Math.sin(ang)) * dist * 0.85 - height / 2
                    rotation: scene.phase * (6 + index * 4)
                }
            }

            Image {
                visible: scene.pose.flash > 0.05 && scene.atkType === 4
                property real boom: 1 - Math.min(1, scene.pose.flash / 0.36)
                x: defender.x + defender.width * 0.28 - width * 0.35
                y: defender.y + defender.height * 0.2 - height * 0.2
                width: stage.width * (0.16 + boom * 0.14)
                height: width
                source: "qrc:/assets/fx/fire.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: Math.min(1, scene.pose.flash * 2.2)
            }
            Image {
                visible: scene.pose.flash > 0.05
                property real boom: 1 - Math.min(1, scene.pose.flash / 0.36)
                x: defender.x + defender.width * 0.38 - width / 2
                y: defender.y + defender.height * 0.4 - height / 2
                width: stage.width * (0.2 + boom * 0.22)
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
                model: 12
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
        opacity: scene.pose.flash * 0.42
    }
    Rectangle {
        anchors.fill: parent
        color: "#fff6ea"
        opacity: scene.pose.flash * 0.16
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
