.pragma library

function clamp01(t) {
    return Math.max(0, Math.min(1, t))
}

function smooth(t) {
    t = clamp01(t)
    return t * t * (3 - 2 * t)
}

function lerp(a, b, t) {
    return a + (b - a) * t
}

function swing(phase, a, b) {
    if (phase < a || phase > b) return 0
    return Math.sin((phase - a) / (b - a) * Math.PI)
}

function killAt(at, dt, variant) {
    var v = (variant || 0) % 3
    if (at === 1 && dt === 1) return [152, 146, 138][v]
    if (at === 1) return [132, 150, 124][v]
    if (at === 2) return [116, 156, 134][v]
    if (at === 4) return [76, 154, 66][v]
    if (at === 3 || at === 5) return [148, 162, 146][v]
    if (at === 6) return [150, 158, 132][v]
    return 140
}

function strikeAt(at, dt, variant) {
    return killAt(at, dt, variant)
}

function deathAt(at, dt, variant) {
    return killAt(at, dt, variant)
}

function deathSound(dt) {
    if (dt === 2) return "fall_knight"
    if (dt === 3) return "fall_bishop"
    if (dt === 4) return "fall_rook"
    if (dt === 5) return "fall_queen"
    if (dt === 6) return "fall_king"
    return "fall_pawn"
}

function castSound(type, side) {
    if (type === 3 || type === 5) return side === 0 ? "holy" : "magic"
    return "melee"
}

function tintFor(at, aside) {
    if (at === 3) return aside === 0 ? "#ffe7a8" : "#c084fc"
    if (at === 5) return aside === 0 ? "#9ecbff" : "#ff4a3a"
    if (at === 4) return aside === 0 ? "#e4eaf2" : "#e07058"
    if (at === 2) return "#eef3fb"
    return "#f7f1e4"
}

function fxFor(at, aside) {
    if (at === 3) return aside === 0 ? "qrc:/assets/fx/holy.png" : "qrc:/assets/fx/shadow.png"
    if (at === 5) return aside === 0 ? "qrc:/assets/fx/holy.png" : "qrc:/assets/fx/fire.png"
    if (at === 4) return "qrc:/assets/fx/boulder.png"
    return "qrc:/assets/fx/slash.png"
}

function base() {
    return {
        ax: 0.16, ay: 0, ar: 0, ao: 1, ascale: 1,
        dx: 0.84, dy: 0, dr: 0, dop: 1, dscale: 1,
        adepth: 0.14, ddepth: 0.86,
        shake: 0,
        boltOn: false, bolt: 0,
        slash: 0, dslash: 0, lance: 0, ring: 0, dust: 0, flash: 0,
        cannon: 0, muzzle: 0, hoof: 0,
        sparks: 0, debris: 0, aura: 0, ward: 0, counter: 0,
        step: 0, overshoot: 0, ballArc: 1, wobble: 0.2,
        acut: -28, dcut: -16, alag: 0, dlag: 0,
        tint: "#f7f1e4",
        ringTint: "#f7f1e4",
        dtint: "#f7f1e4",
        fx: "qrc:/assets/fx/slash.png",
        dfx: "qrc:/assets/fx/slash.png"
    }
}

function moveLine(at, dt, variant) {
    var v = (variant || 0) % 3
    if (at === 1 && dt === 1 && v === 1)
        return "Five cuts, a clinch, and only one blade comes back up."
    if (at === 1 && dt === 1 && v === 2)
        return "A feint, a breath, then the exchange that ends it."
    if (at === 4 && v === 1)
        return "The first ball goes wide. The second does not."
    if (at === 4 && v === 2)
        return "Point-blank. The cannon speaks, and the square breaks."
    if ((at === 3 || at === 5) && v === 1)
        return "Three pulses. The ward takes two of them."
    if ((at === 3 || at === 5) && v === 2)
        return "The spell takes them off the ground, holds them, and lets go."
    var lines = {
        "1x1": "Steel on steel. A high cut, a parry, a bind, then the thrust.",
        "1x2": "The soldier stands into the charge, catches the lance, and brings the rider down.",
        "1x3": "Two cuts, then a short blade breaks the chant.",
        "1x4": "The soldier comes in under the cannon and cracks the stone.",
        "1x5": "The sword goes up through the spell.",
        "1x6": "The soldier steps inside the guard. The second cut lands.",
        "2x1": "The lance drops, the horse runs, and the soldier does not get up.",
        "2x2": "Both lances couch. One rider stays in the saddle.",
        "2x3": "The lance breaks the warding circle and keeps going.",
        "2x4": "The charge hits the gunport. The cannon goes quiet.",
        "2x5": "The rider comes through the fire with the lance still level.",
        "2x6": "The crown stands. The lance comes back around and unseats it.",
        "3x1": "The bolt finds the soldier after the first ward fails.",
        "3x2": "The chant startles the horse. The rider leaves the saddle.",
        "3x3": "Two spells lock, strain, and one of them gives way.",
        "3x4": "The chant runs into the cannon until the stone splits.",
        "3x5": "A lesser ward burns, then the greater one follows.",
        "3x6": "The spell climbs the sword and takes the crown to a knee.",
        "4x1": "The cannon speaks once. The soldier is gone in the smoke.",
        "4x2": "The shot takes the horse out from under the rider.",
        "4x3": "Stone and powder walk through the chant and end it.",
        "4x4": "Cannon against cannon. The second shot is the last.",
        "4x5": "The ball goes through the fire and still hits.",
        "4x6": "The crown slips the first shot. Not the second.",
        "5x1": "The soldier is lifted, held, and dropped.",
        "5x2": "Magic rears the horse and throws the rider.",
        "5x3": "The ward flares, holds for a breath, and is gone.",
        "5x4": "The beam cracks the cannon from the inside.",
        "5x5": "Two great spells. Only one is left standing.",
        "5x6": "The spell reaches for the crown, and the crown kneels.",
        "6x1": "A measured cut, a second, then the soldier falls.",
        "6x2": "The crown lets the horse pass, then cuts the rider down.",
        "6x3": "The crown walks through the bolt and answers it.",
        "6x4": "The crown slips the shot and puts the sword in the stone.",
        "6x5": "The crown closes under the spell and ends it.",
        "6x6": "Crown meets crown. Three cuts, then one of them falls."
    }
    return lines[at + "x" + dt] || "The blow lands."
}

