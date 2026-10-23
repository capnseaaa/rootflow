"""Rootflow's music: songs written as notes, turned into src/music.h, plus WAV previews.

Each song has two or three voices that loop together (or play once: "loop": False):
  melody  -> GBA sound channel 2 (square wave)
  bass    -> GBA sound channel 3 (4-bit wave, a warm rounded tone)
  harmony -> GBA sound channel 1 (square wave; optional; the game's chimes share it and cut in)
Per song (all optional): "mel" and "har" = (duty 12/25/50/75, start volume 0-15, envelope step 0-7;
0 holds the note, n drops one level every n/64 s), "bass_vol" 100/50/25 (percent).
Defaults are the v0.10 sound: melody (50, 10, 6), bass 50.
Notes are "NAME OCTAVE LENGTH": "E5 2" is E in octave 5 for 2 steps; "r 4" is a rest of 4 steps.
A step is one eighth note; FRAMES_PER_STEP sets the tempo (the GBA draws 60 frames a second).

Run: python3 tools/make_music.py           (writes src/music.h)
     python3 tools/make_music.py --wav DIR (also writes one preview .wav per song into DIR)
"""
import math
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "music.h")

SONGS = {
    # The title: the valley's tune, slower and fuller.
    "title": {
        "frames_per_step": 18,
        "melody": """
            C5 2 E5 2 G5 4  A5 2 G5 2 E5 4  F5 2 E5 2 D5 2 C5 2  D5 8
            C5 2 E5 2 G5 4  C6 2 B5 2 A5 4  G5 2 E5 2 D5 2 E5 2  C5 8
        """,
        "bass": """
            C3 4 G2 4  A2 4 E3 4  F2 4 C3 4  G2 8
            C3 4 G2 4  A2 4 F2 4  G2 4 G2 4  C3 8
        """,
    },
    # The valley: cozy, bright, C major.
    "valley": {
        "frames_per_step": 15,
        "melody": """
            E5 2 D5 1 C5 1 D5 2 E5 2  G5 4 E5 4  D5 2 C5 1 A4 1 C5 2 D5 2  E5 6 r 2
            E5 2 D5 1 C5 1 D5 2 E5 2  G5 3 A5 1 G5 2 E5 2  D5 2 E5 1 D5 1 C5 2 A4 2  C5 6 r 2
        """,
        "bass": """
            C3 2 G3 2 E3 2 G3 2  C3 2 G3 2 E3 2 G3 2  F2 2 C3 2 A2 2 C3 2  G2 2 D3 2 B2 2 D3 2
            C3 2 G3 2 E3 2 G3 2  A2 2 E3 2 C3 2 E3 2  F2 2 C3 2 G2 2 D3 2  C3 2 G3 2 C3 4
        """,
    },
    # The Windy Steppe: open and airy, D dorian, long notes.
    "steppe": {
        "frames_per_step": 18,
        "melody": """
            D5 4 A4 4  C5 2 D5 2 E5 4  F5 4 E5 2 D5 2  A4 8
            D5 4 F5 4  G5 2 F5 2 E5 4  C5 2 D5 2 E5 2 C5 2  D5 8
        """,
        "bass": """
            D3 8  C3 8  G2 8  A2 8
            D3 8  C3 8  A2 8  D3 8
        """,
    },
    # Stone Hollow: quiet and echoing, A minor.
    "hollow": {
        "frames_per_step": 17,
        "melody": """
            A4 2 C5 2 E5 2 D5 2  C5 4 B4 4  A4 2 B4 2 C5 2 E5 2  B4 6 r 2
            A4 2 C5 2 E5 2 G5 2  F5 4 E5 4  D5 2 C5 2 B4 2 G4 2  A4 6 r 2
        """,
        "bass": """
            A2 8  F2 8  D3 8  E2 8
            A2 8  F2 8  G2 8  A2 8
        """,
    },
    # ---- The Great Tree's story (v0.16.8, Caleb's picks from the proposals) ----
    # The dead wood, before Bark's four tasks: desolate, sad, lacking.
    "withered": {
        "frames_per_step": 24, "mel": (25, 8, 7), "bass_vol": 25,
        "melody": """A4 4 r 2 G4 2  F4 6 r 2  E4 4 F4 2 D4 2  r 8
                     A4 4 r 2 A#4 2  A4 6 r 2  G4 2 F4 2 E4 2 C#4 2  D4 6 r 2""",
        "bass": "D2 16 A#1 8 A1 8  D2 16 G1 8 A1 8",
    },
    # The tasks done, until the four mini bosses fall: adventurous, close to victory, tense.
    "rootward": {
        "frames_per_step": 10, "mel": (25, 11, 3), "har": (12, 4, 2), "bass_vol": 50,
        "melody": """D5 1 D5 1 F5 2 D5 1 D5 1 G5 2  D5 1 D5 1 A5 2 G5 2 F5 2  E5 2 F5 2 G5 2 A5 2  A#5 4 A5 4
                     D5 1 D5 1 F5 2 D5 1 D5 1 G5 2  A5 2 C6 2 A#5 2 A5 2  G5 2 F5 2 E5 2 C#5 2  D5 8""",
        "bass": "BOUNCE D2 D2 C2 A#1 D2 F2 A1 D2",
        "harmony": "ARPS D4-F4-A4 D4-F4-A4 C4-E4-G4 A#3-D4-F4 D4-F4-A4 F4-A4-C5 A3-C#4-E4 D4-F4-A4",
    },
    # The last mini boss falls: Bright, quieter and left open (an augmented chord), then the tree shakes.
    "omen": {
        "frames_per_step": 8, "mel": (50, 7, 4), "har": (50, 4, 4), "bass_vol": 25, "loop": False,
        "melody": "G4 1 C5 1 E5 1 G5 3 E5 1 G#5 1 C6 8",
        "bass": "C3 3 G2 3 C3 10",
        "harmony": "E4 3 r 3 G#4 10",
    },
    # After the mini bosses, until the Gloomheart fight: tense, quiet, something is off.
    "uneasy": {
        "frames_per_step": 22, "mel": (12, 6, 7), "bass_vol": 25,
        "melody": """E5 6 F5 2  E5 8  r 4 A#4 4  B4 8
                     E5 6 F5 2  G5 4 F5 4  E5 4 D#5 4  E5 8""",
        "bass": "E2 16 A#1 8 B1 8  E2 8 C2 8 A1 8 E2 8",
    },
    # The Gloomheart fight: dangerous and tense, clean.
    "gloomfight": {
        "frames_per_step": 8, "mel": (25, 12, 2), "bass_vol": 50,
        "melody": """C5 1 r 1 C5 1 D#5 1 r 1 C5 1 G5 2  F5 1 D#5 1 D5 1 D#5 1 F5 2 G5 2  G#5 2 G5 2 F5 2 D#5 2  D5 4 G4 4
                     C5 1 r 1 C5 1 D#5 1 r 1 C5 1 G5 2  G#5 1 G5 1 F5 1 G5 1 G#5 2 A#5 2  C6 2 A#5 2 G#5 2 G5 2  C5 8""",
        "bass": "BOUNCE C2 C2 G#1 G1 C2 G#1 A#1 C2",
    },
    # The Gloomheart beaten (plays once).
    "win_warm": {
        "frames_per_step": 8, "mel": (50, 11, 5), "har": (25, 5, 4), "bass_vol": 50, "loop": False,
        "melody": "C5 1 D5 1 E5 1 C5 1 F5 2 A5 2 G5 1 A5 1 B5 1 G5 1 C6 6",
        "bass": "F2 4 C3 4 G2 4 C3 6",
        "harmony": "A4 4 A4 4 B4 4 E5 6",
    },
    # Bark feels funny and starts to change: mysterious, a change coming.
    "wholetone": {
        "frames_per_step": 14, "mel": (12, 7, 5), "bass_vol": 25,
        "melody": """C5 1 E5 1 F#5 1 A#5 1 C5 1 E5 1 F#5 1 A#5 1  D5 1 F#5 1 G#5 1 C6 1 D5 1 F#5 1 G#5 1 C6 1
                     E5 1 G#5 1 A#5 1 D6 1 E5 1 G#5 1 A#5 1 D6 1  F#5 1 A#5 1 C6 1 E6 1 E6 4
                     D6 1 A#5 1 F#5 1 D5 1 D6 1 A#5 1 F#5 1 D5 1  C6 1 G#5 1 E5 1 C5 1 C6 1 G#5 1 E5 1 C5 1
                     A#5 1 F#5 1 D5 1 A#4 1 A#5 1 F#5 1 D5 1 A#4 1  C5 8""",
        "bass": "C2 8 D2 8 E2 8 F#2 8  D2 8 C2 8 A#1 8 C2 8",
    },
    # The white fades into the healed tree, shaking: the big reveal (plays once).
    "reveal_grand": {
        "frames_per_step": 10, "mel": (50, 12, 0), "har": (50, 6, 0), "bass_vol": 50, "loop": False,
        "melody": "G4 3 A4 1 B4 1 D5 1 G5 2 F#5 1 G5 9",
        "bass": "G2 3 r 3 D3 2 G2 10",
        "harmony": "B3 6 D4 2 B4 10",
    },
    # Bark's healed tree and its region: peaceful, soft, rich, almost holy, slow.
    "heartwood": {
        "frames_per_step": 26, "mel": (50, 7, 0), "har": (25, 4, 3), "bass_vol": 50,
        "melody": """A4 4 C5 4  F5 8  E5 4 D5 4  C5 8
                     A#4 4 D5 4  F5 4 A5 4  G5 4 E5 4  F5 8""",
        "bass": "F2 8 F2 8 C2 8 F2 8  A#1 8 D2 8 C2 8 F2 8",
        "harmony": "ARPS F4-A4-C5 F4-A4-C5 C4-E4-G4 F4-A4-C5 A#3-D4-F4 D4-F4-A4 C4-E4-G4 F4-A4-C5",
    },
    # ---- Kept for later (Caleb): not played anywhere yet ----
    # Future victory jingles.
    "win_bright": {
        "frames_per_step": 7, "mel": (50, 12, 4), "har": (50, 6, 4), "bass_vol": 50, "loop": False,
        "melody": "G4 1 C5 1 E5 1 G5 3 E5 1 G5 1 C6 8",
        "bass": "C3 3 G2 3 C3 10",
        "harmony": "E4 3 r 3 E5 10",
    },
    "reveal_rising": {
        "frames_per_step": 9, "mel": (50, 13, 7), "har": (50, 7, 7), "bass_vol": 50, "loop": False,
        "melody": "D5 2 D5 1 E5 1 F#5 1 A5 11",
        "bass": "D2 2 r 3 D3 11",
        "harmony": "F#4 2 F#4 1 G4 1 A4 1 D5 11",
    },
    # The end of the game.
    "canopy": {
        "frames_per_step": 22, "mel": (50, 8, 7), "har": (12, 3, 0), "bass_vol": 50,
        "melody": """F#5 2 A5 2 D6 4  C#6 2 A5 2 E5 4  G#5 2 A5 2 B5 2 C#6 2  A5 8
                     F#5 2 A5 2 D6 4  E6 2 D6 2 C#6 4  B5 2 A5 2 G#5 2 E5 2  D5 8""",
        "bass": "D2 8 A1 8 E2 8 A1 8  D2 8 A1 8 E2 8 D2 8",
        "harmony": "F#4 8 E4 8 G#4 8 E4 8 F#4 8 E4 8 G#4 8 F#4 8",
    },
}
SONG_ORDER = ["title", "valley", "steppe", "hollow", "withered", "rootward", "omen", "uneasy", "gloomfight",
              "win_warm", "wholetone", "reveal_grand", "heartwood", "win_bright", "reveal_rising", "canopy"]

