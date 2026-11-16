#!/usr/bin/env python3
"""Original sound effects and a looping drone for Ashen Crown. No samples borrowed."""

import math
import os
import random
import struct
import wave

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "sounds")


def write_wav(name, samples, volume=0.9):
    path = os.path.join(OUT, name)
    peak = max(1e-6, max(abs(s) for s in samples))
    scale = volume / peak
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = int(max(-1, min(1, s * scale)) * 32767)
            frames += struct.pack("<h", v)
        w.writeframes(frames)
    print(name, len(samples))


def env(n, a=0.01, d=0.08, s=0.4, r=0.1):
    out = []
    for i in range(n):
        t = i / SR
        if t < a:
            out.append(t / a if a else 1)
        elif t < a + d:
            out.append(1 - (1 - s) * ((t - a) / d if d else 1))
        elif t < n / SR - r:
            out.append(s)
        else:
            tail = (n / SR - t) / r if r else 0
            out.append(max(0, s * tail))
    return out


def tone(freq, dur, harm=((1, 1), (2, 0.35), (3, 0.12)), decay=3.5):
    n = int(SR * dur)
    out = []
    for i in range(n):
        t = i / SR
        e = math.exp(-decay * t)
        s = 0
        for h, a in harm:
            s += a * math.sin(2 * math.pi * freq * h * t)
        out.append(s * e)
    return out


def noise(dur, seed=1):
    rng = random.Random(seed)
    n = int(SR * dur)
    return [rng.uniform(-1, 1) for _ in range(n)]


def lowpass(samples, k=0.15):
    y = 0
    out = []
    for s in samples:
        y += k * (s - y)
        out.append(y)
    return out


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for t in tracks:
        for i, s in enumerate(t):
            out[i] += s
    return out


def silence(dur):
    return [0.0] * int(SR * dur)


def pad(samples, dur):
    n = int(SR * dur)
    if len(samples) >= n:
        return samples[:n]
    return samples + [0.0] * (n - len(samples))


def click():
    n = noise(0.045, 3)
    e = env(len(n), 0.001, 0.02, 0.2, 0.02)
    return [a * b for a, b in zip(n, e)]


def move():
    # Two steps and a settle, long enough to follow the piece across the square.
    step_a = tone(168, 0.16, ((1, 1), (2, 0.2)), decay=8)
    n = lowpass(noise(0.09, 4), 0.4)
    e = env(len(n), 0.001, 0.04, 0.0, 0.02)
    thud = [a * b * 0.8 for a, b in zip(n, e)]
    step_b = silence(0.26) + tone(142, 0.18, ((1, 1), (2, 0.22)), decay=9)
    n2 = lowpass(noise(0.1, 5), 0.35)
    e2 = env(len(n2), 0.001, 0.05, 0.0, 0.03)
    land = silence(0.5) + mix(
        tone(96, 0.22, ((1, 1), (2, 0.3)), decay=10),
        [a * b * 0.7 for a, b in zip(n2, e2)],
    )
    return mix(step_a, thud, step_b, land)


def clang(base, dur=0.32, seed=1):
    # A sword is inharmonic and short. The whoosh leads the metal by a breath.
    n = int(SR * dur)
    partials = (1.0, 2.27, 3.52, 4.9, 6.41, 8.2)
    amps = (1.0, 0.52, 0.3, 0.16, 0.09, 0.05)
    out = [0.0] * n
    lead = int(SR * 0.016)
    for i in range(n - lead):
        t = i / SR
        s = 0.0
        for h, a in zip(partials, amps):
            s += a * math.sin(2 * math.pi * base * h * t) * math.exp(-(6.5 + h * 2.4) * t)
        out[i + lead] += s
    whoosh = highpass(noise(0.05, seed), 0.82)
    we = env(len(whoosh), 0.003, 0.018, 0.04, 0.012)
    for i, s in enumerate(whoosh):
        if i < n:
            out[i] += s * we[i] * 0.65
    crack = _crack(0.018, seed)
    for i, s in enumerate(crack):
        j = i + lead
        if j < n:
            out[j] += s * 0.4
    return out


def melee():
    return mix(clang(980, 0.32, 7), [s * 0.32 for s in clang(1540, 0.2, 8)])