function bob(type, phase) {
    if (type === 2) return Math.sin(phase * 0.95) * 16
    if (type === 4) return Math.abs(Math.sin(phase * 0.42)) * -8
    if (type === 5) return -16 + Math.sin(phase * 0.28) * 6
    if (type === 3) return -8 + Math.sin(phase * 0.32) * 4
    if (type === 6) return Math.sin(phase * 0.4) * 3
    return Math.sin(phase * 0.55) * 4
}

function fell(p, dt, t, how) {
    var e = smooth(t)
    if (how === "slam") {
        p.dy = lerp(-40, 36, e)
        p.ddepth = lerp(0.42, 0.82, e)
        p.dscale = 1 - 0.28 * Math.sin(Math.min(1, e) * Math.PI)
        p.dr = 48 * e
        p.dust = Math.min(1, e * 1.5)
        p.sparks = Math.max(p.sparks, 1 - e)
        p.debris = dt === 4 ? e : 0
        p.dop = 1 - 0.92 * Math.max(0, e - 0.4) / 0.6
        return
    }
    if (dt === 1) {
        if (e < 0.1) {
            var q = e / 0.1
            p.dy = -14 * q
            p.dr = 10 * q
        } else {
            // Fall back onto the board. A clockwise tip dumps the near rank off the front edge.
            var u = (e - 0.1) / 0.9
            p.dy = lerp(-14, 4, smooth(u))
            p.dr = lerp(10, -102, u)
            p.dx -= 0.04 * u
            p.dscale = 1 - 0.06 * u
        }
        p.dust = Math.max(p.dust, e)
    } else if (dt === 2) {
        p.dr = -150 * e
        p.dx -= 0.08 * e
        p.dy = -42 * Math.sin(Math.min(1, e) * Math.PI)
        p.dscale = 1 - 0.1 * e
        p.dust = Math.max(p.dust, Math.sin(Math.min(1, e) * Math.PI))
    } else if (dt === 3) {
        p.dop = 1 - e
        p.dy = -64 * e
        p.dr = -48 * e
        p.dscale = 1 + 0.2 * e
        p.ward = Math.max(p.ward, 1 - e)
    } else if (dt === 4) {
        p.dscale = 1 - 0.74 * e
        p.dy = 62 * e
        p.dust = 1
        p.debris = Math.min(1, e * 1.25)
        p.dr = 16 * Math.sin(e * 14) * (1 - e)
        p.shake = Math.max(p.shake, 20 * (1 - e))
    } else if (dt === 5) {
        p.dr = -90 * e
        p.dop = 1 - e
        p.dy -= 52 * e
        p.dscale = 1 + 0.14 * Math.sin(Math.min(1, e) * Math.PI)
    } else {
        if (e < 0.35) {
            var k = e / 0.35
            p.dy = 10 * k
            p.dr = -12 * k
            p.dscale = 1 - 0.06 * k
        } else {
            var s = (e - 0.35) / 0.65
            p.dy = 10
            p.dr = -12 - 88 * s
            p.dx -= 0.04 * s
            p.dust = Math.max(p.dust, s)
        }
    }
}

function victory(p, at, phase, hit) {
    if (phase < hit + 28) return
    var t = smooth(clamp01((phase - hit - 28) / 16))
    if (at === 2) p.ay -= 28 * t
    else if (at === 3 || at === 5) p.ay -= 20 * t
    else if (at === 4) p.ascale = Math.max(p.ascale, 1 + 0.08 * t)
    else if (at === 6) {
        p.ar = lerp(p.ar, 12, t)
        p.ay -= 8 * t
    } else {
        p.ay -= 16 * t
        p.ar = lerp(p.ar, 16, t)
        p.ax = lerp(p.ax, Math.min(p.ax, 0.38), t)
    }
}

function applyRows(p, phase, rows) {
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i]
        var s = swing(phase, r.a, r.b)
        if (s <= 0) continue
        if (r.who !== "d") {
            p.slash = Math.max(p.slash, s)
            // Positive ar is clockwise: the attacker's head comes in toward the defender.
            p.ar += (r.ar !== undefined ? r.ar : 52) * s
        }
        if (r.who !== "a") {
            p.dslash = Math.max(p.dslash, s)
            p.dr += (r.dr !== undefined ? r.dr : -36) * s
        }
        if (r.who === "a") p.dr += (r.flinch !== undefined ? r.flinch : 14) * s
        if (r.who === "d" && r.recoil) p.ar -= r.recoil * s
        p.ax += (r.ax || 0) * s
        p.dx += (r.dx || 0) * s
        if (r.ay && Math.abs(r.ay * s) > Math.abs(p.ay)) p.ay = r.ay * s
        if (r.dy && Math.abs(r.dy * s) > Math.abs(p.dy)) p.dy = r.dy * s
        if (r.ascale) p.ascale += r.ascale * s
        p.sparks = Math.max(p.sparks, s * s)
        p.shake = Math.max(p.shake, (r.shake || 4) * s)
    }
}

