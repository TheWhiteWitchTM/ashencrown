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
            lip: "#d5dbe4",
            base: "#1e6eef",
            inlay: "#59a6ff",
            wash: "#553c97ff",
            home: "#1c486890",
            plate: "#7e8c98",
            plateDeep: "#3a4654",
            glow: "#8ec2ff"
        }
    }
    return {
        face: "#9a1219",
        deep: "#4c070c",
        lip: "#e3d0cc",
        base: "#6c0c12",
        inlay: "#c21822",
        wash: "#669a1219",
        home: "#1c684848",
        plate: "#8d7a76",
        plateDeep: "#4a3c3a",
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

function makeView(vw, vh, files, ranks, padTop, padBot, elevDeg, yawDeg, zoom) {
    if (vw < 40 || vh < 40) return null
    files = files || 8
    ranks = ranks || 8
    padTop = padTop === undefined ? 0.11 : padTop
    padBot = padBot === undefined ? 0.035 : padBot
    // Degrees up from the table. Lower is more tilt. The arena stays at 48
    // so the fighters fill the frame. The main board passes 38.
    if (elevDeg === undefined || elevDeg === null) elevDeg = 48
    if (yawDeg === undefined || yawDeg === null) yawDeg = 5
    if (zoom === undefined || zoom === null || zoom <= 0) zoom = 1
    elevDeg = Math.max(5, Math.min(85, elevDeg))

    var cx = files * 0.5
    var lookZ = ranks * 0.45
    var elev = elevDeg * Math.PI / 180
    var yaw = yawDeg * Math.PI / 180
    var dist = Math.max(files, ranks) * 2.55 + (ranks <= 4 ? 7.5 : 9.5)
    var horiz = dist * Math.cos(elev)
    var eye = {
        x: cx + horiz * Math.sin(yaw),
        y: 0.08 + dist * Math.sin(elev),
        z: lookZ + horiz * Math.cos(yaw)
    }
    var look = { x: cx, y: 0.06, z: lookZ }
    var forward = norm(sub(look, eye))
    var worldUp = { x: 0, y: 1, z: 0 }
    var right = cross(forward, worldUp)
    if (dot(right, right) < 1e-8) right = { x: 1, y: 0, z: 0 }
    right = norm(right)
    var up = norm(cross(right, forward))

    var shortBoard = ranks <= 4
    var margin = shortBoard ? 0.32 : Math.min(files, ranks) * 0.05 + 0.32
    // The full board wears a marble plate past the squares at both camps.
    var apron = shortBoard ? margin : margin + 0.34
    var base = margin + (shortBoard ? 0.2 : 0.3)
    var baseEnd = shortBoard ? base : apron + 0.28
    var thick = shortBoard ? 0.24 : 0.36
    var step = shortBoard ? 0.14 : 0.2
    var view = {
        eye: eye, forward: forward, right: right, up: up,
        files: files, ranks: ranks,
        margin: margin, apron: apron, base: base, baseEnd: baseEnd,
        thick: thick, step: step,
        elevDeg: elevDeg, yawDeg: yawDeg, zoom: zoom,
        scale: 1, ox: 0, oy: 0
    }

    var y2 = -(thick + step)
    var W = files
    var D = ranks
    var a = apron
    var be = baseEnd
    var sample = [
        [-base, 0, -be], [W + base, 0, -be],
        [W + base, 0, D + be], [-base, 0, D + be],
        [-margin, 0, -a], [W + margin, 0, -a],
        [W + margin, 0, D + a], [-margin, 0, D + a],
        [-base, y2, -be], [W + base, y2, -be],
        [W + base, y2, D + be], [-base, y2, D + be],
        [cx, 1.55, 0.4], [cx, 1.55, D - 0.4],
        [0.4, 1.55, lookZ], [W - 0.4, 1.55, lookZ]
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
    var midX = (minX + maxX) * 0.5
    var midY = (minY + maxY) * 0.5
    var cxPx = vw * 0.5
    var cyPx = topPx + extra + rh * scale * 0.5
    view.scale = scale * zoom
    view.ox = cxPx - midX * view.scale
    view.oy = cyPx + midY * view.scale
    return view
}

function camDepth(view, wx, wy, wz) {
    var dx = wx - view.eye.x
    var dy = wy - view.eye.y
    var dz = wz - view.eye.z
    return dx * view.forward.x + dy * view.forward.y + dz * view.forward.z
}

function screenUnit(view, wx, wy, wz) {
    var cz = Math.max(0.08, camDepth(view, wx, wy, wz))
    return view.scale / cz
}

function layerZ(view, wx, wy, wz, bias) {
    return 520 - camDepth(view, wx, wy, wz) * 12 + (bias || 0)
}

function quadArea(q) {
    if (!q) return 0
    function tri(a, b, c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    }
    return tri(q.tl, q.tr, q.br) + tri(q.tl, q.br, q.bl)
}

function facingQuad(q) {
    return Math.abs(quadArea(q)) > 4 ? q : null
}

function planeShown(view, px, py, pz, nx, ny, nz) {
    var vx = view.eye.x - px
    var vy = view.eye.y - py
    var vz = view.eye.z - pz
    return vx * nx + vy * ny + vz * nz > -0.12
}

function shownFace(q, view, px, py, pz, nx, ny, nz) {
    if (!planeShown(view, px, py, pz, nx, ny, nz)) return null
    return facingQuad(q)
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
    var a = view.apron !== undefined ? view.apron : m
    var b = view.base
    var be = view.baseEnd !== undefined ? view.baseEnd : b
    var th = view.thick
    var st = view.step
    var W = view.files
    var D = view.ranks
    var y1 = -th
    var y2 = -(th + st)
    var lip = y1 * 0.16
    var yLow = (y1 + y2) * 0.5
    var yMid = y1 * 0.5
    var yLip = lip * 0.5
    return {
        lowerTop: shownFace(topQuad(view, -b, W + b, -be, D + be, y1), view, W * 0.5, y1, D * 0.5, 0, 1, 0),
        upperTop: shownFace(topQuad(view, -m, W + m, -a, D + a, 0), view, W * 0.5, 0, D * 0.5, 0, 1, 0),
        lowerFront: shownFace(worldQuad(view, [-b, y1, D + be], [W + b, y1, D + be], [W + b, y2, D + be], [-b, y2, D + be]), view, W * 0.5, yLow, D + be, 0, 0, 1),
        upperFront: shownFace(worldQuad(view, [-m, 0, D + a], [W + m, 0, D + a], [W + m, y1, D + a], [-m, y1, D + a]), view, W * 0.5, yMid, D + a, 0, 0, 1),
        lowerBack: shownFace(worldQuad(view, [-b, y1, -be], [W + b, y1, -be], [W + b, y2, -be], [-b, y2, -be]), view, W * 0.5, yLow, -be, 0, 0, -1),
        upperBack: shownFace(worldQuad(view, [-m, 0, -a], [W + m, 0, -a], [W + m, y1, -a], [-m, y1, -a]), view, W * 0.5, yMid, -a, 0, 0, -1),
        lowerLeft: shownFace(vface(view, -b, -be, D + be, y1, y2), view, -b, yLow, D * 0.5, -1, 0, 0),
        upperLeft: shownFace(vface(view, -m, -a, D + a, 0, y1), view, -m, yMid, D * 0.5, -1, 0, 0),
        lowerRight: shownFace(vface(view, W + b, -be, D + be, y1, y2), view, W + b, yLow, D * 0.5, 1, 0, 0),
        upperRight: shownFace(vface(view, W + m, -a, D + a, 0, y1), view, W + m, yMid, D * 0.5, 1, 0, 0),
        frontLip: shownFace(worldQuad(view, [-m, 0, D + a], [W + m, 0, D + a], [W + m, lip, D + a], [-m, lip, D + a]), view, W * 0.5, yLip, D + a, 0, 0, 1),
        backLip: shownFace(worldQuad(view, [-m, 0, -a], [W + m, 0, -a], [W + m, lip, -a], [-m, lip, -a]), view, W * 0.5, yLip, -a, 0, 0, -1),
        leftLip: shownFace(vface(view, -m, -a, D + a, 0, lip), view, -m, yLip, D * 0.5, -1, 0, 0),
        rightLip: shownFace(vface(view, W + m, -a, D + a, 0, lip), view, W + m, yLip, D * 0.5, 1, 0, 0),
        zs: {
            lowerTop: layerZ(view, W * 0.5, y1, D * 0.5),
            upperTop: layerZ(view, W * 0.5, 0, D * 0.5, 4),
            lowerFront: layerZ(view, W * 0.5, yLow, D + be),
            upperFront: layerZ(view, W * 0.5, yMid, D + a),
            lowerBack: layerZ(view, W * 0.5, yLow, -be),
            upperBack: layerZ(view, W * 0.5, yMid, -a),
            lowerLeft: layerZ(view, -b, yLow, D * 0.5),
            upperLeft: layerZ(view, -m, yMid, D * 0.5),
            lowerRight: layerZ(view, W + b, yLow, D * 0.5),
            upperRight: layerZ(view, W + m, yMid, D * 0.5),
            frontLip: layerZ(view, W * 0.5, yLip, D + a, 2),
            backLip: layerZ(view, W * 0.5, yLip, -a, 2),
            leftLip: layerZ(view, -m, yLip, D * 0.5, 2),
            rightLip: layerZ(view, W + m, yLip, D * 0.5, 2)
        }
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
    var cells = []
    for (var row = 0; row < view.ranks; ++row) {
        for (var col = 0; col < view.files; ++col)
            cells.push({ col: col, row: row, z: layerZ(view, col + 0.5, 0, row + 0.5) })
    }
    cells.sort(function (a, b) { return b.z - a.z })
    for (var i = 0; i < cells.length; ++i) {
        var cell = cells[i]
        var q = topQuad(view, cell.col, cell.col + 1, cell.row, cell.row + 1, 0)
        if (!contains(sx, sy, q)) continue
        var file = flipped ? (view.files - 1 - cell.col) : cell.col
        var rank = flipped ? cell.row : (view.ranks - 1 - cell.row)
        return rank * view.files + file
    }
    return -1
}

function shadowBox(view) {
    var b = view.base
    var be = view.baseEnd !== undefined ? view.baseEnd : b
    var y2 = -(view.thick + view.step)
    var W = view.files
    var D = view.ranks
    var pts = [
        project(view, -b, y2, -be),
        project(view, W + b, y2, -be),
        project(view, W + b, y2, D + be),
        project(view, -b, y2, D + be)
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
    var w = (maxX - minX) * 1.14
    var h = Math.max(42, (maxY - minY) * 0.46)
    return { x: (minX + maxX) / 2 - w / 2 + w * 0.02, y: maxY - h * 0.22, w: w, h: h }
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
    var idleWave = Math.sin((breatheN + type * 0.17) * Math.PI * 2)
    var hop = hopWave * profile.hop
    if (selected && !moving) hop += 0.2 + 0.12 * breatheN
    else if (!moving) hop += 0.035 * (0.5 + 0.5 * idleWave)
    var col = a.col + (b.col - a.col) * (moving ? (travel < 0.14 ? 0 : u) : 0)
    var row = a.row + (b.row - a.row) * (moving ? (travel < 0.14 ? 0 : u) : 0)
    var ground = project(view, col + 0.5, 0, row + 0.66)
    var foot = project(view, col + 0.5, hop, row + 0.66)
    var unit = screenUnit(view, col + 0.5, hop, row + 0.66)
    var liftScale = 1 + hop * 0.14
    var ph = unit * pieceHeight(type) * liftScale
    var pw = ph * pieceAspect(side, type)
    var shade = project(view, col + 0.5 + 0.18, hop - 0.1, row + 0.66 + 0.12)
    var depthX = pw > 1 ? (shade.x - foot.x) / pw : 0.148
    var depthY = ph > 1 ? (shade.y - foot.y) / ph : 0.178
    var depthMag = Math.hypot(depthX, depthY)
    if (depthMag > 0.34) {
        depthX *= 0.34 / depthMag
        depthY *= 0.34 / depthMag
    } else if (depthMag < 0.07) {
        depthX = 0.148
        depthY = 0.178
    }
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
    } else {
        lean = idleWave * 1.6
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
    } else if (!moving) {
        squashX = 1 - 0.012 * idleWave
        squashY = 1 + 0.02 * idleWave
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
        z: layerZ(view, col + 0.5, hop, row + 0.66, 24),
        footX: ground.x,
        footY: ground.y,
        hop: hop,
        lean: lean,
        shadow: 1 / (1 + hop * 1.2),
        dust: dust,
        trail: trail,
        squashX: squashX,
        squashY: squashY,
        depthX: depthX,
        depthY: depthY
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
    var unit = screenUnit(view, x, world, z)
    var liftScale = 1 + world * 0.3
    var ph = unit * 2.25 * (scale || 1) * liftScale
    var pw = ph * pieceAspect(side || 0, type || 1)
    var gUnit = screenUnit(view, x, 0, z)
    var shadeW = gUnit * 2.25 * (scale || 1) * pieceAspect(side || 0, type || 1)
    return {
        x: foot.x - pw / 2,
        y: foot.y - ph * 0.966 + drop,
        w: pw,
        h: ph,
        z: layerZ(view, x, world, z, 24),
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
