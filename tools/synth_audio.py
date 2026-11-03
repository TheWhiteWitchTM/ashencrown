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


def _quiet_kick(freq, dur, drop):
    return [s * 0.38 for s in _kick(freq, dur, drop)]


def fall_pawn():
    cry = mix(
        tone(220, 0.18, ((1, 1), (2, 0.2), (3, 0.08)), decay=14),
        tone(330, 0.14, ((1, 0.7),), decay=16),
    )
    steel = tone(1568, 0.28, ((1, 1), (2.4, 0.35), (3.6, 0.12)), decay=11)
    return mix(_crack(0.05, 31), _cymbal(0.35, 51, 11), cry, steel, silence(0.02) + _quiet_kick(96, 0.4, 160))


def fall_knight():
    brass = _chord((146.83, 220.0, 293.66, 440.0), 0.9, 2.6, ((1, 1), (2, 0.35), (3, 0.14)))
    n = int(SR * 0.36)
    neigh = []
    for i in range(n):
        t = i / SR
        trem = 0.45 + 0.55 * math.sin(2 * math.pi * 14 * t)
        e = math.exp(-4.4 * t)
        s = math.sin(2 * math.pi * 740 * t) + 0.7 * math.sin(2 * math.pi * 988 * t)
        neigh.append(s * trem * e)
    return mix(_crack(0.06, 32), _cymbal(0.4, 52, 9), brass, neigh, _quiet_kick(78, 0.48, 130))


def fall_bishop():
    bells = []
    for i, f in enumerate((1174.0, 880.0, 659.25, 523.25)):
        note = tone(f, 1.4, ((1, 1), (2.37, 0.2), (3.9, 0.06)), decay=1.6)
        bells.append(silence(i * 0.07) + note)
    bed = _chord((196.0, 311.13, 392.0), 1.4, 1.6)
    return mix(_crack(0.045, 33), _cymbal(0.5, 53, 8), *bells, [s * 0.45 for s in bed], silence(0.04) + _quiet_kick(88, 0.42, 90))


def fall_rook():
    knocks = []
    for i, f in enumerate((210, 168, 250, 140)):
        knocks.append(silence(i * 0.06) + tone(f, 0.16, ((1, 1), (2.2, 0.4), (3.5, 0.15)), decay=18))
    cracks = [silence(i * 0.055) + _crack(0.04, 40 + i) for i in range(4)]
    return mix(*cracks, *knocks, _cymbal(0.28, 54, 14), _quiet_kick(64, 0.55, 100), silence(0.08) + _quiet_kick(52, 0.6, 70))


def fall_queen():
    # The chord stays on its pitches. A long downward glide is the other bad sound.
    stab = _chord((220.0, 261.63, 329.63, 440.0, 659.25), 1.5, 1.5, ((1, 1), (2, 0.28), (3, 0.1), (4, 0.04)))
    shimmer = tone(1568, 0.7, ((1, 1), (2.01, 0.25)), decay=4.5)
    return mix(_crack(0.05, 35), _cymbal(0.55, 55, 6), stab, shimmer, silence(0.03) + _quiet_kick(82, 0.5, 120))


def fall_king():
    low = _chord((130.81, 196.0, 261.63, 329.63), 1.7, 1.35, ((1, 1), (2, 0.32), (3, 0.12)))
    sw = _swell(len(low), 0.05, 1.2)
    low = [a * b for a, b in zip(low, sw)]
    brass = _chord((196.0, 246.94, 293.66, 392.0), 1.15, 1.8, ((1, 1), (2, 0.3), (3, 0.1)))
    return mix(
        _crack(0.07, 36),
        _cymbal(0.7, 56, 5),
        low,
        brass,
        _quiet_kick(70, 0.45, 90),
        silence(0.32) + _quiet_kick(58, 0.6, 70),
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