function bladeTrack(phase, rows, side, rest, key0, key1, def0, def1) {
    var prevEnd = -999
    var prevAng = rest
    var nextStart = 9999
    var nextAng = rest
    var live = null
    var lag = 0
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i]
        if (side === "a" && r.who === "d") continue
        if (side === "d" && r.who === "a") continue
        var c0 = r[key0] !== undefined ? r[key0] : def0
        var c1 = r[key1] !== undefined ? r[key1] : def1
        if (phase >= r.a && phase <= r.b) {
            var u = (phase - r.a) / Math.max(1, r.b - r.a)
            var e = u * u * (3 - 2 * u)
            var u2 = Math.max(0, u - 0.16)
            var e2 = u2 * u2 * (3 - 2 * u2)
            live = lerp(c0, c1, e)
            lag = live - lerp(c0, c1, e2)
        }
        if (r.b <= phase && r.b >= prevEnd) {
            prevEnd = r.b
            prevAng = c1
        }
        if (r.a > phase && r.a < nextStart) {
            nextStart = r.a
            nextAng = c0
        }
    }
    if (live !== null) return { ang: live, lag: lag }
    if (prevEnd < -100) {
        if (nextStart > 9000) return { ang: rest, lag: 0 }
        var near = clamp01(1 - (nextStart - phase) / 14)
        return { ang: lerp(rest, nextAng, near), lag: (nextAng - rest) * near * 0.35 }
    }
    if (nextStart > 9000) {
        var back = clamp01((phase - prevEnd) / 12)
        return { ang: lerp(prevAng, rest, back), lag: 0 }
    }
    var gap = Math.max(1, nextStart - prevEnd)
    var t = (phase - prevEnd) / gap
    if (t < 0.42) return { ang: lerp(prevAng, rest, t / 0.42), lag: 0 }
    var wind = (t - 0.42) / 0.58
    return { ang: lerp(rest, nextAng, wind), lag: (nextAng - rest) * wind * 0.4 }
}

function playSword(p, phase, plan, dt) {
    var e = smooth(clamp01(phase / plan.close))
    p.ax = lerp(plan.axFrom || 0.10, plan.axTo, e)
    p.dx = lerp(plan.dxFrom || 0.90, plan.dxTo, e)
    if (phase < plan.kill) {
        p.ay += Math.sin(phase * 0.62) * 4
        p.dy += Math.sin(phase * 0.62 + 0.9) * 3
        p.hoof = phase < plan.close ? 0.6 : 0.15
    }
    applyRows(p, phase, plan.rows)
    var aBlade = bladeTrack(phase, plan.rows, "a", -28, "cut0", "cut1", -74, 8)
    var dBlade = bladeTrack(phase, plan.rows, "d", -16, "dcut0", "dcut1", 64, -14)
    p.acut = aBlade.ang
    p.alag = aBlade.lag
    p.dcut = dBlade.ang
    p.dlag = dBlade.lag
    p.hoof = Math.max(p.hoof, p.slash * 0.4)
    var L = plan.lunge
    if (L && phase >= L.from && phase <= L.to + 10) {
        var lunge = smooth(clamp01((phase - L.from) / (L.to - L.from)))
        p.ax = lerp(p.ax, L.ax, lunge)
        p.ar = lerp(p.ar, L.ar, lunge)
        p.ay = -20 * (1 - lunge)
        p.slash = Math.max(p.slash, 0.35 + 0.65 * lunge)
        p.acut = lerp(p.acut, 4, lunge)
        p.alag = lerp(p.alag, 14, lunge)
        p.dcut = lerp(p.dcut, 26, lunge * 0.7)
        if (lunge > 0.62) p.sparks = Math.max(p.sparks, 1)
        p.dslash *= (1 - lunge)
        p.shake = Math.max(p.shake, 8 * lunge)
    }
    if (phase >= plan.kill) {
        if (L) {
            p.ax = L.ax
            p.ar = L.ar
            p.acut = 4
        }
        if (phase < plan.kill + 12) p.slash = 1
        var settle = smooth(clamp01((phase - plan.kill) / 16))
        p.ar = lerp(p.ar, 18, settle)
        p.acut = lerp(p.acut, -24, settle)
        fell(p, dt, clamp01((phase - plan.kill) / (plan.fall || 24)), plan.how || "")
    }
}

