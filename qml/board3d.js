.pragma library

function sub(a, b) {
    return { x: a.x - b.x, y: a.y - b.y, z: a.z - b.z }
}

function mul(a, s) {
    return { x: a.x * s, y: a.y * s, z: a.z * s }
}

function dot(a, b) {
    return a.x * b.x + a.y * b.y + a.z * b.z
}

function cross(a, b) {
    return {
        x: a.y * b.z - a.z * b.y,
        y: a.z * b.x - a.x * b.z,
        z: a.x * b.y - a.y * b.x
    }
}

function norm(a) {
    var len = Math.sqrt(dot(a, a)) || 1
    return mul(a, 1 / len)
}

function camp(side) {
    if (side === 0) {
        return {
            face: "#3c97ff",
            deep: "#0b45b8",
            lip: "#f5f9ff",
            base: "#1e6eef",
            inlay: "#59a6ff",
            wash: "#553c97ff",
            glow: "#8ec2ff"
        }
    }
    return {
        face: "#9a1219",
        deep: "#4c070c",
        lip: "#ffd4d2",
        base: "#6c0c12",
        inlay: "#c21822",
        wash: "#669a1219",
        glow: "#e0454c"
    }
}

function pieceHeight(type) {
    if (type === 1) return 1.05
    if (type === 2) return 1.22
    if (type === 3) return 1.26
    if (type === 4) return 1.08
    if (type === 5) return 1.36
    if (type === 6) return 1.42
    return 1.16
}

function pieceAspect(side, type) {
    var table = [
        [0, 0.534, 0.524, 0.468, 0.503, 0.478, 0.545],
        [0, 0.638, 0.504, 0.559, 0.746, 0.603, 0.493]
    ]
    var row = table[side] || table[0]
    return row[type] || 0.54
}

function makeView(vw, vh, files, ranks, padTop, padBot) {
    if (vw < 40 || vh < 40) return null
    files = files || 8
    ranks = ranks || 8
    padTop = padTop === undefined ? 0.11 : padTop
    padBot = padBot === undefined ? 0.035 : padBot

    var cx = files * 0.5
    var lookZ = ranks * 0.45
    // Frontal and about 48 degrees up. The far rank stays close to the
    // near rank in size: the Battle Chess board, not a low corner camera.
    var elev = 48 * Math.PI / 180
    var dist = Math.max(files, ranks) * 2.55 + (ranks <= 4 ? 7.5 : 9.5)
    var eye = {
        x: cx,
        y: 0.08 + dist * Math.sin(elev),
        z: lookZ + dist * Math.cos(elev)
    }
    var look = { x: cx, y: 0.06, z: lookZ }
    var forward = norm(sub(look, eye))
    var right = norm(cross(forward, { x: 0, y: 1, z: 0 }))
    var up = norm(cross(right, forward))

    var shortBoard = ranks <= 4
    var margin = shortBoard ? 0.28 : Math.min(files, ranks) * 0.045 + 0.28
    var base = margin + (shortBoard ? 0.16 : 0.22)
    var thick = shortBoard ? 0.16 : 0.2
    var step = shortBoard ? 0.1 : 0.12
    var view = {
        eye: eye, forward: forward, right: right, up: up,
        files: files, ranks: ranks,
        margin: margin, base: base, thick: thick, step: step,
        scale: 1, ox: 0, oy: 0
    }

    var y2 = -(thick + step)
    var W = files
    var D = ranks
    var sample = [
        [-base, 0, -base], [W + base, 0, -base],
        [-margin, 0, -margin], [W + margin, 0, -margin],
        [W + margin, 0, D + margin], [-margin, 0, D + margin],
        [-base, y2, -base], [W + base, y2, -base],
        [W + base, y2, D + base], [-base, y2, D + base]
    ]
    var minX = 1e9
    var maxX = -1e9
    var minY = 1e9
    var maxY = -1e9
    for (var i = 0; i < sample.length; ++i) {
        var raw = projectRaw(view, sample[i][0], sample[i][1], sample[i][2])
        if (raw.x < minX) minX = raw.x
        if (raw.x > maxX) maxX = raw.x
        if (raw.y < minY) minY = raw.y
        if (raw.y > maxY) maxY = raw.y
    }
    var rw = Math.max(0.001, maxX - minX)
    var rh = Math.max(0.001, maxY - minY)
    var sidePx = vw * 0.035
    var topPx = vh * padTop
    var botPx = vh * padBot
    var scale = Math.min((vw - sidePx * 2) / rw, (vh - topPx - botPx) / rh)
    var extra = (vh - topPx - botPx - rh * scale) / 2
    view.scale = scale
    view.ox = (vw - rw * scale) / 2 - minX * scale
    view.oy = topPx + extra + maxY * scale
    return view
}

