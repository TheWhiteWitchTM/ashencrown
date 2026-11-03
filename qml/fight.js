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

function strikeAt(at, dt) {
    if (at === 1 && dt === 1) return 150
    if (at === 1) return 136
    if (at === 2) return 124
    if (at === 4) return 132
    if (at === 3 || at === 5) return 146
    if (at === 6) return 148
    return 140
}

function deathAt(at, dt) {
    return strikeAt(at, dt)
}

function deathSound(dt) {
    if (dt === 2) return "fall_knight"
    if (dt === 3) return "fall_bishop"
    if (dt === 4) return "fall_rook"
    if (dt === 5) return "fall_queen"
    if (dt === 6) return "fall_king"
    return "fall_pawn"
}

function tintFor(at, aside) {
    if (at === 3) return aside === 0 ? "#d7e6ff" : "#e07080"
    if (at === 5) return aside === 0 ? "#8ec2ff" : "#ff4a3a"
    if (at === 4) return aside === 0 ? "#d5dde8" : "#c44a42"
    if (at === 2) return "#eef3fb"
    return "#f4f6fb"
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
        shake: 0,
        boltOn: false, bolt: 0,
        slash: 0, dslash: 0, lance: 0, ring: 0, dust: 0, flash: 0,
        cannon: 0, muzzle: 0,
        tint: "#f4f6fb",
        ringTint: "#f4f6fb",
        fx: "qrc:/assets/fx/slash.png"
    }
}