function swordRows(v) {
    if (v === 1) return [
        { a: 16, b: 30, who: "a", ar: 50, ax: 0.05, ay: -16, shake: 6, cut0: -80, cut1: -4 },
        { a: 28, b: 42, who: "d", dr: -40, dx: -0.05, recoil: 16, ax: -0.03, dcut0: 66, dcut1: -20 },
        { a: 42, b: 56, who: "a", ar: 46, ax: 0.045, ay: -10, cut0: 4, cut1: 54 },
        { a: 54, b: 68, who: "d", dr: -44, dx: -0.04, recoil: 12, dcut0: 40, dcut1: -16 },
        { a: 70, b: 86, who: "a", ar: 58, ax: 0.05, ay: -18, shake: 8, cut0: -76, cut1: 18 },
        { a: 88, b: 110, who: "both", ar: 22, dr: -18, ax: 0.05, dx: -0.05, shake: 10, cut0: -8, cut1: 12, dcut0: 14, dcut1: -6 }
    ]
    if (v === 2) return [
        { a: 14, b: 28, who: "a", ar: 22, ax: 0.02, ay: -8, cut0: -36, cut1: -68 },
        { a: 30, b: 44, who: "d", dr: -22, dx: -0.025, recoil: 8, dcut0: 24, dcut1: -46 },
        { a: 52, b: 70, who: "a", ar: 62, ax: 0.07, ay: -20, shake: 8, cut0: -82, cut1: 50 },
        { a: 66, b: 84, who: "d", dr: -48, dx: -0.05, recoil: 18, ax: -0.04, ay: 16, ascale: -0.1, dcut0: 58, dcut1: -18 },
        { a: 86, b: 108, who: "both", ar: 24, dr: -20, ax: 0.055, dx: -0.05, shake: 11, cut0: 8, cut1: -6, dcut0: -4, dcut1: 14 }
    ]
    return [
        { a: 14, b: 34, who: "a", ar: 64, ax: 0.06, ay: -24, shake: 7, cut0: -84, cut1: 8 },
        { a: 36, b: 56, who: "d", dr: -44, dx: -0.05, recoil: 18, ax: -0.045, dcut0: 70, dcut1: -28 },
        { a: 60, b: 82, who: "a", ar: 54, ax: 0.055, ay: -12, shake: 7, cut0: 6, cut1: 62 },
        { a: 84, b: 104, who: "d", dr: -48, dx: -0.045, recoil: 10, ax: -0.05, ay: 18, ascale: -0.1, dcut0: 48, dcut1: -22 },
        { a: 106, b: 128, who: "both", ar: 28, dr: -20, ax: 0.05, dx: -0.05, shake: 12, cut0: -12, cut1: 16, dcut0: 20, dcut1: -10 }
    ]
}

function swordPlan(v) {
    var lunges = [
        { from: 132, to: 150, ax: 0.50, ar: 62 },
        { from: 116, to: 144, ax: 0.52, ar: 68 },
        { from: 112, to: 136, ax: 0.50, ar: 64 }
    ]
    return {
        close: [22, 12, 34][v],
        axTo: [0.30, 0.34, 0.28][v],
        dxTo: [0.70, 0.66, 0.72][v],
        rows: swordRows(v),
        lunge: lunges[v],
        kill: killAt(1, 1, v),
        fall: 24
    }
}

function pawnRows(v) {
    if (v === 1) return [
        { a: 22, b: 44, who: "d", dr: -40, dx: -0.04, recoil: 14, ax: -0.05, dcut0: 62, dcut1: -18 },
        { a: 48, b: 74, who: "a", ar: -16, ax: -0.06, ay: 14, ascale: -0.08, flinch: 0, cut0: 24, cut1: -46 },
        { a: 78, b: 100, who: "a", ar: 56, ax: 0.06, ay: -16, shake: 8, cut0: -74, cut1: 12 },
        { a: 104, b: 124, who: "both", ar: 20, dr: -14, ax: 0.04, dx: -0.03, cut0: -8, cut1: 14, dcut0: 16, dcut1: -6 }
    ]
    if (v === 2) return [
        { a: 16, b: 30, who: "a", ar: 42, ax: 0.05, ay: -10, cut0: -70, cut1: 6 },
        { a: 34, b: 48, who: "a", ar: 48, ax: 0.05, ay: -8, cut0: 8, cut1: 50 },
        { a: 54, b: 72, who: "d", dr: -30, dx: -0.03, recoil: 12, dcut0: 54, dcut1: -16 },
        { a: 76, b: 96, who: "a", ar: 58, ax: 0.06, ay: -16, shake: 8, cut0: -72, cut1: 16 }
    ]
    return [
        { a: 18, b: 38, who: "a", ar: 58, ax: 0.05, ay: -18, shake: 6, cut0: -78, cut1: 6 },
        { a: 42, b: 64, who: "d", dr: -34, dx: -0.04, recoil: 14, ax: -0.03, dcut0: 60, dcut1: -18 },
        { a: 70, b: 92, who: "a", ar: 52, ax: 0.055, ay: -12, shake: 7, cut0: 4, cut1: 56 },
        { a: 96, b: 116, who: "a", ar: 24, ax: 0.03, cut0: 40, cut1: -18 }
    ]
}

function pawnPlan(v, dt) {
    var lunges = [
        { from: 112, to: 130, ax: dt === 4 ? 0.48 : 0.50, ar: 60 },
        { from: 126, to: 148, ax: 0.50, ar: 64 },
        { from: 100, to: 122, ax: 0.51, ar: 62 }
    ]
    return {
        close: [20, 18, 12][v],
        axTo: [0.32, 0.30, 0.34][v],
        dxTo: dt === 4 ? 0.70 : 0.66,
        rows: pawnRows(v),
        lunge: lunges[v],
        kill: killAt(1, dt, v),
        fall: 36
    }
}