function projectRaw(view, wx, wy, wz) {
    var dx = wx - view.eye.x
    var dy = wy - view.eye.y
    var dz = wz - view.eye.z
    var cx = dx * view.right.x + dy * view.right.y + dz * view.right.z
    var cy = dx * view.up.x + dy * view.up.y + dz * view.up.z
    var cz = dx * view.forward.x + dy * view.forward.y + dz * view.forward.z
    if (cz < 0.05) cz = 0.05
    return { x: cx / cz, y: cy / cz }
}

function project(view, wx, wy, wz) {
    var raw = projectRaw(view, wx, wy, wz)
    return {
        x: view.ox + raw.x * view.scale,
        y: view.oy - raw.y * view.scale
    }
}

function topQuad(view, x0, x1, zFar, zNear, y) {
    y = y || 0
    return {
        tl: project(view, x0, y, zFar),
        tr: project(view, x1, y, zFar),
        br: project(view, x1, y, zNear),
        bl: project(view, x0, y, zNear)
    }
}

function worldQuad(view, a, b, c, d) {
    return {
        tl: project(view, a[0], a[1], a[2]),
        tr: project(view, b[0], b[1], b[2]),
        br: project(view, c[0], c[1], c[2]),
        bl: project(view, d[0], d[1], d[2])
    }
}

function vface(view, x, z0, z1, yTop, yBot) {
    return worldQuad(view,
        [x, yTop, z0], [x, yTop, z1], [x, yBot, z1], [x, yBot, z0])
}

function slab(view) {
    var m = view.margin
    var b = view.base
    var th = view.thick
    var st = view.step
    var W = view.files
    var D = view.ranks
    var y1 = -th
    var y2 = -(th + st)
    var lip = y1 * 0.16
    return {
        lowerTop: topQuad(view, -b, W + b, -b, D + b, y1),
        upperTop: topQuad(view, -m, W + m, -m, D + m, 0),
        lowerFront: worldQuad(view, [-b, y1, D + b], [W + b, y1, D + b], [W + b, y2, D + b], [-b, y2, D + b]),
        upperFront: worldQuad(view, [-m, 0, D + m], [W + m, 0, D + m], [W + m, y1, D + m], [-m, y1, D + m]),
        lowerBack: worldQuad(view, [-b, y1, -b], [W + b, y1, -b], [W + b, y2, -b], [-b, y2, -b]),
        upperBack: worldQuad(view, [-m, 0, -m], [W + m, 0, -m], [W + m, y1, -m], [-m, y1, -m]),
        lowerLeft: vface(view, -b, -b, D + b, y1, y2),
        upperLeft: vface(view, -m, -m, D + m, 0, y1),
        lowerRight: vface(view, W + b, -b, D + b, y1, y2),
        upperRight: vface(view, W + m, -m, D + m, 0, y1),
        frontLip: worldQuad(view, [-m, 0, D + m], [W + m, 0, D + m], [W + m, lip, D + m], [-m, lip, D + m]),
        backLip: worldQuad(view, [-m, 0, -m], [W + m, 0, -m], [W + m, lip, -m], [-m, lip, -m]),
        leftLip: vface(view, -m, -m, D + m, 0, lip),
        rightLip: vface(view, W + m, -m, D + m, 0, lip)
    }
}

function tileQuad(view, col, row) {
    var g = 0.045
    return topQuad(view, col + g, col + 1 - g, row + g, row + 1 - g, 0.03)
}

function quadMatrix(rw, rh, q) {
    var x0 = q.tl.x
    var y0 = q.tl.y
    var x1 = q.tr.x
    var y1 = q.tr.y
    var x2 = q.br.x
    var y2 = q.br.y
    var x3 = q.bl.x
    var y3 = q.bl.y
    var dx1 = x1 - x2
    var dy1 = y1 - y2
    var dx2 = x3 - x2
    var dy2 = y3 - y2
    var sx = x0 - x1 + x2 - x3
    var sy = y0 - y1 + y2 - y3
    var a, b, c, d, e, f, g, hh
    if (Math.abs(sx) < 1e-6 && Math.abs(sy) < 1e-6) {
        a = x1 - x0
        d = y1 - y0
        b = x3 - x0
        e = y3 - y0
        c = x0
        f = y0
        g = 0
        hh = 0
    } else {
        var den = dx1 * dy2 - dx2 * dy1
        if (Math.abs(den) < 1e-8) den = den < 0 ? -1e-8 : 1e-8
        g = (sx * dy2 - dx2 * sy) / den
        hh = (dx1 * sy - sx * dy1) / den
        a = x1 - x0 + g * x1
        d = y1 - y0 + g * y1
        b = x3 - x0 + hh * x3
        e = y3 - y0 + hh * y3
        c = x0
        f = y0
    }
    return [
        a / rw, b / rh, 0, c,
        d / rw, e / rh, 0, f,
        0, 0, 1, 0,
        g / rw, hh / rh, 0, 1
    ]
}