def magic():
    dur = 0.68
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        f = 220 + 1100 * (t ** 0.65)
        phase += f / SR
        e = math.sin(math.pi * min(1, t / dur)) ** 0.75
        s = math.sin(2 * math.pi * phase)
        s += 0.4 * math.sin(2 * math.pi * phase * 2.01)
        s += 0.18 * math.sin(2 * math.pi * phase * 3.47)
        out.append(s * e)
    spark = highpass(noise(dur, 24), 0.96)
    se = [math.sin(math.pi * min(1, i / max(1, len(spark)))) ** 2 for i in range(len(spark))]
    return mix(out, [a * b * 0.28 for a, b in zip(spark, se)], [s * 0.45 for s in _crack(0.025, 25)])


def holy():
    notes = [523.25, 659.25, 783.99, 1046.5]
    tracks = []
    for i, f in enumerate(notes):
        note = tone(f, 0.82, ((1, 1), (2, 0.2), (3, 0.07), (4.02, 0.035)), decay=2.3)
        tracks.append(silence(i * 0.04) + note)
    n = int(SR * 0.75)
    shimmer = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        phase += (980 + 520 * t) / SR
        e = math.sin(math.pi * min(1, t / 0.75)) ** 1.1
        shimmer.append(0.22 * math.sin(2 * math.pi * phase) * e)
    return mix(*tracks, shimmer, _crack(0.02, 26))


def beast():
    n = int(SR * 0.72)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        envp = math.sin(math.pi * min(1.0, t / 0.64)) ** 0.5
        f = 360 + 520 * (math.sin(math.pi * min(1.0, t / 0.52)) ** 1.05)
        f *= 1.0 + 0.035 * math.sin(2 * math.pi * 13 * t)
        phase += f / SR
        trem = 0.3 + 0.7 * (0.5 + 0.5 * math.sin(2 * math.pi * (15 + 8 * t) * t))
        s = math.sin(2 * math.pi * phase)
        s += 0.5 * math.sin(2 * math.pi * phase * 2)
        s += 0.18 * math.sin(2 * math.pi * phase * 3.02)
        out.append(s * trem * envp)
    snort = highpass(noise(0.12, 9), 0.72)
    se = env(len(snort), 0.004, 0.04, 0.08, 0.04)
    return mix(out, silence(0.5) + [a * b * 0.5 for a, b in zip(snort, se)])


def siege():
    boom = _kick(46, 0.62, 190)
    sub = _kick(34, 0.48, 70)
    air_n = lowpass(noise(0.32, 11), 0.14)
    air_e = env(len(air_n), 0.001, 0.03, 0.15, 0.18)
    snap = highpass(noise(0.07, 22), 0.94)
    snap_e = env(len(snap), 0.0008, 0.012, 0.04, 0.03)
    return mix(
        [s * 1.15 for s in boom],
        [s * 0.4 for s in sub],
        _crack(0.03, 21),
        [a * b * 0.45 for a, b in zip(air_n, air_e)],
        [a * b * 0.7 for a, b in zip(snap, snap_e)],
    )


def whistle():
    # The shell in the air. A short fall, not a siren: it swells and is gone.
    dur = 1.05
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        f = 1500 * math.exp(-1.35 * t) + 420
        phase += f / SR
        trem = 0.72 + 0.28 * math.sin(2 * math.pi * 23 * t)
        e = math.sin(math.pi * min(1.0, t / dur)) ** 0.55
        s = math.sin(2 * math.pi * phase)
        s += 0.18 * math.sin(2 * math.pi * phase * 2.01)
        out.append(s * trem * e)
    breath = highpass(noise(dur, 23), 0.97)
    return [a * 0.88 + b * 0.07 for a, b in zip(out, breath)]


def check():
    a = tone(196, 0.28, ((1, 1), (2, 0.3), (3, 0.15)), decay=3)
    b = silence(0.16) + tone(262, 0.4, ((1, 1), (2, 0.25)), decay=2.5)
    return mix(a, b)


def fanfare(up=True):
    seq = [261.6, 329.6, 392.0, 523.3] if up else [392.0, 349.2, 311.1, 233.1]
    out = []
    for i, f in enumerate(seq):
        out += tone(f, 0.22 if up else 0.28, ((1, 1), (2, 0.4), (3, 0.18), (4, 0.08)), decay=4)
    return out


def hit():
    # The killing blow: crack, body, and a short ring. Not a voice.
    return mix(
        [s * 0.85 for s in _crack(0.04, 13)],
        [s * 0.9 for s in _kick(68, 0.32, 170)],
        [s * 0.5 for s in clang(860, 0.36, 4)],
    )


def _swell(n, attack, hold):
    out = []
    for i in range(n):
        t = i / SR
        if t < attack:
            out.append((t / attack) ** 2 if attack else 1)
        else:
            out.append(math.exp(-(t - attack) / hold))
    return out