function royalRows(v) {
    if (v === 1) return [
        { a: 24, b: 48, who: "d", dr: -36, dx: -0.04, recoil: 10, dcut0: 58, dcut1: -14 },
        { a: 50, b: 78, who: "a", ar: -14, ax: -0.08, ay: 10, ascale: -0.04, flinch: 4, cut0: 18, cut1: -40 },
        { a: 84, b: 108, who: "a", ar: 48, ax: 0.04, ay: -10, cut0: -70, cut1: 8 },
        { a: 112, b: 134, who: "both", ar: 18, dr: -12, ax: 0.03, dx: -0.03, cut0: -6, cut1: 12, dcut0: 14, dcut1: -8 }
    ]
    if (v === 2) return [
        { a: 28, b: 52, who: "a", ar: 18, ax: 0.02, ay: -6, cut0: -30, cut1: -58 },
        { a: 70, b: 100, who: "a", ar: 68, ax: 0.07, ay: -18, shake: 10, cut0: -80, cut1: 20 }
    ]
    return [
        { a: 20, b: 42, who: "a", ar: 58, ax: 0.045, ay: -14, shake: 6, cut0: -76, cut1: 6 },
        { a: 46, b: 70, who: "d", dr: -34, dx: -0.04, recoil: 12, dcut0: 58, dcut1: -16 },
        { a: 76, b: 102, who: "a", ar: -10, ax: -0.07, ay: 8, flinch: 6, cut0: 16, cut1: -36 },
        { a: 108, b: 128, who: "a", ar: 30, ax: 0.04, ay: -8, cut0: -48, cut1: 10 }
    ]
}

function royalPlan(v, dt) {
    var lunges = [
        { from: 128, to: 148, ax: 0.48, ar: 58 },
        { from: 136, to: 156, ax: 0.50, ar: 66 },
        { from: 104, to: 130, ax: 0.50, ar: 70 }
    ]
    return {
        close: [26, 22, 40][v],
        axTo: [0.32, 0.30, 0.30][v],
        dxTo: dt === 4 ? 0.70 : 0.64,
        rows: royalRows(v),
        lunge: lunges[v],
        kill: killAt(6, dt, v),
        fall: v === 0 ? 48 : 38
    }
}

function defenderMagic(p, dt, phase, killT) {
    if (dt !== 3 && dt !== 5) return
    if (phase >= killT) return
    var ward = 0.35 + 0.55 * Math.max(p.dslash, p.counter)
    p.ward = Math.max(p.ward, Math.min(1, ward))
    p.ring = Math.max(p.ring, p.ward * 0.8)
    p.counter = Math.max(p.counter, p.dslash * 0.75)
    p.aura = Math.max(p.aura, p.counter * 0.35)
}

function swordplay(p, phase, v) {
    playSword(p, phase, swordPlan(v), 1)
}

function pawnPress(p, phase, dt, v) {
    var plan = pawnPlan(v, dt)
    playSword(p, phase, plan, dt)
    defenderMagic(p, dt, phase, plan.kill)
    if (dt === 2 && phase < plan.kill) p.dy += Math.sin(phase * 0.9) * 8
    if (dt === 4) p.dscale = Math.max(p.dscale, 1.06)
}

function royal(p, phase, dt, v) {
    var plan = royalPlan(v, dt)
    playSword(p, phase, plan, dt)
    defenderMagic(p, dt, phase, plan.kill)
    if (dt === 4) p.dscale = Math.max(p.dscale, 1.05)
    if (dt === 2 && phase < plan.kill) p.dy += Math.sin(phase * 0.85) * 7
}