function inTri(px, py, a, b, c) {
    function s(p1x, p1y, p2x, p2y, p3x, p3y) {
        return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y)
    }
    var d1 = s(px, py, a.x, a.y, b.x, b.y)
    var d2 = s(px, py, b.x, b.y, c.x, c.y)
    var d3 = s(px, py, c.x, c.y, a.x, a.y)
    var neg = (d1 < 0) || (d2 < 0) || (d3 < 0)
    var pos = (d1 > 0) || (d2 > 0) || (d3 > 0)
    return !(neg && pos)
}

function contains(px, py, q) {
    return inTri(px, py, q.tl, q.tr, q.br) || inTri(px, py, q.tl, q.br, q.bl)
}

function hit(view, sx, sy, flipped) {
    if (!view) return -1
    for (var row = view.ranks - 1; row >= 0; --row) {
        for (var col = 0; col < view.files; ++col) {
            var q = topQuad(view, col, col + 1, row, row + 1, 0)
            if (!contains(sx, sy, q)) continue
            var file = flipped ? (view.files - 1 - col) : col
            var rank = flipped ? row : (view.ranks - 1 - row)
            return rank * view.files + file
        }
    }
    return -1
}

function shadowBox(view) {
    var b = view.base
    var y2 = -(view.thick + view.step)
    var W = view.files
    var D = view.ranks
    var pts = [
        project(view, -b, y2, -b),
        project(view, W + b, y2, -b),
        project(view, W + b, y2, D + b),
        project(view, -b, y2, D + b)
    ]
    var minX = pts[0].x
    var maxX = pts[0].x
    var minY = pts[0].y
    var maxY = pts[0].y
    for (var i = 1; i < pts.length; ++i) {
        if (pts[i].x < minX) minX = pts[i].x
        if (pts[i].x > maxX) maxX = pts[i].x
        if (pts[i].y < minY) minY = pts[i].y
        if (pts[i].y > maxY) maxY = pts[i].y
    }
    var w = (maxX - minX) * 1.06
    var h = Math.max(36, (maxY - minY) * 0.34)
    return { x: (minX + maxX) / 2 - w / 2, y: maxY - h * 0.55, w: w, h: h }
}

function visualOf(sq, flipped) {
    var file = sq % 8
    var rank = Math.floor(sq / 8)
    return {
        col: flipped ? (7 - file) : file,
        row: flipped ? rank : (7 - rank)
    }
}

function motionOf(type) {
    if (type === 1) return { hop: 0.52, steps: 2, lean: 8, dust: 0.8, trail: 0.4, weight: 0.7 }
    if (type === 2) return { hop: 1.08, steps: 1, lean: 16, dust: 0.9, trail: 0.75, weight: 0.35 }
    if (type === 3) return { hop: 0.64, steps: 1, lean: 9, dust: 0.28, trail: 0.95, weight: 0.28 }
    if (type === 4) return { hop: 0.26, steps: 1, lean: 4, dust: 1, trail: 0.18, weight: 1 }
    if (type === 5) return { hop: 0.92, steps: 1, lean: 8, dust: 0.38, trail: 1, weight: 0.22 }
    if (type === 6) return { hop: 0.46, steps: 1, lean: 5, dust: 0.48, trail: 0.28, weight: 0.78 }
    return { hop: 0.6, steps: 1, lean: 6, dust: 0.5, trail: 0.4, weight: 0.5 }
}