NOTE_INDEX = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def expand(text):
    """Shorthands: "BOUNCE A2 G2 ..." = each root as low-high eighths for one bar (8 steps);
    "ARPS F4-A4-C5 ..." = each three-note chord as a 1-step arpeggio, up and back, for one bar."""
    words = text.split()
    if words and words[0] == "BOUNCE":
        return " ".join(f"{r} 1 {r[:-1]}{int(r[-1]) + 1} 1 " * 4 for r in words[1:])
    if words and words[0] == "ARPS":
        out = []
        for c in words[1:]:
            a, b, d = c.split("-")
            out.append(f"{a} 1 {b} 1 {d} 1 {b} 1 " * 2)
        return " ".join(out)
    return text


def parse(text):
    """-> list of (frequency or 0 for a rest, steps)"""
    tokens = expand(text).split()
    out = []
    for name, steps in zip(tokens[0::2], tokens[1::2]):
        if name == "r":
            out.append((0.0, int(steps)))
            continue
        pitch, octave = name[:-1], int(name[-1])
        midi = 12 * (octave + 1) + NOTE_INDEX[pitch]
        out.append((440.0 * 2 ** ((midi - 69) / 12), int(steps)))
    return out


def square_reg(f):
    return 0 if f == 0 else max(0, min(2047, round(2048 - 131072 / f)))