function lanceCharge(p, phase, dt, v) {
    var killT = killAt(2, dt, v)
    var wind = smooth(clamp01(phase / 18))
    p.lance = lerp(0.4, 0.75, wind)
    p.ar = 14 * wind
    p.ax = lerp(0.04, 0.12, wind)
    p.hoof = 0.45 * wind
    p.ay = Math.sin(phase * 0.9) * 12 * wind
    if (dt === 2) {
        p.dx = lerp(0.94, 0.78, wind)
        p.dr = 10 * wind
        p.dy = Math.sin(phase * 0.9 + 0.5) * 8
    } else {
        p.dx = dt === 4 ? 0.74 : 0.70
        if (phase < killT) p.dy = bob(dt, phase)
        if (dt === 4) p.dscale = 1.06
    }

    if (v === 1) {
        if (phase > 18 && phase < 72) {
            var t = smooth((phase - 18) / 48)
            p.ax = lerp(0.12, 0.40, t * t)
            p.ay = Math.sin(phase * 1.2) * (16 + 20 * t)
            p.lance = lerp(0.75, 1, t)
            p.hoof = 0.6 + 0.4 * t
            p.dust = t * 0.55
        }
        if (phase > 58 && phase < 86) {
            var slip = swing(phase, 58, 86)
            p.dx += 0.1 * slip
            p.dy = -14 * slip
            p.dr = 18 * slip
            p.sparks = Math.max(p.sparks, slip * slip * 0.6)
            p.slash = Math.max(p.slash, slip * 0.45)
            p.lance = Math.max(0.2, p.lance * (1 - 0.7 * slip))
        }
        if (phase > 86 && phase < 112) {
            var wheel = smooth((phase - 86) / 26)
            p.ax = lerp(0.40, 0.18, wheel)
            p.ar = lerp(16, -14, wheel)
            p.ay = -18 * Math.sin(wheel * Math.PI)
            p.lance = 0.45
        }
        if (phase > 112 && phase < killT) {
            var t2 = smooth((phase - 112) / (killT - 112))
            p.ax = lerp(0.18, 0.48, t2 * t2)
            p.ar = 14
            p.lance = 1
            p.ay = Math.sin(phase * 1.3) * 14
            p.hoof = 1
            p.dust = 0.6
        }
    } else if (v === 2) {
        if (phase > 16 && phase < 96) {
            var t3 = smooth((phase - 16) / 70)
            var meet = dt === 2 ? 0.40 : 0.44
            p.ax = lerp(0.12, meet, t3 * t3)
            p.ay = Math.sin(phase * 1.15) * (14 + 16 * t3)
            p.lance = lerp(0.7, 1, t3)
            p.hoof = 0.55 + 0.45 * t3
            p.dust = t3 * 0.5
            if (dt === 2) {
                p.dx = lerp(0.9, 0.56, t3 * t3)
                p.dy = Math.sin(phase * 1.15 + 0.6) * (12 + 14 * t3)
                p.dr = -8
                p.lance = Math.max(p.lance, t3)
            }
        }
        if (phase > 96 && phase < killT) {
            var rear = swing(phase, 96, killT)
            p.ay = -30 * rear
            p.dy = dt === 2 ? -22 * rear : p.dy
            p.shake = 16 * rear
            p.sparks = rear * rear
            p.slash = rear
        }
    } else if (phase > 20 && phase < killT) {
        var t0 = smooth((phase - 20) / (killT - 28))
        var meet0 = dt === 2 ? 0.38 : 0.42
        p.ax = lerp(0.12, meet0, t0 * t0)
        p.ay = Math.sin(phase * 1.15) * (18 + 18 * t0)
        p.lance = lerp(0.72, 1, t0)
        p.ar = 12
        p.hoof = 0.6 + 0.4 * t0
        p.dust = t0 * 0.5
        if (dt === 2) {
            p.dx = lerp(0.88, 0.58, t0 * t0)
            p.dy = Math.sin(phase * 1.15 + 0.5) * (14 + 12 * t0)
            p.dr = 12
        }
        if (phase > killT - 16) {
            var hit = swing(phase, killT - 16, killT + 4)
            p.shake = Math.max(p.shake, 22 * hit)
            p.slash = Math.max(p.slash, hit)
            p.sparks = Math.max(p.sparks, hit * hit)
        }
    }

    defenderMagic(p, dt, phase, killT)
    if (phase >= killT) {
        p.ax = v === 1 ? 0.48 : (dt === 2 ? 0.40 : 0.44)
        p.ar = 10
        p.lance = phase < killT + 10 ? 1 : p.lance
        p.slash = phase < killT + 10 ? 1 : p.slash
        fell(p, dt, clamp01((phase - killT) / 40), "")
    }
}

function cannonShot(p, phase, dt, v) {
    var killT = killAt(4, dt, v)
    var plant = smooth(clamp01(phase / (v === 2 ? 12 : 16)))
    p.ax = lerp(0.14, v === 2 ? 0.44 : 0.34, plant)
    p.ascale = 1 + 0.06 * plant
    p.dx = dt === 2 ? lerp(0.90, 0.70, plant * 0.45) : (dt === 5 ? 0.74 : 0.70)
    if (phase < killT) p.dy = bob(dt, phase)
    if (dt === 2) p.dy = Math.sin(phase * 0.9) * 8
    if (dt === 4) p.dscale = Math.max(p.dscale, 1.06)
    p.ballArc = v === 2 ? 0.32 : 1.2

    function muzzle(from, span, power) {
        if (phase <= from || phase >= from + span) return
        var kick = Math.sin((phase - from) / span * Math.PI)
        p.ax = lerp(p.ax, p.ax - 0.16 * power, kick)
        p.ar = -12 * kick * power
        p.muzzle = Math.max(p.muzzle, kick * power)
        p.shake = Math.max(p.shake, 24 * kick * power)
        p.dust = Math.max(p.dust, 0.85 * kick)
    }

    if (v === 1) {
        if (phase > 18 && phase < 36) p.ar = 6 * smooth((phase - 18) / 18)
        muzzle(32, 20, 1)
        if (phase >= 36 && phase < 88) {
            p.cannon = (phase - 36) / 40
            p.overshoot = 1.05
            p.ballArc = 0.85
        }
        if (phase > 52 && phase < 88) {
            var slip = swing(phase, 52, 88)
            p.dx += 0.12 * slip
            p.dy = -16 * slip
            p.dr = 10 * slip
        }
        if (phase >= 88 && phase < 108) {
            p.cannon = 0
            p.overshoot = 0
            p.ax = lerp(p.ax, 0.32, smooth((phase - 88) / 20))
        }
        if (phase > 100 && phase < 118) p.ar = 6 * smooth((phase - 100) / 16)
        muzzle(112, 22, 1.15)
        if (phase >= 116 && phase < killT + 4) {
            p.cannon = clamp01((phase - 116) / (killT - 116))
            p.overshoot = 0
            p.ballArc = 1.05
        }
    } else if (v === 2) {
        muzzle(24, 18, 1.25)
        if (phase >= 26 && phase < killT + 2) {
            p.cannon = clamp01((phase - 26) / (killT - 26))
            p.ballArc = 0.32
        }
        if (dt === 6 && phase > 30 && phase < killT - 8) {
            var slipK = swing(phase, 30, killT - 8)
            p.dr = 14 * slipK
            p.dx += 0.04 * slipK
        }
    } else {
        if (phase > 14 && phase < 34) p.ar = 6 * smooth((phase - 14) / 18)
        muzzle(32, 22, 1.1)
        if (phase >= 36 && phase < killT + 4) {
            p.cannon = clamp01((phase - 36) / (killT - 36))
            p.ballArc = 1.25
        }
        if (dt === 6 && phase > 40 && phase < 62) {
            var crown = swing(phase, 40, 62)
            p.dr = 12 * crown
            p.dx += 0.03 * crown
        }
    }

    defenderMagic(p, dt, phase, killT)
    if ((dt === 3 || dt === 5) && phase > 18 && phase < killT) {
        p.ward = Math.max(p.ward, 0.75)
        p.ring = Math.max(p.ring, p.ward)
    }
    if (phase >= killT) {
        p.ax = v === 2 ? 0.36 : 0.26
        p.muzzle = 0
        p.cannon = 0
        p.shake = Math.max(p.shake, 22 * (1 - clamp01((phase - killT) / 14)))
        p.debris = Math.max(p.debris, 1 - clamp01((phase - killT) / 26))
        fell(p, dt, clamp01((phase - killT) / 40), "")
    }
}

