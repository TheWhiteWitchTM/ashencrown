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


def melee():
    n = noise(0.28, 7)
    e = env(len(n), 0.002, 0.05, 0.25, 0.12)
    ring = tone(620, 0.22, ((1, 0.4), (2.1, 0.25), (3.4, 0.1)), decay=10)
    return mix([a * b for a, b in zip(n, e)], ring)


def magic():
    n = int(SR * 0.55)
    out = []
    for i in range(n):
        t = i / n
        f = 280 + 740 * t
        e = math.sin(math.pi * t) * (1 - t)
        out.append(math.sin(2 * math.pi * f * i / SR) * e + 0.25 * math.sin(2 * math.pi * f * 1.5 * i / SR) * e)
    return out


def holy():
    notes = [523.25, 659.25, 783.99]
    tracks = []
    for i, f in enumerate(notes):
        tracks.append(pad(tone(f, 0.7, ((1, 1), (2, 0.15), (3, 0.08)), decay=2.2), 0.9))
        # stagger
        tracks[-1] = silence(i * 0.03) + tracks[-1]
    return mix(*tracks)


def beast():
    n = int(SR * 0.45)
    rng = random.Random(9)
    raw = []
    y = 0
    for i in range(n):
        y += 0.08 * (rng.uniform(-1, 1) - y)
        t = i / SR
        growl = math.sin(2 * math.pi * (70 + 20 * math.sin(2 * math.pi * 8 * t)) * t)
        e = math.sin(math.pi * min(1, t / 0.45)) ** 0.6
        raw.append((growl * 0.8 + y) * e)
    return raw


def siege():
    boom = tone(55, 0.55, ((1, 1), (2, 0.4), (3, 0.2)), decay=4)
    n = lowpass(noise(0.4, 11), 0.08)
    e = env(len(n), 0.001, 0.08, 0.3, 0.2)
    return mix(boom, [a * b for a, b in zip(n, e)])


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
    # The killing blow: a crack, then a metal ring that hangs in the quiet.
    n = noise(0.16, 13)
    e = env(len(n), 0.001, 0.02, 0.08, 0.08)
    crack = [a * b for a, b in zip(n, e)]
    ring = tone(740, 0.55, ((1, 0.45), (2.01, 0.28), (3.2, 0.12), (4.7, 0.05)), decay=6)
    body = tone(86, 0.28, ((1, 1), (2, 0.35)), decay=11)
    return mix(crack, [s * 0.7 for s in ring], body)


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


def _thud(seed, dur=0.34, freq=58):
    nse = lowpass(noise(dur, seed), 0.08)
    e = env(len(nse), 0.002, 0.05, 0.2, 0.16)
    body = tone(freq, dur, ((1, 1), (2, 0.4), (3, 0.15)), decay=7)
    return mix(body, [a * b * 0.85 for a, b in zip(nse, e)])


def fall_pawn():
    n = int(SR * 0.42)
    rng = random.Random(21)
    y = 0.0
    grunt = []
    for i in range(n):
        t = i / SR
        f = 128 * (1 + 0.04 * math.sin(2 * math.pi * 7 * t))
        saw = 2 * ((f * t) % 1 - 0.5)
        y += 0.16 * (saw - y)
        breath = rng.uniform(-1, 1) * 0.2
        e = math.sin(math.pi * min(1, t / 0.42)) ** 1.3
        grunt.append((y * 0.85 + breath) * e)
    return silence(0.06) + mix(grunt, silence(0.28) + _thud(22, 0.3, 62))


def fall_knight():
    n = int(SR * 0.7)
    raw = []
    for i in range(n):
        t = i / SR
        u = t / 0.7
        f = 220 + 380 * math.sin(math.pi * u) - 80 * u
        f *= 1 + 0.06 * math.sin(2 * math.pi * 14 * t)
        e = math.sin(math.pi * min(1, u)) ** 0.7
        raw.append(math.sin(2 * math.pi * f * t) * e)
    cry = lowpass(raw, 0.35)
    return silence(0.05) + mix(cry, silence(0.55) + _thud(23, 0.36, 70))


def fall_bishop():
    bells = []
    for i, f in enumerate((880, 659.25, 392.0, 246.94)):
        note = tone(f, 1.15, ((1, 1), (2.37, 0.18), (3.9, 0.06)), decay=1.6)
        bells.append(silence(0.08 + i * 0.16) + note)
    whoosh = lowpass(noise(0.9, 24), 0.05)
    e = env(len(whoosh), 0.2, 0.3, 0.2, 0.3)
    air = [a * b * 0.35 for a, b in zip(whoosh, e)]
    return silence(0.1) + mix(*bells, air)


def fall_rook():
    crack = noise(0.12, 25)
    e = env(len(crack), 0.001, 0.02, 0.05, 0.06)
    snap = [a * b for a, b in zip(crack, e)]
    rumble = lowpass(noise(0.9, 26), 0.04)
    re = env(len(rumble), 0.01, 0.12, 0.45, 0.35)
    stone = mix(tone(42, 0.9, ((1, 1), (2.2, 0.4), (3.1, 0.2)), decay=2.4), [a * b for a, b in zip(rumble, re)])
    return silence(0.04) + mix(snap, silence(0.08) + stone)


def fall_queen():
    n = int(SR * 1.35)
    out = []
    for i in range(n):
        t = i / SR
        u = t / 1.35
        f = 1480 - 1280 * (u ** 1.4)
        e = math.sin(math.pi * min(1, u / 0.15)) * math.exp(-1.5 * max(0, u - 0.2))
        s = math.sin(2 * math.pi * f * t)
        s += 0.45 * math.sin(2 * math.pi * f * 1.5 * t)
        s += 0.25 * math.sin(2 * math.pi * (f * 0.5 + 40) * t)
        out.append(s * e)
    bed = silence(0.35) + tone(110, 1.1, ((1, 1), (2, 0.2)), decay=1.5)
    return silence(0.08) + mix([s * 0.8 for s in out], bed)


def fall_king():
    n = int(SR * 1.7)
    envl = _swell(n, 0.18, 1.15)
    horn = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        u = t / 1.7
        f = 130 - 78 * (u ** 1.15)
        phase += f / SR
        s = 0
        for h, a in ((1, 1), (3, 0.45), (5, 0.18), (2, 0.12)):
            s += a * math.sin(2 * math.pi * phase * h)
        horn.append(s * envl[i])
    drum = silence(1.15) + _thud(27, 0.45, 48)
    return silence(0.1) + mix([s * 0.7 for s in horn], drum)


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
    write_wav("check.wav", check(), 0.8)
    write_wav("win.wav", fanfare(True), 0.8)
    write_wav("lose.wav", fanfare(False), 0.75)
    write_wav("hit.wav", hit(), 0.9)
    write_wav("drums.wav", drums(), 0.9)
    write_wav("notify.wav", notify(), 0.6)
    write_wav("sting.wav", sting(), 0.55)
    write_wav("fall_pawn.wav", fall_pawn(), 0.8)
    write_wav("fall_knight.wav", fall_knight(), 0.82)
    write_wav("fall_bishop.wav", fall_bishop(), 0.7)
    write_wav("fall_rook.wav", fall_rook(), 0.86)
    write_wav("fall_queen.wav", fall_queen(), 0.72)
    write_wav("fall_king.wav", fall_king(), 0.78)
    write_wav("music.wav", music(), 0.7)


if __name__ == "__main__":
    main()