function moveLine(at, dt) {
    var lines = {
        "1x1": "Steel on steel, three times. A parry, a riposte, then the thrust that ends it.",
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
    if (type === 2) return Math.sin(phase * 0.95) * 14
    if (type === 4) return Math.abs(Math.sin(phase * 0.42)) * -8
    if (type === 5) return -16 + Math.sin(phase * 0.28) * 6
    if (type === 3) return -8 + Math.sin(phase * 0.32) * 4
    if (type === 6) return Math.sin(phase * 0.4) * 3
    return Math.sin(phase * 0.55) * 4
}

function kill(p, dt, t) {
    var e = smooth(t)
    if (dt === 1) {
        p.dr = 78 * e
        p.dx += 0.08 * e
        p.dy = 22 * e
    } else if (dt === 2) {
        p.dr = 130 * e
        p.dx += 0.16 * e
        p.dy = -40 * Math.sin(e * Math.PI)
    } else if (dt === 3) {
        p.dop = 1 - e
        p.dy = -32 * e
        p.dr = -18 * e
    } else if (dt === 4) {
        p.dscale = 1 - 0.46 * e
        p.dy = 36 * e
        p.dust = Math.max(p.dust, 0.9)
        p.dr = 6 * e
    } else if (dt === 5) {
        p.dr = -40 * e
        p.dop = 1 - e
        p.dy -= 20 * e
    } else {
        if (e < 0.42) {
            p.dy = 26 * (e / 0.42)
            p.dr = 16
        } else {
            var u = (e - 0.42) / 0.58
            p.dy = 26
            p.dr = 16 + 74 * u
            p.dx += 0.05 * u
        }
    }
}

function victory(p, at, phase) {
    if (phase < 160) return
    if (at === 2) p.ay -= 12
    else if (at === 3 || at === 5) p.ay -= 10
    else if (at === 4) p.ascale = Math.max(p.ascale, 1.04)
    else if (at === 6) p.ar = Math.min(p.ar, -6)
}

function swing(phase, a, b) {
    if (phase < a || phase > b) return 0
    return Math.sin((phase - a) / (b - a) * Math.PI)
}

function swordplay(p, phase) {
    var e = smooth(clamp01(phase / 26))
    p.ax = lerp(0.14, 0.32, e)
    p.dx = lerp(0.86, 0.68, e)
    p.ay = Math.sin(phase * 0.55) * 3
    p.dy = Math.sin(phase * 0.55 + 1) * 3

    var high = swing(phase, 30, 48)
    var parry = swing(phase, 52, 70)
    var low = swing(phase, 78, 98)
    var bind = swing(phase, 104, 128)
    p.ar = -24 * high + 8 * parry - 16 * low - 8 * bind
    p.dr = 16 * high - 20 * parry + 10 * low + 12 * bind
    p.slash = Math.max(high, low * 0.9)
    p.dslash = Math.max(parry, bind * 0.7)
    p.ax += 0.03 * high - 0.025 * parry + 0.02 * low
    p.dx -= 0.02 * parry
    if (low > 0) p.dy = -8 * low
    if (bind > 0) {
        p.ax = lerp(p.ax, 0.36, bind)
        p.dx = lerp(p.dx, 0.64, bind)
    }

    if (phase > 136 && phase < 156) {
        var lunge = smooth((phase - 136) / 16)
        p.ax = lerp(0.34, 0.40, lunge)
        p.ar = -22
        p.slash = 1
        p.dslash = 0
    }
    if (phase > 150) {
        var t = smooth(clamp01((phase - 150) / 36))
        p.ax = 0.40
        p.ar = -18
        kill(p, 1, t)
        if (phase < 168) p.slash = 1
    }
}

function pawnPress(p, phase, dt) {
    var e = smooth(clamp01(phase / 28))
    p.ax = lerp(0.14, 0.34, e)
    p.dx = lerp(0.86, dt === 4 ? 0.74 : 0.66, e * (dt === 2 ? 0.85 : 0.55))
    p.ay = Math.sin(phase * 0.6) * 4
    if (phase < 120) p.dy = bob(dt, phase)

    var cut = swing(phase, 32, 52)
    var answer = swing(phase, 58, 78)
    var second = swing(phase, 86, 112)
    p.ar = -20 * cut - 18 * second + 6 * answer
    p.dr = 12 * cut - 14 * answer
    p.slash = Math.max(cut, second)
    p.dslash = answer
    p.ax += 0.03 * second
    if (dt === 3 || dt === 5) p.ring = Math.max(p.ring, answer * 0.6)
    if (dt === 4) p.dscale = 1 + 0.05 * e

    if (phase > 136) kill(p, dt, clamp01((phase - 136) / 40))
}

function lanceCharge(p, phase, dt) {
    var wind = smooth(clamp01(phase / 28))
    p.lance = lerp(0.35, 0.7, wind)
    p.ar = -8 * wind
    p.ax = lerp(0.06, 0.12, wind)
    p.ay = Math.sin(phase * 0.8) * 6 * wind
    if (dt === 2) {
        p.dx = lerp(0.94, 0.78, wind)
        p.dr = 8 * wind
        p.dy = Math.sin(phase * 0.8 + 0.4) * 6 * wind
    } else {
        p.dx = dt === 4 ? 0.74 : 0.70
        p.dr = -6 * wind
        if (phase < 120) p.dy = bob(dt, phase)
        if (dt === 4) p.dscale = 1.06
    }

    if (phase > 32 && phase < 124) {
        var t = smooth((phase - 32) / 86)
        var accel = t * t
        var meet = dt === 2 ? 0.36 : 0.40
        p.ax = lerp(0.12, meet, accel)
        p.ay = Math.sin(phase * 1.15) * (10 + 10 * t)
        p.lance = lerp(0.72, 1, t)
        p.ar = -7
        if (dt === 2) {
            p.dx = lerp(0.88, 0.62, accel)
            p.dy = Math.sin(phase * 1.15 + 0.5) * (10 + 8 * t)
            p.dr = 8
        }
        p.dust = t * 0.35
    }

    if (phase > 112 && phase < 132) {
        var hit = Math.sin((phase - 112) / 20 * Math.PI)
        p.shake = 14 * hit
        p.slash = hit
        p.lance = 1
        p.dust = Math.max(p.dust, hit)
    }
    if (phase > 124) {
        p.ax = dt === 2 ? 0.36 : 0.40
        p.ay = -8
        p.ar = -6
        kill(p, dt, clamp01((phase - 124) / 48))
    }
}

function cannonShot(p, phase, dt) {
    var plant = smooth(clamp01(phase / 36))
    p.ax = lerp(0.16, 0.36, plant)
    p.ay = Math.abs(Math.sin(phase * 0.35)) * -5
    p.ascale = 1 + 0.04 * plant
    p.dx = dt === 2 ? lerp(0.88, 0.68, plant * 0.4) : (dt === 5 ? 0.74 : 0.70)
    if (phase < 128) p.dy = bob(dt, phase)
    if (dt === 2) p.dy = Math.sin(phase * 0.9) * 8
    if (dt === 4) p.dscale = 1.05

    if (phase > 48 && phase < 74) {
        var aim = smooth((phase - 48) / 26)
        p.ar = -3 * aim
        p.dr = dt === 6 ? 8 * aim : -4 * aim
    }
    if (phase > 74 && phase < 96) {
        var kick = Math.sin((phase - 74) / 22 * Math.PI)
        p.ax = lerp(0.36, 0.26, kick)
        p.ar = -6 * kick
        p.muzzle = kick
        p.shake = 7 * kick
        p.dust = 0.4 * kick
    }
    if (phase >= 78) p.cannon = clamp01((phase - 78) / 54)
    if (dt === 3 || dt === 5) p.ring = clamp01((phase - 40) / 30) * (phase < 132 ? 0.7 : 0)

    if (phase > 132) {
        p.ax = 0.32
        p.muzzle = 0
        p.cannon = 0
        p.shake = Math.max(p.shake, 8 * (1 - clamp01((phase - 132) / 20)))
        kill(p, dt, clamp01((phase - 132) / 44))
    }
}

function spell(p, phase, at, dt) {
    var rise = smooth(clamp01(phase / 28))
    p.ax = 0.22
    p.dx = dt === 4 ? 0.72 : 0.76
    p.ay = lerp(0, at === 5 ? -18 : -12, rise) + Math.sin(phase * 0.28) * 4
    p.dy = bob(dt, phase)
    if (dt === 3 || dt === 5) p.dy = lerp(0, -14, rise) + Math.sin(phase * 0.28 + 1) * 4
    p.ring = rise * 0.45

    if (phase > 36) {
        p.boltOn = true
        p.bolt = smooth(clamp01((phase - 36) / 40))
        p.ring = Math.max(p.ring, p.bolt * 0.8)
    }
    if (phase > 78 && phase < 120) {
        var lock = Math.sin((phase - 78) / 42 * Math.PI)
        p.shake = 6 * lock
        p.bolt = 0.72 + 0.28 * lock
        if (dt === 3 || dt === 5 || dt === 6) {
            p.dr = 8 * lock
            p.ring = Math.max(p.ring, lock)
        }
        p.ay -= 4 * lock
    }
    if (phase > 120 && phase < 150) {
        var push = smooth((phase - 120) / 24)
        p.bolt = 1
        p.ax += 0.03 * push
        p.shake = 4 * (1 - push)
    }
    if (phase > 146) {
        p.bolt = 1
        kill(p, dt, clamp01((phase - 146) / 40))
    }
}

function royal(p, phase, dt) {
    var e = smooth(clamp01(phase / 30))
    p.ax = lerp(0.16, 0.34, e)
    p.dx = lerp(0.86, dt === 4 ? 0.70 : 0.64, e * 0.7)
    p.ay = Math.sin(phase * 0.4) * 3
    if (phase < 140) p.dy = bob(dt, phase)
    if (dt === 6) p.dr = 8 * e

    var cut = swing(phase, 34, 54)
    var answer = swing(phase, 60, 82)
    var slip = swing(phase, 88, 114)
    p.ar = -18 * cut + 4 * answer
    p.dr = 12 * answer - 8 * cut
    p.slash = cut
    p.dslash = answer * 0.85
    if (slip > 0) {
        p.ax = lerp(0.34, 0.20, slip)
        p.ay = 6 * slip
    }

    if (phase > 118 && phase < 148) {
        var end = smooth((phase - 118) / 22)
        p.ax = lerp(0.24, 0.40, end)
        p.ar = -18
        p.slash = end
        p.dslash = 0
    }
    if (phase > 148) {
        p.ax = 0.40
        p.ar = -14
        kill(p, dt, clamp01((phase - 148) / 38))
    }
}

function makePose(phase, at, dt, aside, dside) {
    var p = base()
    if (!at || !dt) return p
    if (at === 1 && dt === 1) swordplay(p, phase)
    else if (at === 1) pawnPress(p, phase, dt)
    else if (at === 2) lanceCharge(p, phase, dt)
    else if (at === 4) cannonShot(p, phase, dt)
    else if (at === 3 || at === 5) spell(p, phase, at, dt)
    else if (at === 6) royal(p, phase, dt)
    else pawnPress(p, phase, dt)

    p.tint = tintFor(at, aside || 0)
    p.fx = fxFor(at, aside || 0)
    if (at === 3 || at === 5) p.ringTint = p.tint
    else p.ringTint = tintFor(dt, dside || 0)
    victory(p, at, phase)

    var hit = strikeAt(at, dt)
    if (phase > hit + 12) {
        var fade = Math.max(0, 1 - (phase - hit - 12) / 16)
        p.lance *= fade
        p.slash *= fade
        p.dslash *= fade
        p.muzzle *= fade
        if (phase > hit + 28) {
            p.boltOn = false
            p.cannon = 0
        }
    }
    if (phase >= hit && phase < hit + 12) {
        var k = 1 - (phase - hit) / 12
        p.flash = 0.36 * k
        if (p.shake < 6) p.shake = 14 * k
        p.dust = Math.max(p.dust, 0.8 * k)
    }
    return p
}