function piecePose(view, fromSq, toSq, t, flipped, side, type, selected, breathe) {
    var a = visualOf(fromSq, flipped)
    var b = visualOf(toSq, flipped)
    var travel = Math.max(0, Math.min(1, t))
    var moving = fromSq !== toSq
    var profile = motionOf(type)
    var breatheN = breathe || 0
    var u = 0
    var crouch = 0
    var hopWave = 0
    if (moving) {
        if (travel < 0.14) {
            crouch = Math.sin(travel / 0.14 * Math.PI)
        } else {
            var raw = (travel - 0.14) / 0.86
            u = raw * raw * (3 - 2 * raw)
            if (profile.steps > 1) {
                hopWave = Math.abs(Math.sin(Math.PI * u * 2))
                if (u > 0.5) hopWave *= 0.76
            } else {
                hopWave = Math.sin(Math.PI * u)
            }
        }
    }
    var hop = hopWave * profile.hop
    if (selected && !moving) hop += 0.2 + 0.12 * breatheN
    var col = a.col + (b.col - a.col) * (moving ? (travel < 0.14 ? 0 : u) : 0)
    var row = a.row + (b.row - a.row) * (moving ? (travel < 0.14 ? 0 : u) : 0)
    var ground = project(view, col + 0.5, 0, row + 0.66)
    var foot = project(view, col + 0.5, hop, row + 0.66)
    var left = project(view, col, hop, row + 0.66)
    var right = project(view, col + 1, hop, row + 0.66)
    var unit = Math.hypot(right.x - left.x, right.y - left.y)
    var liftScale = 1 + hop * 0.14
    var ph = unit * pieceHeight(type) * liftScale
    var pw = ph * pieceAspect(side, type)
    var footPad = 0.034
    var fileDelta = b.col - a.col
    var dir = fileDelta === 0 ? 0 : (fileDelta > 0 ? 1 : -1)
    var lean = 0
    if (moving && crouch > 0) {
        lean = (dir === 0 ? -1 : -dir) * profile.lean * 0.45 * crouch
    } else if (moving) {
        lean = dir * profile.lean * Math.sin(Math.PI * u)
        if (type === 3 || type === 5) lean += Math.sin(u * Math.PI * 2) * 5
        if (type === 2) lean += dir * 7 * (1 - u)
    } else if (selected) {
        lean = (breatheN - 0.5) * 6
    }
    var squashX = 1
    var squashY = 1
    if (crouch > 0) {
        squashX = 1 + 0.1 * crouch * profile.weight
        squashY = 1 - 0.14 * crouch * profile.weight
    } else if (moving && u > 0.9) {
        var land = Math.sin((u - 0.9) / 0.1 * Math.PI)
        squashX = 1 + 0.16 * land * (0.4 + profile.weight)
        squashY = 1 - 0.18 * land * (0.4 + profile.weight)
    } else if (selected && !moving) {
        squashX = 1 - 0.025 * breatheN
        squashY = 1 + 0.045 * breatheN
    }
    var dust = 0
    var trail = 0
    if (moving) {
        var nearGround = Math.pow(Math.max(0, 1 - hopWave * 1.25), 2)
        dust = profile.dust * Math.max(crouch * 0.45, nearGround)
        if (type === 4) dust = Math.max(dust, profile.dust * 0.55)
        if (u > 0.92) dust = Math.max(dust, profile.dust)
        trail = hopWave * profile.trail
    }
    return {
        x: foot.x - pw / 2,
        y: foot.y - ph * (1 - footPad),
        w: pw,
        h: ph,
        z: 220 + ground.y,
        footX: ground.x,
        footY: ground.y,
        hop: hop,
        lean: lean,
        shadow: 1 / (1 + hop * 1.2),
        dust: dust,
        trail: trail,
        squashX: squashX,
        squashY: squashY
    }
}

function actorStand(view, frac, hopPx, type, side, scale, depth) {
    // depth 0 stands toward the far rank, depth 1 toward the near rank.
    // Negative hopPx is a rise. It becomes world height so the figure
    // grows as it leaves the floor. Positive hopPx stays a screen drop.
    var inset = 0.65
    var x = inset + frac * (view.files - inset * 2)
    var t = depth === undefined || depth === null ? 0.5 : depth
    var zFar = Math.max(0.55, view.ranks * 0.42)
    var zNear = view.ranks - 0.7
    if (zNear < zFar + 0.45) zNear = zFar + 0.45
    var z = zFar + (zNear - zFar) * t
    var hop = hopPx || 0
    var world = Math.max(0, -hop) * 0.02
    var drop = Math.max(0, hop)
    var foot = project(view, x, world, z)
    var ground = project(view, x, 0, z)
    var left = project(view, x - 0.5, world, z)
    var right = project(view, x + 0.5, world, z)
    var unit = Math.hypot(right.x - left.x, right.y - left.y)
    var liftScale = 1 + world * 0.3
    var ph = unit * 2.25 * (scale || 1) * liftScale
    var pw = ph * pieceAspect(side || 0, type || 1)
    var gLeft = project(view, x - 0.5, 0, z)
    var gRight = project(view, x + 0.5, 0, z)
    var gUnit = Math.hypot(gRight.x - gLeft.x, gRight.y - gLeft.y)
    var shadeW = gUnit * 2.25 * (scale || 1) * pieceAspect(side || 0, type || 1)
    return {
        x: foot.x - pw / 2,
        y: foot.y - ph * 0.966 + drop,
        w: pw,
        h: ph,
        footX: ground.x,
        footY: ground.y,
        rise: world,
        shadow: Math.max(0.36, 1 - world * 0.9),
        shadeW: shadeW
    }
}

function nearSide(flipped) {
    return flipped ? 1 : 0
}

function farSide(flipped) {
    return flipped ? 0 : 1
}
