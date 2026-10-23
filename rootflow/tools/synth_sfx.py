"""The Gloomheart's own sounds, made from math instead of recordings (v0.16.8, Caleb's picks from the
proposals: the rumbling growl, the spit, the heavy landing).

Run: python3 tools/synth_sfx.py   (writes rootflow/sfx/gloom_*.raw; make_sfx.py then builds them in)
The .raw files are kept in the repo like the recorded ones.
"""
import math
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SFX_DIR = os.path.join(ROOT, "sfx")
RATE = 13379


def lowpass(x, a):
    y, out = 0.0, []
    for v in x:
        y += a * (v - y)
        out.append(y)
    return out


def roar():
    """A rumbling 'hrrrooom': filtered noise with a growl that rises, then falls away."""
    n, out = int(1.1 * RATE), []
    rng = random.Random(2)
    noise = lowpass([rng.uniform(-1, 1) for _ in range(n)], 0.08)
    ph = 0.0
    for i in range(n):
        t = i / RATE
        f = 90 + 40 * math.sin(math.pi * min(1, t / 1.1))
        ph += f / RATE
        growl = math.sin(2 * math.pi * ph) * (0.6 + 0.4 * math.sin(2 * math.pi * 23 * t))
        env = min(1, t / 0.1) * max(0, 1 - t / 1.1)
        out.append(env * (0.6 * growl + 3.0 * noise[i]))
    return out


def shoot():
    """'Pthoo': a soft spit of air with a little squelch."""
    n, out, ph = int(0.25 * RATE), [], 0.0
    rng = random.Random(3)
    noise = lowpass([rng.uniform(-1, 1) for _ in range(n)], 0.3)
    for i in range(n):
        t = i / RATE
        f = 300 * math.exp(-t * 9) + 90
        ph += f / RATE
        env = math.exp(-t * 14)
        out.append(env * (0.5 * math.sin(2 * math.pi * ph) + 1.2 * noise[i]))
    return out


def slam():
    """A heavy landing with a short crack of wood and a rumble after it."""
    n, out, ph = int(0.6 * RATE), [], 0.0
    rng = random.Random(4)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    rumble = lowpass(noise, 0.05)
    for i in range(n):
        t = i / RATE
        f = 70 * math.exp(-t * 3) + 30
        ph += f / RATE
        body = math.exp(-t * 5) * math.sin(2 * math.pi * ph)
        crack = math.exp(-t * 60) * noise[i] * 0.7
        out.append(body + crack + 2.5 * rumble[i] * math.exp(-t * 3))
    return out


SOUNDS = {"gloom_roar": roar, "gloom_shoot": shoot, "gloom_slam": slam}


def main():
    os.makedirs(SFX_DIR, exist_ok=True)
    for name, fn in SOUNDS.items():
        x = fn()
        peak = max(abs(v) for v in x) or 1
        data = bytes((max(-127, min(127, round(v / peak * 114))) & 0xFF) for v in x)
        with open(os.path.join(SFX_DIR, name + ".raw"), "wb") as fh:
            fh.write(data)
        print(f"{name}: {len(data)} samples ({len(data) / RATE:.2f} s)")


if __name__ == "__main__":
    main()