def sting():
    # A low chord under the death. It arrives late and stays quiet.
    dur = 2.4
    n = int(SR * dur)
    notes = (73.42, 110.0, 146.83, 220.0)
    amps = (0.5, 0.32, 0.2, 0.08)
    air = lowpass(noise(dur, 17), 0.015)
    envl = _swell(n, 0.45, 1.35)
    out = []
    for i in range(n):
        t = i / SR
        s = air[i] * 0.08
        for f, a in zip(notes, amps):
            s += a * math.sin(2 * math.pi * f * t)
            s += a * 0.12 * math.sin(2 * math.pi * f * 2.003 * t)
        out.append(s * envl[i] * 0.42)
    return silence(0.12) + out


def highpass(samples, alpha=0.97):
    y = 0.0
    prev = 0.0
    out = []
    for s in samples:
        y = alpha * (y + s - prev)
        prev = s
        out.append(y)
    return out


def _crack(dur, seed):
    # Bright and short. A long lowpassed noise is the raspberry we are avoiding.
    n = highpass(noise(dur, seed), 0.97)
    e = env(len(n), 0.0004, 0.008, 0.02, 0.02)
    return [a * b * 3.2 for a, b in zip(n, e)]


def _cymbal(dur, seed, decay=7):
    n = highpass(noise(dur, seed), 0.988)
    out = []
    for i, s in enumerate(n):
        out.append(s * math.exp(-decay * i / SR) * 1.6)
    return out


def _kick(freq, dur=0.5, drop=140):
    # A sine that falls in the first few dozen milliseconds, then rings as a drum.
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        slide = max(0.0, 1.0 - t / 0.045)
        f = freq + drop * slide * slide
        phase += f / SR
        e = math.exp(-7.2 * t)
        s = math.sin(2 * math.pi * phase)
        s += 0.28 * math.sin(2 * math.pi * phase * 2) * math.exp(-16 * t)
        out.append(s * e)
    return out


def _chord(freqs, dur, decay, harm=((1, 1), (2, 0.22), (3, 0.08))):
    return mix(*[tone(f, dur, harm, decay) for f in freqs])


def _resonator(samples, freq, bw):
    r = math.exp(-math.pi * bw / SR)
    theta = 2 * math.pi * freq / SR
    a1 = -2.0 * r * math.cos(theta)
    a2 = r * r
    y1 = 0.0
    y2 = 0.0
    out = []
    for x in samples:
        y = x - a1 * y1 - a2 * y2
        y2 = y1
        y1 = y
        out.append(y)
    return out


def _glottal(dur, f0_fn, seed):
    n = int(SR * dur)
    rng = random.Random(seed)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / SR
        f0 = f0_fn(t)
        f0 *= 1.0 + 0.012 * math.sin(2 * math.pi * 5.2 * t)
        f0 += (rng.random() - 0.5) * 1.4
        f0 = max(75.0, min(480.0, f0))
        phase += f0 / SR
        if phase >= 1.0:
            phase -= 1.0
        if phase < 0.40:
            g = 0.5 * (1.0 - math.cos(math.pi * phase / 0.40))
        elif phase < 0.52:
            g = max(0.0, math.cos(0.5 * math.pi * (phase - 0.40) / 0.12))
        else:
            g = 0.0
        out[i] = g
    return out


_SHAPES = {
    "ah": ((730, 90, 1.0), (1120, 110, 0.42), (2450, 160, 0.16)),
    "aw": ((600, 80, 1.0), (1040, 100, 0.38), (2300, 150, 0.12)),
    "oh": ((490, 70, 1.0), (860, 90, 0.36), (2300, 150, 0.1)),
    "uh": ((440, 80, 1.0), (1080, 110, 0.3), (2200, 160, 0.08)),
    "eh": ((560, 80, 0.65), (1760, 120, 0.5), (2500, 150, 0.14)),
}


def _unit(samples, peak=1.0):
    m = max(1e-8, max(abs(s) for s in samples))
    return [s * peak / m for s in samples]