def wave_reg(f):
    return 0 if f == 0 else max(0, min(2047, round(2048 - 65536 / f)))


DUTY_CODE = {12: 0, 25: 1, 50: 2, 75: 3}
DUTY_FRAC = {12: 0.125, 25: 0.25, 50: 0.5, 75: 0.75}
BASS_CODE = {100: 1, 50: 2, 25: 3}          # channel 3 volume bits 13-14
VOICES = ("melody", "bass", "harmony")


def settings(song):
    return song.get("mel", (50, 10, 6)), song.get("har", (50, 4, 3)), song.get("bass_vol", 50), song.get("loop", True)


def check(name, song):
    m = sum(s for _, s in parse(song["melody"]))
    for v in ("bass", "harmony"):
        if v in song:
            n = sum(s for _, s in parse(song[v]))
            assert m == n, f"{name}: melody is {m} steps, {v} is {n}; they must line up"


def env_reg(duty, vol, step):
    return (vol << 12) | (step << 8) | (DUTY_CODE[duty] << 6)


def write_header():
    lines = ["// Generated by tools/make_music.py. Do not edit by hand.", "#pragma once", ""]
    lines.append(f"#define NUM_SONGS {len(SONG_ORDER)}")
    for i, name in enumerate(SONG_ORDER):
        lines.append(f"#define SONG_{name.upper()} {i}")
    # A note is one u16 for the frequency register (0 = rest) and one u8 for its length in steps.
    for name in SONG_ORDER:
        song = SONGS[name]
        check(name, song)
        for voice, reg in (("melody", square_reg), ("bass", wave_reg), ("harmony", square_reg)):
            if voice not in song:
                continue
            notes = parse(song[voice])
            lines.append(f"static const unsigned short {name}_{voice}_freq[{len(notes)}] = {{ "
                         + ", ".join(str(reg(f)) for f, _ in notes) + " };")
            lines.append(f"static const unsigned char {name}_{voice}_len[{len(notes)}] = {{ "
                         + ", ".join(str(s) for _, s in notes) + " };")
    def ptr(name, voice, kind):
        return f"{name}_{voice}_{kind}" if voice in SONGS[name] else "0"
    lines.append("static const unsigned short *const song_freq[NUM_SONGS][3] = {")
    for name in SONG_ORDER:
        lines.append("    { " + ", ".join(ptr(name, v, "freq") for v in VOICES) + " },")
    lines.append("};")
    lines.append("static const unsigned char *const song_len[NUM_SONGS][3] = {")
    for name in SONG_ORDER:
        lines.append("    { " + ", ".join(ptr(name, v, "len") for v in VOICES) + " },")
    lines.append("};")
    lines.append("static const unsigned short song_count[NUM_SONGS][3] = {")
    for name in SONG_ORDER:
        s = SONGS[name]
        lines.append("    { " + ", ".join(str(len(parse(s[v])) if v in s else 0) for v in VOICES) + " },")
    lines.append("};")
    lines.append("static const unsigned char song_fps[NUM_SONGS] = { "
                 + ", ".join(str(SONGS[n]["frames_per_step"]) for n in SONG_ORDER) + " };")
    # per song: the melody's and harmony's envelope registers, the bass volume, loop or play once
    lines.append("static const unsigned short song_mel_env[NUM_SONGS] = { "
                 + ", ".join(str(env_reg(*settings(SONGS[n])[0])) for n in SONG_ORDER) + " };")
    lines.append("static const unsigned short song_har_env[NUM_SONGS] = { "
                 + ", ".join(str(env_reg(*settings(SONGS[n])[1])) for n in SONG_ORDER) + " };")
    lines.append("static const unsigned short song_bass_cnt[NUM_SONGS] = { "
                 + ", ".join(str(BASS_CODE[settings(SONGS[n])[2]] << 13) for n in SONG_ORDER) + " };")
    lines.append("static const unsigned char song_loop[NUM_SONGS] = { "
                 + ", ".join("1" if settings(SONGS[n])[3] else "0" for n in SONG_ORDER) + " };")
    # the bass wave: 32 samples, 4 bits each, one smooth sine cycle (a warm, round tone)
    wave = [min(15, max(0, round(7.5 + 7 * math.sin(2 * math.pi * i / 32)))) for i in range(32)]
    packed = [(wave[2 * i] << 4) | wave[2 * i + 1] for i in range(16)]
    lines.append("static const unsigned char bass_wave[16] = { " + ", ".join(str(v) for v in packed) + " };")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote", OUT)
    return wave


