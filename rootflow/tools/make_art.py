"""Draw the toy's pixel art and save it as indexed PNGs in art/.

The PNGs are the source of truth after the first run: edit them in any
pixel editor (keep the 16-color palette order), then rebuild.
Run with --force to regenerate them from this script.
"""
import os
import random
import sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")

# ---- terrain palette (BG bank 0) ----
BG_PAL = [
    (16, 20, 24),     # 0 backdrop
    (74, 68, 64),     # 1 dead dark
    (107, 98, 90),    # 2 dead mid
    (138, 128, 120),  # 3 dead light
    (139, 94, 60),    # 4 dirt
    (168, 119, 76),   # 5 dirt light
    (63, 122, 58),    # 6 grass dark
    (106, 168, 79),   # 7 grass mid
    (156, 204, 101),  # 8 grass light
    (47, 95, 158),    # 9 water dark
    (79, 143, 214),   # 10 water light
    (168, 212, 240),  # 11 water foam
    (90, 58, 34),     # 12 trunk
    (217, 96, 59),    # 13 flower orange
    (240, 226, 182),  # 14 cream
    (28, 36, 32),     # 15 outline
]

# ---- sprite palette (OBJ bank 0) ----
OBJ_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline
    (240, 200, 160),  # 2 skin
    (139, 94, 60),    # 3 hat
    (90, 58, 34),     # 4 hat band / hair
    (63, 120, 200),   # 5 shirt
    (42, 79, 138),    # 6 shirt dark
    (74, 68, 64),     # 7 pants
    (255, 255, 255),  # 8 white
    (184, 240, 138),  # 9 spirit light
    (106, 196, 106),  # 10 spirit mid
    (47, 122, 58),    # 11 spirit dark
    (156, 204, 101),  # 12 leaf
    (255, 236, 140),  # 13 glow
    (240, 226, 182),  # 14 cream
    (217, 96, 59),    # 15 orange (backpack)
]

# Metatile order matters: the game refers to these by index.
META = ["dead", "dead_crack", "dirt", "grass", "grass_tuft", "flowers",
        "tree", "dead_tree", "rock", "water", "water_b", "bridge",
        "thorn", "homesite", "chest", "chest_open", "wreck", "dry_grass", "pebbles", "menhir",
        "dry_river", "dry_bridge", "path", "flowerbed", "flowerbed_empty", "sapling",
        "fruit_tree", "fruit_tree_bare", "lantern", "bench",
        "dam", "lever", "lever_pulled", "lost_stone", "blossom", "fountain", "statue", "pinwheel"]

# ---- water spirit palette (OBJ bank 1). The game builds a gray copy for sleep. ----
RIPPLE_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline
    (168, 212, 240),  # 2 light blue
    (79, 143, 214),   # 3 mid blue
    (47, 95, 158),    # 4 dark blue
    (255, 255, 255),  # 5 white
    (210, 245, 255),  # 6 shine
] + [(0, 0, 0)] * 9


def blank(c):
    return [[c] * 16 for _ in range(16)]


def speckle(px, rng, colors, n):
    for _ in range(n):
        px[rng.randrange(16)][rng.randrange(16)] = rng.choice(colors)


def dead(rng):
    px = blank(2)
    speckle(px, rng, [1, 3], 26)
    return px


def dead_crack(rng):
    px = dead(rng)
    x, y = rng.randrange(3, 13), 0
    while y < 16:
        px[y][x] = 1
        y += 1
        x = max(0, min(15, x + rng.choice([-1, 0, 1])))
    for _ in range(4):
        px[rng.randrange(16)][rng.randrange(16)] = 1
    return px


def dirt(rng):
    px = blank(4)
    speckle(px, rng, [5, 12], 24)
    return px


def grass(rng, tufts=0, flowers=0):
    px = blank(7)
    speckle(px, rng, [6, 8], 30)
    for _ in range(tufts):
        x, y = rng.randrange(1, 15), rng.randrange(2, 15)
        px[y][x] = 8
        px[y - 1][x] = 8
        px[y][x - 1] = 6
        px[y][x + 1] = 6
    for _ in range(flowers):
        x, y = rng.randrange(1, 15), rng.randrange(1, 15)
        c = rng.choice([13, 14])
        px[y][x] = c
        px[y - 1][x] = c
        px[y + 1][x] = c
        px[y][x - 1] = c
        px[y][x + 1] = c
        px[y][x] = 8 if c == 13 else 13
    return px