def yell(dur, f0_fn, shape, seed, attack=0.016, decay=0.24):
    # A short voiced cry. Pitch may fall inside the shout. It does not siren.
    excit = _glottal(dur, f0_fn, seed)
    breath = highpass(noise(dur, seed + 19), 0.9)
    excit = [a + b * 0.05 for a, b in zip(excit, breath)]
    parts = []
    for freq, bw, amp in _SHAPES[shape]:
        parts.append([s * amp for s in _resonator(excit, freq, bw)])
    voice = lowpass(mix(*parts), 0.38)
    out = []
    for i, s in enumerate(voice):
        t = i / SR
        if t < attack:
            e = (t / attack) ** 0.5 if attack else 1
        else:
            e = math.exp(-(t - attack) / decay)
        out.append(math.tanh(s * e * 2.2))
    return _unit(out, 1.0)


def fall_pawn():
    def f0(t):
        if t < 0.045:
            return 165 + 190 * (t / 0.045)
        return 130 + 210 * math.exp(-3.4 * (t - 0.045))

    def f0b(t):
        return 115 + 55 * math.exp(-4.5 * t)

    voice = yell(0.7, f0, "ah", 3, attack=0.01, decay=0.2)
    grunt = silence(0.24) + yell(0.32, f0b, "uh", 5, attack=0.008, decay=0.12)
    return mix(
        voice,
        [s * 0.5 for s in grunt],
        [s * 0.62 for s in clang(1180, 0.26, 31)],
        silence(0.3) + _kick(82, 0.3, 150),
        silence(0.32) + [s * 0.28 for s in clang(1720, 0.14, 32)],
    )


def fall_knight():
    def f0(t):
        if t < 0.04:
            return 190 + 90 * (t / 0.04)
        return 145 + 130 * math.exp(-2.8 * (t - 0.04))

    voice = silence(0.16) + yell(0.58, f0, "ah", 7, attack=0.012, decay=0.18)
    return mix(
        [s * 0.85 for s in beast()],
        [s * 0.8 for s in voice],
        _kick(62, 0.38, 150),
        [s * 0.4 for s in clang(640, 0.4, 33)],
    )


def fall_bishop():
    def f0(t):
        return 205 + 28 * math.sin(2 * math.pi * 3.2 * t)

    voice = yell(0.85, f0, "oh", 8, attack=0.035, decay=0.32)
    bells = []
    for i, f in enumerate((1568.0, 1174.0, 880.0, 659.25)):
        note = tone(f, 1.05, ((1, 1), (2.4, 0.14), (3.8, 0.04)), decay=2.1)
        bells.append(silence(0.1 + i * 0.06) + note)
    spark = highpass(noise(0.7, 40), 0.985)
    se = _swell(len(spark), 0.08, 0.4)
    return mix(
        [s * 0.7 for s in voice],
        *[[s * 0.32 for s in b] for b in bells],
        [a * b * 0.22 for a, b in zip(spark, se)],
    )


def fall_rook():
    def f0(t):
        return 108 + 36 * math.exp(-3.2 * t)

    bark = yell(0.38, f0, "aw", 9, attack=0.008, decay=0.11)
    rubble = []
    for i, f in enumerate((190, 142, 220, 96, 68)):
        rubble.append(silence(0.06 + i * 0.055) + tone(f, 0.18, ((1, 1), (2.3, 0.35), (3.6, 0.12)), decay=16))
    cracks = [silence(i * 0.06) + _crack(0.03, 40 + i) for i in range(5)]
    return mix(
        [s * 0.9 for s in bark],
        [s * 0.85 for s in _unit(_kick(40, 0.85, 130))],
        *[ [s * 0.45 for s in c] for c in cracks ],
        *[[s * 0.5 for s in r] for r in rubble],
    )


def fall_queen():
    def f0(t):
        if t < 0.07:
            return 230 + 150 * (t / 0.07)
        return 210 + 160 * math.exp(-1.7 * (t - 0.07))

    def f0b(t):
        return 300 + 36 * math.sin(2 * math.pi * 5.5 * t)

    voice = yell(0.9, f0, "eh", 11, attack=0.018, decay=0.3)
    high = silence(0.2) + yell(0.62, f0b, "ah", 12, attack=0.025, decay=0.26)
    shimmer = tone(1680, 0.85, ((1, 1), (2.01, 0.28)), decay=3.4)
    low = _chord((196.0, 246.94, 293.66), 1.15, 1.5)
    return mix(voice, [s * 0.5 for s in high], [s * 0.26 for s in shimmer], [s * 0.32 for s in low])