def write_wav(name, song, wave, path):
    """A preview close to the GBA: square waves with the song's duty and envelope (one level per
    step/64 s), and the 4-bit wave for the bass. Loops twice, or once with a tail."""
    rate = 32768
    mel, har, bass_vol, loop = settings(song)
    loops = 2 if loop else 1
    step_s = song["frames_per_step"] / 60.0
    total = int(sum(s for _, s in parse(song["melody"])) * step_s * rate) * loops + (0 if loop else rate)
    def square(notes, duty, vol0, step):
        buf, pos = [0.0] * total, 0
        for f, steps in notes:
            n = int(steps * step_s * rate)
            for i in range(min(n, total - pos)) if f else ():
                t = i / rate
                vol = vol0 if step == 0 else max(0, vol0 - int(t * 64 / step))
                buf[pos + i] = vol / 15.0 * (1 if (t * f) % 1 < DUTY_FRAC[duty] else -1)
            pos += n
        return buf
    def bass(notes):
        buf, pos = [0.0] * total, 0
        for f, steps in notes:
            n = int(steps * step_s * rate)
            for i in range(min(n, total - pos)) if f else ():
                buf[pos + i] = bass_vol / 100 * 1.1 * (wave[int((i / rate * f) % 1 * 32)] / 7.5 - 1)
            pos += n
        return buf
    voices = [square(parse(" ".join([expand(song["melody"])] * loops)), *mel),
              bass(parse(" ".join([expand(song["bass"])] * loops)))]
    if "harmony" in song:
        voices.append(square(parse(" ".join([expand(song["harmony"])] * loops)), *har))
    mix = [max(-1, min(1, 0.38 * sum(v[i] for v in voices))) for i in range(total)]
    data = b"".join(struct.pack("<h", int(v * 24000)) for v in mix)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt "
                + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", len(data)) + data)
    print("wrote", path)


def main():
    wave = write_header()
    if "--wav" in sys.argv:
        d = sys.argv[sys.argv.index("--wav") + 1]
        os.makedirs(d, exist_ok=True)
        for name in SONG_ORDER:
            write_wav(name, SONGS[name], wave, os.path.join(d, f"rootflow_{name}.wav"))


if __name__ == "__main__":
    main()