def tree(rng):
    px = grass(rng)
    # trunk
    for y in range(10, 15):
        for x in (7, 8):
            px[y][x] = 12
    px[14][6] = 15
    px[14][9] = 15
    # canopy (circle)
    for y in range(0, 12):
        for x in range(1, 15):
            d = (x - 7.5) ** 2 + (y - 5.5) ** 2
            if d < 36:
                px[y][x] = 6
            if d < 25:
                px[y][x] = 7
            if d < 9 and x < 8 and y < 6:
                px[y][x] = 8
            if 36 <= d < 46 and y < 11:
                px[y][x] = 15
    return px


def dead_tree(rng):
    px = dead(rng)
    for y in range(4, 15):
        px[y][7] = 12
        px[y][8] = 1
    branches = [(6, 6, -1), (5, 9, 1), (8, 5, 1), (9, 10, -1)]
    for y, x0, dx in branches:
        x = x0
        for k in range(4):
            px[y - k // 2][x] = 12
            x += dx
    px[14][6] = 1
    px[14][9] = 1
    return px


def rock(rng):
    px = dead(rng)
    for y in range(2, 15):
        for x in range(1, 15):
            d = ((x - 7.5) / 6.5) ** 2 + ((y - 9) / 5.5) ** 2
            if d < 1.0:
                px[y][x] = 3
            if d < 1.0 and (x - 5) ** 2 + (y - 7) ** 2 < 6:
                px[y][x] = 14
            if d < 1.0 and y > 11 and x > 7:
                px[y][x] = 2
            if 1.0 <= d < 1.35:
                px[y][x] = 15
    return px


def water(rng, phase):
    px = blank(9)
    for y in range(16):
        for x in range(16):
            if (x + y * 3 + phase * 5) % 11 == 0 or (x * 2 - y + phase * 3) % 13 == 0:
                px[y][x] = 10
    for _ in range(3):
        x, y = rng.randrange(1, 14), rng.randrange(16)
        px[y][x] = 11
        px[y][x + 1] = 11
    return px


def bridge(rng):
    px = water(rng, 0)
    # vertical planks with water showing in the gaps
    for x in range(1, 15):
        if x % 4 == 0:
            continue
        c = 5 if x % 4 == 1 else 4
        for y in range(2, 14):
            px[y][x] = c
        px[rng.randrange(3, 13)][x] = 12   # wood grain
    # rope rails
    for x in range(16):
        px[1][x] = 12
        px[14][x] = 12
        px[0][x] = 15
        px[15][x] = 15
    for x in (0, 15):
        for y in range(0, 16):
            px[y][x] = 15 if y in (0, 1, 14, 15) else px[y][x]
    return px


def thorn(rng):
    px = dead(rng)
    # tangled dead brambles: a few wandering strands with thorn tips
    for k in range(7):
        x, y = rng.randrange(1, 14), 15
        while y > 0:
            c = 12 if k % 2 else 15
            px[y][x] = c
            px[y][x + 1] = 15 if c == 12 else 12
            if rng.random() < 0.25:
                px[y][max(0, x - 1)] = 14
            y -= 1
            x = max(0, min(14, x + rng.choice([-1, 0, 1])))
    return px


def homesite(rng):
    px = dirt(rng)
    # a ring of small cream stones marks where a spirit could live
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if 30 <= d < 42 and (x + y) % 3 != 0:
                px[y][x] = 14
            elif 42 <= d < 50 and (x + y) % 3 != 0:
                px[y][x] = 15
    px[7][7] = px[8][8] = 8
    px[7][8] = px[8][7] = 6
    return px


def chest(rng, open_=False):
    px = dead(rng)
    # a small wooden chest with a gold latch
    for y in range(5, 14):
        for x in range(2, 14):
            edge = y in (5, 13) or x in (2, 13)
            px[y][x] = 15 if edge else (5 if y < 9 else 4)
    for x in range(3, 13):
        px[9][x] = 12                      # lid seam
    if open_:
        for y in range(6, 9):
            for x in range(3, 13):
                px[y][x] = 15                  # dark empty inside
        for x in range(3, 13):
            px[4][x] = 12; px[3][x] = 15       # lid tipped back
    else:
        px[9][7] = px[9][8] = 14; px[10][7] = px[10][8] = 13   # latch
    return px


def wreck(rng):
    px = dead(rng)
    # a rusted Vael pump: a squat tank, a bent pipe, rivets
    for y in range(6, 15):
        for x in range(3, 12):
            edge = y in (6, 14) or x in (3, 11)
            px[y][x] = 15 if edge else (3 if (x + y) % 4 else 13)
    for y in range(2, 7):
        px[y][9] = 15; px[y][10] = 1
    for x in range(10, 15):
        px[2][x] = 15; px[3][x] = 1
    for x, y in ((5, 8), (9, 8), (5, 12), (9, 12)):
        px[y][x] = 14
    px[10][6] = px[10][7] = px[10][8] = 1     # dead gauge slot
    return px


def dry_grass(rng):
    """Steppe ground: dead land with tall dry tufts."""
    px = dead(rng)
    for _ in range(9):
        x, y = rng.randrange(1, 15), rng.randrange(5, 16)
        h = rng.randrange(3, 6)
        for k in range(h):
            px[y - k][x] = 14 if k == h - 1 else 3
        px[y - 2][x - 1] = 3
        px[y - 1][min(15, x + 1)] = 1
    return px


def pebbles(rng):
    """Hollow ground: dead land strewn with small stones."""
    px = dead(rng)
    for _ in range(5):
        x, y = rng.randrange(1, 14), rng.randrange(1, 14)
        px[y][x] = px[y][x + 1] = 3
        px[y + 1][x] = px[y + 1][x + 1] = 1
        px[y][x] = 14
    return px


def menhir(rng):
    """A standing stone: a tall weathered slab with a patch of moss."""
    px = dead(rng)
    for y in range(0, 15):
        for x in range(2, 14):
            d = abs(x - 7.5) - (5.5 if y > 3 else y + 2)
            if d < 0:
                px[y][x] = 3 if x < 8 else 2
            if -1 <= d < 0 or y == 14:
                px[y][x] = 15
    px[3][6] = px[4][6] = px[5][5] = 14
    px[10][6] = px[10][7] = px[11][6] = 6
    return px


def dry_river(rng):
    """A dried-up riverbed: a sunken channel of cracked mud and stones."""
    px = blank(12)
    speckle(px, rng, [4, 15], 30)
    for _ in range(3):                      # cracks
        x, y = rng.randrange(2, 14), rng.randrange(2, 14)
        for k in range(4):
            px[min(15, y + k)][min(15, x + (k & 1))] = 15
    for _ in range(4):                      # dry stones
        x, y = rng.randrange(1, 14), rng.randrange(1, 14)
        px[y][x] = px[y][x + 1] = 3
        px[y + 1][x] = 1
    for x in range(16):                     # dark banks top and bottom of the tile edge
        if rng.random() < 0.5:
            px[0][x] = 15
    return px


def dry_bridge(rng):
    """Ripple's planks laid over a dry riverbed."""
    px = dry_river(rng)
    for x in range(1, 15):
        if x % 4 == 0:
            continue
        c = 5 if x % 4 == 1 else 4
        for y in range(2, 14):
            px[y][x] = c
        px[rng.randrange(3, 13)][x] = 12
    for x in range(16):
        px[1][x] = 12; px[14][x] = 12; px[0][x] = 15; px[15][x] = 15
    return px


def path(rng):
    """Stone path slabs set into the grass."""
    px = grass(rng)
    slabs = [(1, 1, 6, 6), (8, 1, 14, 5), (1, 8, 5, 14), (7, 7, 14, 14)]
    for x0, y0, x1, y1 in slabs:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                edge = x in (x0, x1) or y in (y0, y1)
                px[y][x] = 15 if edge else (14 if (x + y) % 5 == 0 else 3)
    return px


def flowerbed(rng, empty=False):
    """A wooden-edged bed of soil; full of flowers, or freshly picked with green shoots."""
    px = grass(rng)
    for y in range(2, 14):
        for x in range(1, 15):
            edge = x in (1, 14) or y in (2, 13)
            px[y][x] = 12 if edge else 4
    for y in range(4, 12, 3):
        for x in range(3, 13, 3):
            if empty:
                px[y][x] = 8; px[y + 1][x] = 6
            else:
                c = 13 if (x + y) % 2 else 14
                px[y][x] = c; px[y - 1][x] = c; px[y][x - 1] = c; px[y][x + 1] = c
                px[y + 1][x] = 6
    return px


def sapling(rng):
    """A young fruit tree: a thin trunk and a few leaves."""
    px = grass(rng)
    for y in range(8, 14):
        px[y][7] = 12
    for x, y in ((6, 6), (7, 5), (8, 6), (5, 7), (9, 7), (7, 7), (6, 8), (8, 8)):
        px[y][x] = 8 if (x + y) % 2 else 6
    px[13][6] = px[13][8] = 15
    return px


def fruit_tree(rng, fruit=True):
    """A planted fruit tree; with ripe orange fruit, or bare after picking."""
    px = tree(rng)
    if fruit:
        for x, y in ((4, 4), (9, 3), (6, 7), (11, 6), (8, 9), (3, 8)):
            px[y][x] = 13; px[y][x + 1] = 13; px[y + 1][x] = 13; px[y - 1][x + 1] = 14
    return px


def lantern(rng):
    """A small stone lantern with a warm light."""
    px = grass(rng)
    for y in range(3, 15):
        for x in range(5, 11):
            w = 2 if y < 5 else (3 if y < 11 else 2)
            if abs(x - 7.5) <= w:
                px[y][x] = 3
                if abs(x - 7.5) > w - 1 or y in (3, 14):
                    px[y][x] = 15
    px[2][7] = px[2][8] = 15
    for y in range(6, 10):
        for x in (7, 8):
            px[y][x] = 14 if y in (7, 8) else 13
    return px


def bench(rng):
    """A wooden bench for resting."""
    px = grass(rng)
    for x in range(1, 15):
        px[6][x] = 15; px[7][x] = 5; px[8][x] = 4      # back rest
        px[10][x] = 15; px[11][x] = 5; px[12][x] = 4   # seat
    for x in (2, 13):
        for y in range(6, 15):
            px[y][x] = 12
        px[14][x] = 15
    return px


def dam(rng):
    """A rusted Vael dam gate across the riverbed: riveted plates and a dark seam."""
    px = dry_river(rng)
    for y in range(1, 15):
        for x in range(0, 16):
            px[y][x] = 3 if (x // 4 + y // 5) % 2 else 2
            if y in (1, 14) or x % 4 == 0:
                px[y][x] = 15
    for x in range(1, 15, 2):
        px[3][x] = 14; px[12][x] = 14              # rivets
    for y in range(5, 11):
        px[y][7] = 13; px[y][8] = 13               # rust streak
    return px


def lever(rng, pulled=False):
    """A Vael lever on a post; pulled, the handle points the other way."""
    px = dead(rng)
    for y in range(9, 15):
        for x in range(5, 11):
            px[y][x] = 15 if x in (5, 10) or y in (9, 14) else 3
    sx, dx = (7, 1) if pulled else (8, -1)
    for k in range(7):
        px[8 - k][sx + dx * (k // 2)] = 12
    hx = sx + dx * 3
    px[1][hx] = px[1][hx + 1] = px[2][hx] = px[2][hx + 1] = 13
    return px


def lost_stone(rng):
    """Pebble's lost standing stone: a small menhir with a glowing mark."""
    px = menhir(rng)
    for y, x in ((6, 7), (7, 6), (7, 8), (8, 7), (9, 7)):
        px[y][x] = 14
    return px


def blossom(rng):
    """Sprout's reward: a blossom tree with pink-orange flowers."""
    px = tree(rng)
    for x, y in ((4, 3), (8, 2), (11, 4), (5, 7), (9, 6), (7, 9), (3, 6), (11, 8)):
        px[y][x] = 14; px[y][x + 1] = 13; px[y + 1][x] = 13
    return px


def fountain(rng):
    """Ripple's reward: a stone basin with a jet of water."""
    px = grass(rng)
    for y in range(8, 15):
        for x in range(2, 14):
            edge = y in (8, 14) or x in (2, 13)
            px[y][x] = 15 if edge else (3 if y > 11 else 10)
    for y in range(2, 9):
        px[y][7] = 11; px[y][8] = 10
    px[2][6] = px[3][9] = px[1][7] = 11
    return px


def statue(rng):
    """Pebble's reward: a small stone statue of Pebble on a plinth."""
    px = grass(rng)
    for y in range(11, 15):
        for x in range(3, 13):
            px[y][x] = 15 if y in (11, 14) or x in (3, 12) else 2
    for y in range(3, 11):
        for x in range(4, 12):
            d = ((x - 7.5) / 4) ** 2 + ((y - 7) / 4) ** 2
            if d < 1:
                px[y][x] = 3
            if 0.75 <= d < 1:
                px[y][x] = 15
    px[6][6] = px[6][9] = 15
    px[3][7] = px[2][8] = 6                        # moss cap
    return px


def pinwheel(rng):
    """Breeze's reward: a pinwheel on a stick, spinning in the wind."""
    px = grass(rng)
    for y in range(7, 15):
        px[y][7] = 12
    blades = [((7, 2), (8, 3), (8, 4)), ((10, 6), (9, 5), (8, 5)), ((6, 9), (6, 8), (7, 7)), ((3, 5), (5, 5), (6, 6))]
    for i, blade in enumerate(blades):
        for x, y in blade:
            px[y][x] = 13 if i % 2 else 14
    px[6][7] = 15
    return px


def make_terrain():
    rng = random.Random(7)
    tiles = [
        dead(rng), dead_crack(rng), dirt(rng), grass(rng, tufts=0),
        grass(rng, tufts=5), grass(rng, tufts=1, flowers=3), tree(rng),
        dead_tree(rng), rock(rng), water(rng, 0), water(rng, 1), bridge(rng),
        thorn(rng), homesite(rng), chest(rng), chest(rng, True), wreck(rng),
        dry_grass(rng), pebbles(rng), menhir(rng),
        dry_river(rng), dry_bridge(rng), path(rng), flowerbed(rng), flowerbed(rng, empty=True), sapling(rng),
        fruit_tree(rng), fruit_tree(rng, fruit=False), lantern(rng), bench(rng),
        dam(rng), lever(rng), lever(rng, pulled=True), lost_stone(rng), blossom(rng), fountain(rng),
        statue(rng), pinwheel(rng),
    ]
    assert len(tiles) == len(META)
    img = Image.new("P", (16 * len(tiles), 16))
    img.putpalette([c for rgb in BG_PAL for c in rgb])
    for i, px in enumerate(tiles):
        for y in range(16):
            for x in range(16):
                img.putpixel((i * 16 + x, y), px[y][x])
    return img


# ---- sprites from ASCII art. '.' = transparent, hex digits = palette index ----
SPRITES = {
    "down0": [
        "................",
        ".....111111.....",
        "....13333331....",
        "...1344444431...",
        "...1333333331...",
        "....12222221....",
        "....12122121....",
        "....12222221....",
        ".....122221.....",
        "....15555551....",
        "...1255555521...",
        "...1255555521...",
        "....16666661....",
        ".....177771.....",
        ".....171171.....",
        ".....11..11.....",
    ],
    "down1": [
        "................",
        ".....111111.....",
        "....13333331....",
        "...1344444431...",
        "...1333333331...",
        "....12222221....",
        "....12122121....",
        "....12222221....",
        ".....122221.....",
        "....15555551....",
        "...1255555521...",
        "...1255555521...",
        "....16666661....",
        ".....177771.....",
        "....171..171....",
        "....11....11....",
    ],
    "up0": [
        "................",
        ".....111111.....",
        "....13333331....",
        "...1344444431...",
        "...1333333331...",
        "....14444441....",
        "....14444441....",
        "....14444441....",
        ".....144441.....",
        "....1ffffff1....",
        "...12ffffff21...",
        "...12ffffff21...",
        "....16666661....",
        ".....177771.....",
        ".....171171.....",
        ".....11..11.....",
    ],
    "up1": [
        "................",
        ".....111111.....",
        "....13333331....",
        "...1344444431...",
        "...1333333331...",
        "....14444441....",
        "....14444441....",
        "....14444441....",
        ".....144441.....",
        "....1ffffff1....",
        "...12ffffff21...",
        "...12ffffff21...",
        "....16666661....",
        ".....177771.....",
        "....171..171....",
        "....11....11....",
    ],
    "side0": [
        "................",
        ".....111111.....",
        "....13333331....",
        "....134444441...",
        "....13333333311.",
        ".....1222221....",
        ".....1222212....",
        ".....1222222....",
        "......12221.....",
        ".....1ff5551....",
        "....1ff55521....",
        "....1ff55521....",
        ".....166661.....",
        "......17771.....",
        "......17171.....",
        "......11.11.....",
    ],
    "side1": [
        "................",
        ".....111111.....",
        "....13333331....",
        "....134444441...",
        "....13333333311.",
        ".....1222221....",
        ".....1222212....",
        ".....1222222....",
        "......12221.....",
        ".....1ff5551....",
        "....1ff55521....",
        "....1ff55521....",
        ".....166661.....",
        "......17771.....",
        ".....171..171...",
        ".....11...11....",
    ],
    "spirit0": [
        "................",
        "......c...c.....",
        ".....ccc.ccc....",
        "......cbcbc.....",
        "........b.......",
        ".....111111.....",
        "....19999991....",
        "...199999aa91...",
        "...19a1aa1aa1...",
        "...19aaaaaaa1...",
        "...1aaaaaaaa1...",
        "...1aabbbbaa1...",
        "....1bbbbbb1....",
        ".....111111.....",
        "................",
        "................",
    ],
    "spirit1": [
        "................",
        ".....c.....c....",
        "....ccc...ccc...",
        ".....ccb.bcc....",
        "........b.......",
        ".....111111.....",
        "....19999991....",
        "...199999aa91...",
        "...19a1aa1aa1...",
        "...19aaaaaaa1...",
        "...1aaaaaaaa1...",
        "...1aabbbbaa1...",
        "....1bbbbbb1....",
        ".....111111.....",
        "................",
        "................",
    ],
    "sparkle": [
        "................",
        ".......d........",
        ".......d........",
        "......d8d.......",
        "..d..d888d..d...",
        "......d8d.......",
        ".......d........",
        "...9...d....9...",
        "..989......989..",
        "...9........9...",
        "......d.........",
        ".....d8d........",
        "......d....d....",
        "..........d8d...",
        "...........d....",
        "................",
    ],
    # building preview frames: corner brackets around the tile you face (green = OK, orange = no)
    "cursor_ok": [
        "aaaa........aaaa",
        "a99..........99a",
        "a9............9a",
        "a..............a",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "a..............a",
        "a9............9a",
        "a99..........99a",
        "aaaa........aaaa",
    ],
    "cursor_bad": [
        "1111........1111",
        "1ff..........ff1",
        "1f............f1",
        "1..............1",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "1..............1",
        "1f............f1",
        "1ff..........ff1",
        "1111........1111",
    ],
}
# The main sheet is split into three files: the player, Sprout, and effects.
# player_sprite.png and fx_sprite.png share OBJ bank 0; Sprout has its own bank (7).
PLAYER_ORDER = ["down0", "down1", "up0", "up1", "side0", "side1"]
SPROUT_ORDER = ["spirit0", "spirit1"]
FX_ORDER = ["sparkle", "cursor_ok", "cursor_bad"]

# Water spirit "Ripple" and the sleep bubble, drawn with RIPPLE_PAL.
RIPPLE_SPRITES = {
    "ripple0": [
        "................",
        ".......11.......",
        "......1221......",
        ".....122221.....",
        "....12222221....",
        "...1223333221...",
        "...1233333321...",
        "..123313313321..",
        "..123333333321..",
        "..123333333321..",
        "..124333333421..",
        "...1244444421...",
        "....12444421....",
        ".....111111.....",
        "................",
        "................",
    ],
    "ripple1": [
        "................",
        "................",
        ".......11.......",
        "......1221......",
        ".....122221.....",
        "....12622221....",
        "...1263333321...",
        "..123313313321..",
        "..123333333321..",
        "..123333333321..",
        "..124333333421..",
        "...1244444421...",
        "....12444421....",
        ".....111111.....",
        "................",
        "................",
    ],
    "zzz": [
        "................",
        ".........5555...",
        "...........5....",
        "..........5.....",
        ".........5555...",
        "................",
        "....555.........",
        ".....5..........",
        "....555.........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
}
RIPPLE_ORDER = ["ripple0", "ripple1", "zzz"]

# ---- stone spirit palette (OBJ bank 3). The game builds a gray copy for sleep. ----
PEBBLE_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline / eyes
    (196, 184, 164),  # 2 stone light
    (150, 138, 122),  # 3 stone mid
    (104, 94, 86),    # 4 stone dark
    (184, 240, 138),  # 5 moss light
    (106, 196, 106),  # 6 moss mid
] + [(0, 0, 0)] * 9

# Stone spirit "Pebble": a round boulder with a moss cap, drawn with PEBBLE_PAL.
PEBBLE_SPRITES = {
    "stone0": [
        "................",
        "................",
        ".......5.5......",
        "......56565.....",
        "....11166111....",
        "...1222222221...",
        "..122222222221..",
        "..122122221221..",
        "..123122221321..",
        "..123333333321..",
        "..133333333331..",
        "..134333333431..",
        "...1444444441...",
        "....11111111....",
        "................",
        "................",
    ],
}
PEBBLE_SPRITES["stone1"] = ["." * 16] + PEBBLE_SPRITES["stone0"][:-1]   # bob frame: one pixel lower
PEBBLE_ORDER = ["stone0", "stone1"]

# ---- wind spirit palette (OBJ bank 5). The game builds a gray copy for sleep. ----
BREEZE_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline / eyes
    (244, 250, 255),  # 2 white
    (190, 228, 240),  # 3 pale cyan
    (120, 188, 210),  # 4 teal
] + [(0, 0, 0)] * 11

# Wind spirit "Breeze": a small cloud puff with a curling tail, drawn with BREEZE_PAL.
BREEZE_SPRITES = {
    "breeze0": [
        "................",
        "................",
        "......111.......",
        "....11222111....",
        "...1222222221...",
        "..122222222221..",
        ".1222122221221..",
        ".1223122221321..",
        ".1333333333331..",
        "..13333333331...",
        "...11444411.....",
        "......1111.444..",
        "...........44...",
        "................",
        "................",
        "................",
    ],
}
BREEZE_SPRITES["breeze1"] = ["." * 16] + BREEZE_SPRITES["breeze0"][:-1]
BREEZE_ORDER = ["breeze0", "breeze1"]


def make_breeze():
    return draw_sheet(BREEZE_SPRITES, BREEZE_ORDER, BREEZE_PAL)


def make_pebble():
    return draw_sheet(PEBBLE_SPRITES, PEBBLE_ORDER, PEBBLE_PAL)


def make_ripple():
    return draw_sheet(RIPPLE_SPRITES, RIPPLE_ORDER, RIPPLE_PAL)


def make_player():
    """Fallback player sheet (192x32): the original 16x16 art in tall cells, 4 frames per
    direction (stand, walk1, walk2, walk3) built from its two frames."""
    small = draw_sheet(SPRITES, PLAYER_ORDER, OBJ_PAL)
    img = Image.new("P", (16 * 12, 32), 0)
    img.putpalette(small.getpalette())
    for d in range(3):                      # down, up, side
        for k, src in enumerate((0, 1, 0, 1)):
            cell = small.crop(((d * 2 + src) * 16, 0, (d * 2 + src + 1) * 16, 16))
            img.paste(cell, ((d * 4 + k) * 16, 16))
    return img


def make_sprout():
    return draw_sheet(SPRITES, SPROUT_ORDER, OBJ_PAL)


def make_fx():
    return draw_sheet(SPRITES, FX_ORDER, OBJ_PAL)


def rename_old_sheets():
    """Older builds used other names for the sprite sheets. Rename them to <name>_sprite.png."""
    for old, new in (("sprites2.png", "ripple_sprite.png"), ("sprites3.png", "pebble_sprite.png"),
                     ("sprites4.png", "breeze_sprite.png"), ("ripple.png", "ripple_sprite.png"),
                     ("pebble.png", "pebble_sprite.png"), ("breeze.png", "breeze_sprite.png"),
                     ("player.png", "player_sprite.png"), ("sprout.png", "sprout_sprite.png"),
                     ("fx.png", "fx_sprite.png")):
        old_path, new_path = os.path.join(ART, old), os.path.join(ART, new)
        if os.path.exists(old_path) and not os.path.exists(new_path):
            os.rename(old_path, new_path)
            print("renamed", old, "->", new)


def split_old_sheet():
    """Older builds kept the player, Sprout, and the sparkle in one sprites.png.
    Cut it into player_sprite.png, sprout_sprite.png, and fx_sprite.png, keeping every pixel and the palette."""
    old_path = os.path.join(ART, "sprites.png")
    if not os.path.exists(old_path):
        return
    old = Image.open(old_path)
    start = 0
    for name, order in (("player_sprite.png", PLAYER_ORDER), ("sprout_sprite.png", SPROUT_ORDER), ("fx_sprite.png", FX_ORDER)):
        path = os.path.join(ART, name)
        if not os.path.exists(path):
            part = old.crop((start * 16, 0, (start + len(order)) * 16, 16))
            part.putpalette(old.getpalette())
            part.save(path)
            print("split", name, "from sprites.png")
        start += len(order)
    os.remove(old_path)
    print("removed", old_path)


def draw_sheet(sheet, order, pal):
    img = Image.new("P", (16 * len(order), 16))
    img.putpalette([c for rgb in pal for c in rgb])
    for i, name in enumerate(order):
        rows = sheet[name]
        assert len(rows) == 16, name
        for y, row in enumerate(rows):
            assert len(row) == 16, f"{name} row {y} has {len(row)} chars"
            for x, ch in enumerate(row):
                img.putpixel((i * 16 + x, y), 0 if ch == "." else int(ch, 16))
    return img


def append_new_cells(path, order):
    """A sprite sheet that predates newer cells in `order`: add only the missing cells on the
    right, keeping every existing pixel (and hand edits)."""
    old = Image.open(path)
    have = old.width // 16
    if have >= len(order):
        return
    new = draw_sheet(SPRITES, order, OBJ_PAL)
    out = Image.new("P", (16 * len(order), 16))
    out.putpalette(old.getpalette())
    out.paste(old, (0, 0))
    out.paste(new.crop((have * 16, 0, len(order) * 16, 16)), (have * 16, 0))
    out.save(path)
    print("appended", len(order) - have, "cells to", path)


def append_new_metatiles(path):
    """terrain.png exists but predates newer META entries: add only the missing
    tiles on the right, keeping every existing pixel (and hand edits) as is."""
    old = Image.open(path)
    have = old.width // 16
    if have >= len(META):
        return
    new = make_terrain()
    out = Image.new("P", (16 * len(META), 16))
    out.putpalette(old.getpalette())
    out.paste(old, (0, 0))
    out.paste(new.crop((have * 16, 0, len(META) * 16, 16)), (have * 16, 0))
    out.save(path)
    print("appended", len(META) - have, "metatiles to", path)


def main():
    os.makedirs(ART, exist_ok=True)
    force = "--force" in sys.argv
    if not force:
        rename_old_sheets()
        split_old_sheet()
    for name, fn in (("terrain.png", make_terrain), ("player_sprite.png", make_player),
                     ("sprout_sprite.png", make_sprout), ("fx_sprite.png", make_fx),
                     ("ripple_sprite.png", make_ripple), ("pebble_sprite.png", make_pebble),
                     ("breeze_sprite.png", make_breeze)):
        path = os.path.join(ART, name)
        if force or not os.path.exists(path):
            fn().save(path)
            print("wrote", path)
        elif name == "terrain.png":
            append_new_metatiles(path)
        elif name == "fx_sprite.png":
            append_new_cells(path, FX_ORDER)


if __name__ == "__main__":
    main()