def fall_king():
    def f0(t):
        if t < 0.5:
            return 122 + 7 * math.sin(2 * math.pi * 4.2 * t)
        return 122 - 36 * min(1.0, (t - 0.5) / 0.22)

    voice = silence(0.06) + yell(0.78, f0, "oh", 13, attack=0.028, decay=0.34)
    low = _chord((110.0, 164.81, 220.0), 1.45, 1.35)
    sw = _swell(len(low), 0.06, 1.05)
    low = [a * b for a, b in zip(low, sw)]
    brass = _chord((196.0, 246.94, 293.66, 392.0), 1.05, 1.7)
    return mix(
        voice,
        [s * 0.28 for s in _unit(low)],
        [s * 0.26 for s in _unit(brass)],
        silence(0.42) + [s * 0.22 for s in _unit(clang(1980, 0.2, 14))],
        silence(0.48) + [s * 0.4 for s in _unit(_kick(56, 0.42, 100))],
        [s * 0.18 for s in _unit(_cymbal(0.55, 56, 6))],
    )


def drums():
    hits = []
    for i, amp in enumerate((0.55, 0.7, 1.0)):
        h = tone(70, 0.18, ((1, 1), (2, 0.3)), decay=14)
        n = noise(0.08, 20 + i)
        e = env(len(n), 0.001, 0.02, 0, 0.02)
        one = mix([s * amp for s in h], [a * b * amp for a, b in zip(n, e)])
        hits.append(silence(i * 0.16) + one)
    return mix(*hits)


def notify():
    return mix(tone(880, 0.12, ((1, 1), (3, 0.2)), decay=8), silence(0.09) + tone(1174, 0.16, ((1, 1),), decay=7))


def music():
    seconds = 16
    n = SR * seconds
    # Integer cycles so the loop point is seamless.
    def sine_cycles(cycles, amp, harm=0.0):
        freq = cycles / seconds
        out = []
        for i in range(n):
            t = i / SR
            s = math.sin(2 * math.pi * freq * t)
            if harm:
                s += harm * math.sin(2 * math.pi * freq * 2 * t)
            out.append(s * amp)
        return out

    d = sine_cycles(1175, 0.55, 0.18)   # ~73.4 Hz, D2
    a = sine_cycles(1760, 0.28, 0.1)    # 110 Hz, A2
    fifth = sine_cycles(2349, 0.12, 0.0)
    lfo_cycles = 2  # returns to the same phase
    out = []
    rng = random.Random(1)
    # Looping noise: generate one buffer and it already matches only if we crossfade.
    wind = [rng.uniform(-1, 1) for _ in range(n)]
    wind = lowpass(wind, 0.02)
    fade = int(SR * 0.4)
    for i in range(fade):
        k = i / fade
        wind[i] = wind[i] * k + wind[n - fade + i] * (1 - k)
        wind[n - fade + i] = wind[i]
    for i in range(n):
        t = i / SR
        lfo = 0.82 + 0.18 * math.sin(2 * math.pi * lfo_cycles * t / seconds)
        pulse = 0.85 + 0.15 * math.sin(2 * math.pi * 8 * t / seconds)  # 8 beats, seamless
        drum = 0
        beat = (i % (n // 8))
        if beat < int(SR * 0.12):
            drum = math.sin(2 * math.pi * 48 * beat / SR) * math.exp(-beat / SR * 10) * 0.35
        out.append((d[i] + a[i] + fifth[i]) * lfo * 0.55 + wind[i] * 0.08 * pulse + drum)
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    write_wav("click.wav", click(), 0.7)
    write_wav("move.wav", move(), 0.8)
    write_wav("melee.wav", melee(), 0.85)
    write_wav("magic.wav", magic(), 0.8)
    write_wav("holy.wav", holy(), 0.75)
    write_wav("beast.wav", beast(), 0.85)
    write_wav("siege.wav", siege(), 0.9)
    write_wav("whistle.wav", whistle(), 0.72)
    write_wav("check.wav", check(), 0.8)
    write_wav("win.wav", fanfare(True), 0.8)
    write_wav("lose.wav", fanfare(False), 0.75)
    write_wav("hit.wav", hit(), 0.9)
    write_wav("drums.wav", drums(), 0.9)
    write_wav("notify.wav", notify(), 0.6)
    write_wav("sting.wav", sting(), 0.55)
    write_wav("fall_pawn.wav", fall_pawn(), 0.88)
    write_wav("fall_knight.wav", fall_knight(), 0.88)
    write_wav("fall_bishop.wav", fall_bishop(), 0.84)
    write_wav("fall_rook.wav", fall_rook(), 0.9)
    write_wav("fall_queen.wav", fall_queen(), 0.86)
    write_wav("fall_king.wav", fall_king(), 0.9)
    write_wav("music.wav", music(), 0.7)


if __name__ == "__main__":
    main()