function spell(p, phase, at, dt, v) {
    var killT = killAt(at, dt, v)
    var rise = smooth(clamp01(phase / 20))
    p.ax = 0.20
    p.dx = dt === 4 ? 0.72 : 0.74
    p.ay = lerp(0, at === 5 ? -30 : -20, rise) + Math.sin(phase * 0.3) * 6
    if (phase < killT && dt !== 3 && dt !== 5) p.dy = bob(dt, phase)
    if (dt === 3 || dt === 5) p.dy = lerp(0, -12, rise) + Math.sin(phase * 0.3 + 1) * 4
    p.aura = rise
    p.ring = rise * 0.4
    p.boltOn = phase > 22

    if (v === 1) {
        var pulses = [32, 70, 112]
        var reach = [0.5, 0.78, 1]
        var bolt = 0
        for (var i = 0; i < pulses.length; i++) {
            var s = swing(phase, pulses[i], pulses[i] + 24)
            if (s > 0) {
                bolt = Math.max(bolt, reach[i] * (0.4 + 0.6 * s))
                p.shake = Math.max(p.shake, 12 * s)
                p.sparks = Math.max(p.sparks, s * s * 0.7)
            }
        }
        if (phase > pulses[2] + 10) bolt = Math.max(bolt, 1)
        p.bolt = bolt
        p.aura = Math.max(p.aura, 0.45 + 0.55 * bolt)
        p.ward = (dt === 3 || dt === 5 || dt === 6) && phase < killT ? Math.max(0.15, 1 - bolt * 0.85) : 0
        p.ring = Math.max(p.ring, p.ward)
    } else if (v === 2) {
        p.bolt = smooth(clamp01((phase - 22) / 26))
        p.aura = Math.max(rise, p.bolt)
        if (phase > 50 && phase < killT) {
            var lift = smooth(clamp01((phase - 50) / 26))
            p.dy = -40 * lift
            p.dr = 16 * lift
            p.dscale = 1 + 0.06 * lift
            p.ddepth = lerp(0.86, 0.4, lift)
            p.ward = (1 - lift) * 0.9
            p.ring = p.ward
            p.shake = Math.max(p.shake, 8 * lift)
            p.counter = Math.max(0, 0.55 * (1 - lift))
        }
    } else {
        if (phase > 24) p.bolt = smooth(clamp01((phase - 24) / 36))
        p.aura = Math.max(p.aura, p.bolt)
        p.ring = Math.max(p.ring, p.bolt * 0.85)
        if (phase > 72 && phase < 118) {
            var lock = Math.sin((phase - 72) / 46 * Math.PI)
            p.shake = Math.max(p.shake, 14 * lock)
            p.bolt = Math.max(p.bolt, 0.72 + 0.28 * lock)
            p.ward = (dt === 3 || dt === 5 || dt === 6) ? lock : 0.25 * lock
            p.ring = Math.max(p.ring, p.ward)
            p.ay -= 6 * lock
            if (dt === 3 || dt === 5) {
                p.dr = 10 * lock
                p.counter = Math.max(p.counter, lock * 0.45)
            }
        }
        if (phase > 118 && phase < killT) {
            var push = smooth((phase - 118) / (killT - 118))
            p.bolt = 1
            p.ax += 0.04 * push
            p.ward *= (1 - push)
        }
    }

    if (dt === 4 && phase < killT) p.dscale = Math.max(p.dscale, 1.05)
    if (phase >= killT) {
        p.bolt = 1
        p.boltOn = phase < killT + 16
        p.ward = 0
        fell(p, dt, clamp01((phase - killT) / (v === 2 ? 26 : 40)), v === 2 ? "slam" : "")
    }
}

function rowCues(rows, killT, sound) {
    var list = []
    for (var i = 0; i < rows.length; i++) {
        var mid = (rows[i].a + rows[i].b) / 2
        if (mid < killT - 8) list.push({ at: mid, sound: sound })
    }
    return list
}

