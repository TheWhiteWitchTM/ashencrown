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
    body = tone(168, 0.16, ((1, 1), (2, 0.2)), decay=8)
    n = lowpass(noise(0.09, 4), 0.4)
    e = env(len(n), 0.001, 0.04, 0.0, 0.02)
    thud = [a * b * 0.8 for a, b in zip(n, e)]
    return mix(body, thud)


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
    n = noise(0.12, 13)
    e = env(len(n), 0.001, 0.03, 0.1, 0.06)
    return mix([a * b for a, b in zip(n, e)], tone(90, 0.15, ((1, 1),), decay=12))


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
    write_wav("music.wav", music(), 0.7)


if __name__ == "__main__":
    main()