function cues(at, dt, variant, aside, dside) {
    var v = (variant || 0) % 3
    var list = []
    if (!at || !dt) return list
    if (at === 1 && dt === 1) {
        list = rowCues(swordRows(v), killAt(1, 1, v), "melee")
    } else if (at === 1) {
        var rows = pawnRows(v)
        var killT = killAt(1, dt, v)
        for (var i = 0; i < rows.length; i++) {
            var mid = (rows[i].a + rows[i].b) / 2
            if (mid >= killT - 8) continue
            var sound = "melee"
            if (rows[i].who === "d" && (dt === 3 || dt === 5)) sound = castSound(dt, dside || 0)
            else if (rows[i].who === "d" && dt === 2) sound = "beast"
            list.push({ at: mid, sound: sound })
        }
    } else if (at === 6) {
        var rrows = royalRows(v)
        var rkill = killAt(6, dt, v)
        for (var j = 0; j < rrows.length; j++) {
            var rm = (rrows[j].a + rrows[j].b) / 2
            if (rm >= rkill - 8) continue
            var rs = "melee"
            if (rrows[j].who === "d" && (dt === 3 || dt === 5)) rs = castSound(dt, dside || 0)
            list.push({ at: rm, sound: rs })
        }
    } else if (at === 2) {
        list.push({ at: 8, sound: "beast" })
        if (v === 1) list.push({ at: 66, sound: "melee" })
        if (v === 2) list.push({ at: 100, sound: "melee" })
        if (dt === 3 || dt === 5) list.push({ at: 24, sound: castSound(dt, dside || 0) })
    } else if (at === 4) {
        if (v === 1) {
            list.push({ at: 34, sound: "siege" })
            list.push({ at: 40, sound: "whistle" })
            list.push({ at: 114, sound: "siege" })
            list.push({ at: 120, sound: "whistle" })
        } else if (v === 2) {
            list.push({ at: 26, sound: "siege" })
            list.push({ at: 32, sound: "whistle" })
        } else {
            list.push({ at: 34, sound: "siege" })
            list.push({ at: 40, sound: "whistle" })
        }
        if (dt === 3 || dt === 5) list.push({ at: 18, sound: castSound(dt, dside || 0) })
    } else if (at === 3 || at === 5) {
        var cast = castSound(at, aside || 0)
        if (v === 1) {
            list.push({ at: 32, sound: cast })
            list.push({ at: 70, sound: cast })
            list.push({ at: 112, sound: cast })
        } else if (v === 2) {
            list.push({ at: 16, sound: cast })
            list.push({ at: 52, sound: cast })
        } else {
            list.push({ at: 18, sound: cast })
            list.push({ at: 78, sound: cast })
        }
        if ((dt === 3 || dt === 5) && (dside || 0) !== (aside || 0))
            list.push({ at: 46, sound: castSound(dt, dside || 0) })
    }
    list.sort(function (a, b) { return a.at - b.at })
    return list
}

function makePose(phase, at, dt, aside, dside, variant) {
    var p = base()
    if (!at || !dt) return p
    var v = (variant || 0) % 3
    if (at === 1 && dt === 1) swordplay(p, phase, v)
    else if (at === 1) pawnPress(p, phase, dt, v)
    else if (at === 2) lanceCharge(p, phase, dt, v)
    else if (at === 4) cannonShot(p, phase, dt, v)
    else if (at === 3 || at === 5) spell(p, phase, at, dt, v)
    else if (at === 6) royal(p, phase, dt, v)
    else pawnPress(p, phase, dt, v)

    p.tint = tintFor(at, aside || 0)
    p.fx = fxFor(at, aside || 0)
    p.dtint = tintFor(dt, dside || 0)
    p.dfx = fxFor(dt, dside || 0)
    if (at === 3 || at === 5) p.ringTint = p.tint
    else p.ringTint = p.dtint
    if (at === 3 && (aside || 0) === 1) p.wobble = 1
    else if (at === 5 && (aside || 0) === 1) p.wobble = 0.62
    else if (at === 3 || at === 5) p.wobble = 0.16

    var hit = killAt(at, dt, v)
    victory(p, at, phase, hit)

    if (phase > hit + 10) {
        var fade = Math.max(0, 1 - (phase - hit - 10) / 16)
        p.lance *= fade
        p.slash *= fade
        p.dslash *= fade
        p.muzzle *= fade
        p.hoof *= fade
        p.sparks *= fade
        p.counter *= fade
        p.aura *= Math.max(fade, phase > hit + 36 ? 0 : 0.35)
        if (phase > hit + 24) {
            p.boltOn = false
            p.cannon = 0
            p.ward *= fade
        }
    }
    if (phase >= hit && phase < hit + 14) {
        var k = 1 - (phase - hit) / 14
        p.flash = Math.max(p.flash, 0.68 * k)
        if (p.shake < 12) p.shake = 30 * k
        p.dust = Math.max(p.dust, k)
        p.sparks = Math.max(p.sparks, 0.9 * k)
        if (at === 4 || dt === 4) p.debris = Math.max(p.debris, k)
        if (at !== 3 && at !== 5) p.ay -= 10 * k
    }

    p.ax = Math.max(0.05, Math.min(0.66, p.ax))
    p.dx = Math.max(0.34, Math.min(0.96, p.dx))
    p.ascale = Math.max(0.45, p.ascale)
    p.dscale = Math.max(0.18, p.dscale)
    p.slash = clamp01(p.slash)
    p.dslash = clamp01(p.dslash)
    p.sparks = clamp01(p.sparks)
    p.debris = clamp01(p.debris)
    p.aura = clamp01(p.aura)
    p.ward = clamp01(p.ward)
    p.counter = clamp01(p.counter)
    p.lance = clamp01(p.lance)
    p.hoof = clamp01(p.hoof)
    p.dust = clamp01(p.dust)
    p.flash = clamp01(p.flash)
    p.bolt = clamp01(p.bolt)
    p.ao = clamp01(p.ao)
    p.dop = clamp01(p.dop)
    return p
}
