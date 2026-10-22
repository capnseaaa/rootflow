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
        "dam", "lever", "lever_pulled", "lost_stone", "blossom", "fountain", "statue", "pinwheel",
        "vael_pump", "vael_pump_b",
        # built homes: for each spirit, level 1 roof + base, then level 2 roof + base
        "grove1_top", "grove1_base", "grove2_top", "grove2_base",
        "spring1_top", "spring1_base", "spring2_top", "spring2_base",
        "cairn1_top", "cairn1_base", "cairn2_top", "cairn2_base",
        "nook1_top", "nook1_base", "nook2_top", "nook2_base",
        # home animation frames (never placed on the map; copied over the home tiles)
        "spring1_ring1", "spring1_ring2", "spring2_ring1", "spring2_ring2", "spring2_spray",
        "cairn1_glow", "cairn2_glow", "nook1_swing", "nook2_spin",
        # v0.13: stumps (dead land, grass) and chests / relics on grass
        "stump", "stump_grass", "chest_grass", "chest_open_grass", "wreck_grass"]
# v0.14: the Great Tree's trunk (6 x 6 metatiles, row by row), its roots, and Bark's quest pieces
# (the trunk itself is drawn on its own layer, art/tree_trunk.png; under it the map has solid ground)
META += ["trunk_block", "root_tip_dead", "root_tip_alive", "clamp", "dry_pool"]

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


def stump(rng, grassy=False):
    """A cut tree stump seen from above: bark rim, pale rings, roots. On dead land, or on grass
    (what Bark leaves when it cuts a living tree)."""
    px = grass(rng) if grassy else dead(rng)
    for y in range(4, 15):                                   # the trunk's side, below the cut
        for x in range(3, 13):
            d = ((x - 7.5) / 4.8) ** 2
            if y >= 8 and d < 1 and y < 13 - int(d * 2):
                px[y][x] = 12 if (x + y) % 3 else 15
    for y in range(16):                                      # the cut face
        for x in range(16):
            d = ((x - 7.5) / 4.8) ** 2 + ((y - 7.0) / 3.2) ** 2
            if d < 1:
                px[y][x] = 15 if d > 0.78 else 12 if d > 0.6 else 5 if d > 0.3 else 4 if d > 0.12 else 5
    for x, y in ((2, 12), (13, 12), (3, 13), (12, 13)):      # roots
        px[y][x] = 12
    return px


# ---- the Great Tree (v0.14) ----
def root(rng, kind):
    """A thick root lying on dead ground: h(orizontal), v(ertical), d1 (top-left to bottom-right),
    d2 (top-right to bottom-left). Roots can be walked on."""
    px = dead(rng)
    for y in range(16):
        for x in range(16):
            if kind == "h":
                d = abs(y - 8)
            elif kind == "v":
                d = abs(x - 7.5)
            elif kind == "d1":
                d = abs(x - y) * 0.71
            else:
                d = abs(x + y - 15) * 0.71
            if d < 5.6:
                px[y][x] = 15 if d > 4.6 else (4 if d < 1.4 and (x + y) % 4 else 12 if d < 3.6 else 1)
    return px


def root_tip(rng, alive):
    """A root coming down from the top and ending: withered and gray, or alive with a green shoot."""
    px = dead(rng)
    for y in range(16):
        for x in range(16):
            w = 5.6 - max(0, y - 4) * 0.5
            d = abs(x - 7.5)
            if y < 14 and d < w:
                px[y][x] = 15 if d > w - 1 else (12 if alive else 2)
    if alive:
        dots(px, [(7, 13), (8, 13), (8, 14), (6, 12), (9, 12)], 7)
        dots(px, [(5, 11), (10, 11), (4, 10), (11, 10), (7, 15)], 8)
        dots(px, [(5, 12), (10, 12)], 6)
    else:
        dots(px, [(7, 13), (8, 14)], 1)
    return px


def clamp(rng):
    """A Vael stone-and-iron clamp bolted over a root (Pebble breaks it)."""
    px = root(rng, "h")
    for y in range(3, 14):
        for x in range(4, 12):
            edge = y in (3, 13) or x in (4, 11)
            px[y][x] = 15 if edge else (3 if (x + y) % 3 else 2)
    for x in range(5, 11):
        px[7][x] = 13 if x % 2 else 1                          # a rusty band
    dots(px, [(5, 4), (10, 4), (5, 12), (10, 12)], 14)          # bolts
    return px


def dry_pool(rng):
    """The Great Tree's drinking pool, dried to cracked brown mud (Ripple fills it)."""
    px = blank(4)
    speckle(px, rng, [5, 12], 20)
    for y in range(16):                                     # cracks
        x = (y * 5 + 3) % 16
        px[y][x] = 12
    for x in range(16):
        px[(x * 7 + 5) % 16][x] = 12
    for x, y in ((3, 3), (11, 6), (6, 12)):                 # dried-up mud plates' dark edges
        px[y][x] = 15
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


def chest(rng, open_=False, ground=None):
    """ground: the tile drawn under it (dead land; grass once a bloom reaches it)."""
    px = (ground or dead)(rng)
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


def wreck(rng, ground=None):
    px = (ground or dead)(rng)
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


def vael_pump(rng, frame):
    """A running Vael pump on dried ground: a dark riveted tank, a pipe sunk into the earth,
    and a red light that blinks between the two frames (the game swaps them like water)."""
    px = dead(rng)
    for y in range(4, 15):
        for x in range(3, 13):
            edge = y in (4, 14) or x in (3, 12)
            px[y][x] = 15 if edge else (1 if (x + y) % 3 else 2)
    for x in range(4, 12):
        px[9][x] = 12                       # band
    for x, y in ((4, 6), (11, 6), (4, 12), (11, 12)):
        px[y][x] = 14                       # rivets
    for y in range(0, 5):                   # exhaust pipe
        px[y][9] = 15; px[y][10] = 2
    px[0][8] = px[0][11] = 15
    px[6][7] = px[6][8] = 13 if frame == 0 else 12   # the warning light
    px[7][7] = px[7][8] = 13 if frame == 0 else 15
    piston = 11 if frame == 0 else 12
    for x in range(5, 11):
        px[piston][x] = 3                   # moving piston line
    for x in (2, 13):                       # drained cracks spreading from the base
        px[14][x] = px[15][x] = 1
    return px


# ---- built homes (two tiles tall: a roof above, a base where the spirit sits) ----
def home_pad(rng):
    """The shared base texture: a round mossy pad with a cream doorstep in the middle."""
    px = grass(rng)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 7.5) ** 2 + ((y - 8.5) / 6.5) ** 2
            if d < 1:
                px[y][x] = 6 if d > 0.6 else 7
            if 0.85 <= d < 1:
                px[y][x] = 15
            if ((x - 7.5) / 4) ** 2 + ((y - 9.5) / 3) ** 2 < 1:
                px[y][x] = 14 if y < 10 else 5            # doorstep
    return px


def dots(px, pts, c):
    for x, y in pts:
        if 0 <= x < 16 and 0 <= y < 16:
            px[y][x] = c


def blob(px, cx, cy, rx, ry, fill, edge=15):
    for y in range(16):
        for x in range(16):
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if d < 1:
                px[y][x] = fill
            elif d < 1.35 and edge is not None and px[y][x] in (6, 7, 8, 2, 3, 4):
                pass
    for y in range(16):
        for x in range(16):
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if 0.82 <= d < 1 and edge is not None:
                px[y][x] = edge


def grove_top(rng, level):
    px = grass(rng)
    if level == 1:      # a leafy bush arch
        blob(px, 7.5, 9, 7, 6, 6)
        dots(px, [(4, 7), (6, 5), (10, 6), (11, 9), (5, 10), (8, 6), (9, 9), (3, 9)], 8)
        for y in range(11, 16):
            px[y][5] = px[y][10] = 12                 # arch sides
        for y in range(12, 16):
            for x in range(6, 10):
                px[y][x] = 6
    else:               # the tree house's canopy: full, round, in bloom
        blob(px, 7.5, 8.5, 7.5, 7.5, 6)
        for y in range(16):
            for x in range(16):
                if ((x - 6) / 5) ** 2 + ((y - 6.5) / 4.5) ** 2 < 1:
                    px[y][x] = 7                        # lit side
                if ((x - 5) / 2.5) ** 2 + ((y - 4.5) / 2) ** 2 < 1:
                    px[y][x] = 8                        # highlight
        for x, y in ((3, 8), (9, 3), (12, 7), (6, 11), (10, 12), (4, 4), (12, 11)):
            dots(px, [(x, y), (x + 1, y), (x, y + 1)], 13)
            dots(px, [(x + 1, y + 1)], 14)               # blossoms
        for y in range(13, 16):
            for x in range(5, 11):
                px[y][x] = 12                           # the trunk starts
        px[15][5] = px[15][10] = 15
    return px


def grove_base(rng, level):
    if level == 1:
        px = home_pad(rng)
        dots(px, [(2, 9), (13, 9), (3, 12), (12, 12)], 8)          # sprouts
        return px
    px = grass(rng)     # the tree house's trunk: bark, a round door, roots
    for y in range(0, 13):
        for x in range(4, 12):
            px[y][x] = 12 if (x + y // 3) % 4 else 4               # bark
        px[y][4] = px[y][11] = 15
    for y in range(13, 16):                                        # roots spreading out
        for x, dx in ((4, -1), (11, 1)):
            xx = x + dx * (y - 12)
            if 0 <= xx < 16:
                px[y][xx] = 12
                if 0 <= xx - dx < 16:
                    px[y][xx - dx] = 15
        for x in range(5, 11):
            px[y][x] = 12 if y < 15 else 15
    for y in range(2, 14):                                         # a round hole with a frame; Sprout
        for x in range(4, 12):                                     # peeks out of it (build_data cuts the
            d = ((x - 7.5) / 3.4) ** 2 + ((y - 7.5) / 4.4) ** 2    # peek frames to its dark pixels)
            if d < 1:
                px[y][x] = 15 if d < 0.6 else 5
    dots(px, [(2, 12), (13, 12), (1, 14), (14, 14)], 13)           # flowers by the roots
    dots(px, [(2, 13), (13, 13)], 6)
    return px


def spring_top(rng, level, spray=False):
    """A water-drop dome standing on a pale stone plinth. Level 2: the drop is taller, sprays a
    jet of water from its tip, and has arched windows."""
    px = water(rng, 0)
    tip, cy, r = (2, 9.0, 5.4) if level == 1 else (2, 8.8, 5.9)
    neck = cy - r * 0.6                                     # where the point meets the round body

    def width(y):
        if y < tip:
            return -1.0
        if y < neck:
            return (y - tip) / max(1.0, neck - tip) * r * 0.8   # the drop's point
        return max(0.0, r * r - (y - cy) ** 2) ** 0.5           # its round body

    for y in range(12, 16):                                     # the plinth: a rounded stone ring
        half = (6.5, 7.5, 7.5, 7.0)[y - 12]
        for x in range(16):
            d = abs(x - 7.5)
            if d <= half:
                edge = d > half - 1 or y == 12
                px[y][x] = 15 if edge else (3 if y == 15 or (x * 5 + y * 3) % 7 == 0 else 14)
    for y in range(15):
        for x in range(16):
            w = width(y)
            d = abs(x - 7.5)
            if w >= 0 and d <= w:
                px[y][x] = 15 if (d > w - 1 or y == tip) else (11 if x < 7.5 - w + 2.5 else 10)
    for y in range(int(neck), 14):                              # ripples on the dome
        for x in range(6, 10):
            if px[y][x] == 10 and (x + y) % 5 == 0:
                px[y][x] = 11
    for y, xs in ((11, (7, 8)), (12, (6, 7, 8, 9)), (13, (6, 7, 8, 9)), (14, (6, 7, 8, 9)), (15, (6, 7, 8, 9))):
        for x in xs:                                            # an arched doorway
            px[y][x] = 9
    dots(px, [(6, 11), (9, 11), (5, 12), (10, 12)], 15)
    if level == 2:
        for wx in (4, 11):                                      # arched windows
            dots(px, [(wx, 8), (wx, 9)], 9)
        if spray:   # the animation frame when Ripple comes up: a jet falling in two arcs
            dots(px, [(7, 0), (8, 0), (7, 1), (8, 1)], 14)
            dots(px, [(6, 0), (9, 0), (5, 1), (10, 1), (4, 2), (11, 2), (3, 3), (12, 3)], 11)
            dots(px, [(2, 4), (13, 4), (2, 5), (13, 5), (1, 7), (14, 7)], 11)
            dots(px, [(1, 8), (14, 8)], 14)
        else:       # otherwise a small spurt at the tip
            dots(px, [(7, 0), (8, 0)], 11)
            dots(px, [(7, 1), (8, 1)], 14)
    return px


def spring_base(rng, level, ring=0):
    """ring 1 or 2: animation frames with a ripple ring spreading where Ripple dives or comes up."""
    px = grass(rng)
    # a round stone basin rim around a pool (both levels)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 7.2) ** 2 + ((y - 8) / 6.8) ** 2
            if d < 1:
                px[y][x] = 14 if d > 0.7 else (10 if d > 0.35 else 9)
            if 0.88 <= d < 1:
                px[y][x] = 15
    dots(px, [(5, 6), (6, 6), (9, 10)], 11)                    # ripples
    if ring:
        lo, hi = ((0.06, 0.2), (0.36, 0.52))[ring - 1]
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) / 7.2) ** 2 + ((y - 8) / 6.8) ** 2
                if lo <= d < hi:
                    px[y][x] = 11
    if level == 2:
        for cx, cy in ((4, 9), (11, 6), (10, 11)):              # lily pads
            dots(px, [(cx, cy), (cx + 1, cy), (cx, cy + 1), (cx + 1, cy + 1)], 7)
            dots(px, [(cx + 1, cy)], 6)
        dots(px, [(7, 7), (8, 7), (7, 8), (8, 8)], 13)          # a lotus
        dots(px, [(7, 6), (8, 9)], 14)
    return px


def cairn_top(rng, level, glow=False):
    """Rounded stones stacked smaller toward the top; level 2 is taller and mossy. A rune is carved
    in a stone; glow = the animation frame when Pebble lands on top and the rune lights up."""
    px = grass(rng)
    stones = [(7.5, 12.5, 5.5, 3.0), (7.5, 8.0, 4.0, 2.4)]
    if level == 2:
        stones += [(7.5, 4.4, 3.0, 2.0), (7.5, 1.6, 1.8, 1.3)]
    for cx, cy, rx, ry in stones:
        for y in range(16):
            for x in range(16):
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d < 1:
                    px[y][x] = 2 if y > cy + ry * 0.4 else 3
                if 0.72 <= d < 1:
                    px[y][x] = 15
        hx, hy = int(cx - rx * 0.4), int(cy - ry * 0.35)
        px[hy][hx] = 14                                      # highlight
    ry = 12 if level == 1 else 8
    if glow:
        dots(px, [(7, ry), (8, ry), (7, ry + 1), (8, ry + 1)], 14)
        dots(px, [(6, ry), (9, ry), (7, ry - 1), (8, ry + 2)], 11)
    else:
        dots(px, [(7, ry), (8, ry + 1)], 2)                  # the rune, dark until Pebble lands
    if level == 2:
        dots(px, [(6, 1), (9, 4), (5, 4), (10, 7)], 8)       # moss
    return px


def cairn_base(rng, level):
    px = home_pad(rng)
    dots(px, [(2, 8), (13, 8), (3, 13), (12, 13)], 3)          # ring of small stones
    dots(px, [(2, 9), (13, 9), (3, 14), (12, 14)], 15)
    if level == 2:
        dots(px, [(1, 11), (14, 11)], 3)
        dots(px, [(1, 12), (14, 12)], 15)
        dots(px, [(2, 7), (13, 7)], 8)
    return px


def nook_top(rng, level, frame=0):
    """Level 1: a wind-chime post (frame 1: the chimes swing when Breeze puffs). Level 2: a little
    windmill with Breeze's face as the hub (frame 1: the blades turned 45 degrees)."""
    px = grass(rng)
    for y in range(2, 16):                                     # wooden post
        px[y][7] = 12; px[y][8] = 5
    if level == 1:                                             # wind-chime and ribbons
        for x in range(4, 12):
            px[3][x] = 12
        for x, ln in ((4, 5), (6, 7), (9, 6), (11, 4)):
            for k in range(ln):
                xx = x + (k + 1) // 2 if frame else x                 # swinging: the chimes lean
                px[4 + k][xx] = 14 if k < ln - 1 else 13
    else:                                                      # a little windmill
        if frame == 0:
            blades = [((9, 2), (10, 1), (11, 0)), ((6, 2), (5, 1), (4, 0)),
                      ((10, 8), (11, 9), (12, 10)), ((5, 8), (4, 9), (3, 10))]
        else:
            blades = [((7, 1), (7, 0), (8, 0)), ((12, 5), (13, 5), (14, 5)),
                      ((8, 10), (8, 11), (7, 11)), ((3, 5), (2, 5), (1, 5))]
        for i, blade in enumerate(blades):
            for x, y in blade:
                px[y][x] = 14 if i % 2 else 13
                if y + 1 < 16 and px[y + 1][x] not in (13, 14):
                    px[y + 1][x] = 15
        hub = {3: (6, 9), 4: (5, 10), 5: (4, 11), 6: (4, 11), 7: (5, 10), 8: (6, 9)}
        for y, (x0, x1) in hub.items():                        # Breeze's cloud face as the hub
            for x in range(x0, x1 + 1):
                edge = x in (x0, x1) or y in (3, 8)
                px[y][x] = 15 if edge else 14
        dots(px, [(6, 5), (9, 5)], 15)                         # eyes
        dots(px, [(7, 7), (8, 7)], 15)                         # a little mouth
    return px


def nook_base(rng, level):
    px = home_pad(rng)
    dots(px, [(2, 8), (13, 8)], 12)                            # two ribbon posts
    dots(px, [(2, 7), (13, 7)], 13 if level == 1 else 14)
    if level == 2:
        dots(px, [(1, 9), (14, 9), (1, 10), (14, 10)], 14)     # streamers
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
        statue(rng), pinwheel(rng), vael_pump(rng, 0), vael_pump(rng, 1),
    ]
    states = {}                     # random state before each home tile, by tile index
    for top, base in ((grove_top, grove_base), (spring_top, spring_base),
                      (cairn_top, cairn_base), (nook_top, nook_base)):
        for level in (1, 2):
            for fn in (top, base):
                states[len(tiles)] = rng.getstate()
                tiles.append(fn(rng, level))
    # Animation frames for the homes; main.c copies them over a home's tiles (animate_homes).
    # Each uses the same random state as its home tile, so the grass or water around it matches.
    def same(i):
        r = random.Random()
        r.setstate(states[i])
        return r
    tiles += [
        spring_base(same(45), 1, ring=1), spring_base(same(45), 1, ring=2),
        spring_base(same(47), 2, ring=1), spring_base(same(47), 2, ring=2),
        spring_top(same(46), 2, spray=True),
        cairn_top(same(48), 1, glow=True), cairn_top(same(50), 2, glow=True),
        nook_top(same(52), 1, frame=1), nook_top(same(54), 2, frame=1),
    ]
    tiles += [stump(rng), stump(rng, grassy=True), chest(rng, ground=grass), chest(rng, True, ground=grass),
              wreck(rng, ground=grass)]
    tiles += [dead(rng), root_tip(rng, False), root_tip(rng, True), dead(rng), dry_pool(rng)]   # clamp: a sprite
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


# ---- wood spirit palette (OBJ bank 9). The game builds a gray copy for sleep. ----
BARK_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline / eyes
    (196, 142, 88),   # 2 cut wood (pale)
    (150, 98, 56),    # 3 bark light
    (104, 64, 36),    # 4 bark dark
    (120, 200, 96),   # 5 leaf
    (236, 196, 140),  # 6 rings / cheeks
] + [(0, 0, 0)] * 9

# Wood spirit "Bark": a grumpy little stump with a wilted leaf sprout, drawn with BARK_PAL.
BARK_SPRITES = {     # grumpy: brows slanting down to the middle, a squint, and a frown
    "bark0": [
        "................",
        "..........5.....",
        "........5551....",
        ".....1111511....",
        "....12266221....",
        "...1262222621...",
        "...1226666221...",
        "...1133333311...",
        "...1311331131...",
        "...1313333131...",
        "...1333333331...",
        "...1333113331...",
        "...1431331341...",
        "..14411441144...",
        "...11..11..11...",
        "................",
    ],
}
BARK_SPRITES["bark1"] = ["." * 16] + BARK_SPRITES["bark0"][:-1]   # bob frame: one pixel lower
BARK_ORDER = ["bark0", "bark1"]


# ---- the Great Tree (v0.14): two 512 x 512 layers over its whole region ----
# tree_trunk.png (under the characters): a huge trunk, ~240 px wide, shaded like a cylinder
#   (lit from the left, grooves bunching toward the edges) so it stands upright, with a flared,
#   curved base and a dark opening at its foot. Its bark repeats every 32 px down the straight
#   part, so most tiles repeat.
# canopy.png (over the characters, mostly solid): thick limbs, tapered and shaded (light on top),
#   spreading over the top and down both sides. Strokes run at slopes 0, 1/2, 1, 2 from points on
#   the 8-px grid, so long runs repeat. Both layers share TREE_PAL; color 0 is clear.
TREE_PAL = [
    (255, 0, 255),    # 0 clear
    (24, 18, 16),     # 1 outline
    (58, 40, 32),     # 2 bark dark
    (84, 60, 46),     # 3 bark mid
    (112, 84, 62),    # 4 bark light
    (38, 26, 22),     # 5 groove
    (146, 116, 88),   # 6 highlight
    (8, 6, 6),        # 7 the opening
    (188, 178, 138),  # 8 fungus, pale
    (132, 122, 92),   # 9 fungus, shadow
    (58, 30, 50),     # 10 rot, deep
    (112, 66, 98),    # 11 rot, ooze
    (170, 148, 112),  # 12 dead wood (peeled bark)
    (96, 104, 74),    # 13 sickly moss
] + [(0, 0, 0)] * 2
TREE_CX, TRUNK_TOP, TRUNK_HALF, BASE_Y = 256, 40, 120, 262   # the straight part: y TRUNK_TOP..BASE_Y
CANOPY_CENTER = (256, 48)


def trunk_half(y):
    """Half-width of the trunk at row y: straight, then a flared base."""
    if y <= BASE_Y:
        return TRUNK_HALF
    return TRUNK_HALF + ((y - BASE_Y) / 64) ** 2 * 84


def trunk_bottom(x):
    """The curved bottom edge of the base (an ellipse seen from above at an angle)."""
    dx = (x - TREE_CX) / (TRUNK_HALF + 84)
    return BASE_Y + 46 + 26 * max(0.0, 1 - dx * dx) ** 0.5


def bark_color(x, y):
    """Bark on a cylinder of radius TRUNK_HALF: tone from the light, grooves from the angle.
    Depends on y only through y % 32, so the straight trunk repeats."""
    import math
    u = max(-1.0, min(1.0, (x - TREE_CX) / TRUNK_HALF))
    a = math.asin(u)                                  # angle around the trunk
    light = 0.62 - 0.42 * u - 0.18 * u * u
    col = a * 9 / math.pi + 9                         # grooves, closer together toward the edges
    g, ci = col % 1, int(col)
    yy = y % 32
    if g < 0.14 or (g < 0.22 and (yy + ci * 7) % 11 < 3):
        return 5                                        # a groove, ragged at the edges
    if abs(u) > 0.97:
        return 1
    cy = (ci * 13) % 32                                 # one short, slanted crack per plate
    if 0.3 < g < 0.7 and yy == (cy + int(g * 4)) % 32:
        return 2
    if g > 0.78:
        light -= 0.18                                   # each plate darkens toward its groove
    if light > 0.86 and (yy + ci) % 4:
        return 6
    return 4 if light > 0.58 else 3 if light > 0.32 else 2


# Roots (left half; the right half is its mirror image): grid points from the base outward.
# Each: (points, width at the base, width at the end). Ends that point down get a root tip tile
# on the map (ROOT_TIPS); the clamps sit on the first root (CLAMP_SPOTS, in pixels).
def root_path(x, y, moves):
    """Grid points from (x, y) by moves (dx, dy, steps): each step is (dx, dy) * 8 pixels."""
    pts = [(x, y)]
    for dx, dy, n in moves:
        x, y = x + dx * 8 * n, y + dy * 8 * n
        pts.append((x, y))
    return pts


ROOTS = [   # winding: each turns between neighboring slopes
    (root_path(104, 296, [(-2, 1, 1), (-2, 0, 2), (-2, -1, 1), (-2, 0, 2), (-2, 1, 2)]), 36, 18),
    (root_path(136, 320, [(-1, 1, 2), (-1, 2, 1), (0, 2, 1), (-1, 2, 1), (-1, 1, 2), (0, 2, 2), (-1, 2, 1),
                          (0, 2, 1)]), 36, 12),
    (root_path(192, 328, [(0, 2, 1), (1, 2, 1), (0, 2, 2), (-1, 2, 1), (0, 2, 3)]), 30, 12),
]
CLAMP_SPOTS = [(64, 304), (24, 296)]           # left side, on the first root; mirrored for the right


def mirror_x(x):
    return 511 - x


def stroke(px, pts, w0, w1, colors, S=512):
    """A thick path through grid points, tapering from w0 to w1, shaded light on top. colors:
    (outline, dark, mid, light, highlight)."""
    out, dark, mid, light, hi = colors
    segs = list(zip(pts, pts[1:]))
    total = sum(max(abs(b[0] - a[0]), abs(b[1] - a[1])) for a, b in segs) or 1
    for outline in (True, False):
        done = 0
        for (x0, y0), (x1, y1) in segs:
            n = max(abs(x1 - x0), abs(y1 - y0)) or 1
            for t in range(n + 1):
                x = x0 + (x1 - x0) * t // n
                y = y0 + (y1 - y0) * t // n
                w = w0 + (w1 - w0) * done // total // 2 * 2       # constant along a segment: tiles repeat
                r = w / 2 + (1 if outline else 0)
                ir = int(r) + 1
                for yy in range(max(0, y - ir), min(S, y + ir + 1)):
                    for xx in range(max(0, x - ir), min(S, x + ir + 1)):
                        d2 = (xx - x) ** 2 + (yy - y) ** 2
                        if d2 > r * r:
                            continue
                        if outline:
                            if px[yy][xx] == 0:
                                px[yy][xx] = out
                        else:
                            v = (yy - y) / max(1.0, r)
                            px[yy][xx] = hi if v < -0.62 else light if v < -0.15 else mid if v < 0.45 else dark
            done += n


def blot(px, cx, cy, rx, ry, fill, edge):
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            if 0 <= x < 512 and 0 <= y < 512 and px[y][x]:
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d < 1:
                    px[y][x] = edge if d > 0.7 else fill


def shelf(px, cx, cy, w):
    """A bracket fungus: a pale half-disc sticking out of the bark, shadowed underneath."""
    for y in range(cy - w // 2, cy + 3):
        for x in range(cx - w, cx + w + 1):
            d = ((x - cx) / w) ** 2 + ((y - cy) / (w * 0.55)) ** 2
            if d < 1 and y <= cy:
                px[y][x] = 1 if d > 0.82 else 8 if y < cy - 1 else 9
        if y > cy:
            for x in range(cx - w + 2, cx + w - 1):
                if 0 <= x < 512:
                    px[y][x] = 5


def make_trunk():
    S = 512
    px = [[0] * S for _ in range(S)]
    rootc = (1, 2, 3, 4, 6)
    for pts, w0, w1 in ROOTS:                            # roots first, under the trunk's base
        stroke(px, pts, w0, w1, rootc)
    for y in range(S):                                   # the right half mirrors the left
        for x in range(256):
            px[y][mirror_x(x)] = px[y][x]
    for y in range(TRUNK_TOP, S):
        h = trunk_half(y)
        for x in range(S):
            d = abs(x - TREE_CX)
            if d > h or y > trunk_bottom(x):
                continue
            c = bark_color(x, y)
            if d > TRUNK_HALF:                            # the flare: buttresses running out
                k = int((d - TRUNK_HALF) * 0.45 + (y - BASE_Y) * 0.25) % 6
                c = 1 if k == 0 else 5 if k == 1 else 2 if x > TREE_CX else 3
            if d > h - 1.5 or y > trunk_bottom(x) - 1.5:
                c = 1
            px[y][x] = c
    # disease: peeled strips of dead wood, rot holes oozing, bracket fungus, sickly moss
    for x0, y0, w, hgt in ((170, 128, 8, 32),):
        for y in range(y0, y0 + hgt):
            for x in range(x0 + (y - y0) // 9, x0 + w + (y - y0) // 9):
                if px[y][x]:
                    px[y][x] = 12 if (x + y) % 7 else 9
    for cx, cy, rx, ry in ((214, 150, 10, 14), (322, 236, 8, 11)):
        blot(px, cx, cy, rx + 3, ry + 3, 11, 1)
        blot(px, cx, cy, rx, ry, 10, 10)
        for y in range(int(cy + ry), int(cy + ry + 14)):    # ooze running down
            if px[y][cx]:
                px[y][cx] = px[y][cx + 1] = 11
    for cx, cy, w in ((168, 104, 12), (288, 176, 12)):
        shelf(px, cx, cy, w)
    for y in range(BASE_Y - 30, S):                      # the opening at its foot
        for x in range(TREE_CX - 26, TREE_CX + 26):
            dx = (x - TREE_CX) / 22
            top = BASE_Y - 26 + 26 * (1 - max(0.0, 1 - dx * dx) ** 0.5) * 2
            if abs(dx) <= 1 and top <= y < trunk_bottom(x) - 2:
                px[y][x] = 7 if abs(dx) < 0.85 and y > top + 2 else 1
    img = Image.new("P", (S, S), 0)
    img.putpalette([c for rgb in TREE_PAL for c in rgb])
    img.putdata([v for row in px for v in row])
    return img


def make_clamp():
    """A Vael iron clamp (16 x 32 sprite) that sits across a root; Pebble breaks it."""
    pal = [(255, 0, 255), (24, 22, 24), (84, 88, 96), (128, 132, 140), (176, 180, 186), (150, 72, 40),
           (214, 120, 60)] + [(0, 0, 0)] * 9
    img = Image.new("P", (16, 32), 0)
    img.putpalette([c for rgb in pal for c in rgb])
    for y in range(2, 30):
        for x in range(3, 13):
            edge = x in (3, 12) or y in (2, 29)
            c = 1 if edge else 4 if x == 5 else 3 if x < 9 else 2
            if not edge and (x * 3 + y * 5) % 11 == 0:
                c = 5                                     # rust
            img.putpixel((x, y), c)
    for y in (6, 16, 25):
        for x in (6, 9):
            img.putpixel((x, y), 6)                       # rivets
    return img


def make_canopy():
    """Five thick limbs from the crown, out over the top and down the sides, each with side
    branches and twigs; tapered and top-lit. Only the left half is drawn; the right half is its
    mirror image (the GBA reuses flipped tiles for free). Limbs never cross the trunk's face.
    A little disease: pale fungus on the big limbs."""
    rng = random.Random(2424)
    S = 512
    px = [[0] * S for _ in range(S)]
    DIRS = [(2, 0), (2, 1), (1, 1), (1, 2), (0, 2), (-1, 2), (-1, 1), (-2, 1),
            (-2, 0), (-2, -1), (-1, -1), (-1, -2), (0, -2), (1, -2), (1, -1), (2, -1)]
    WIDTHS = [40, 26, 16, 8]
    paths = []

    def over_trunk(x, y):
        return x > TREE_CX - TRUNK_HALF - 4 and y > 88

    def limb(x, y, di, wi, runs, kids):
        pts = [(x, y)]
        for r, run in enumerate(runs):
            dx, dy = DIRS[di]
            for _ in range(run):
                nx, ny = x + dx * 8, y + dy * 8
                if over_trunk(nx, ny) or nx < -32 or ny < -32 or ny > S + 32:
                    paths.append((pts, WIDTHS[wi], WIDTHS[min(3, wi + 1)]))
                    return
                x, y = nx, ny
                pts.append((x, y))
            if kids and wi < 3:
                kids -= 1
                side = (di + rng.choice((-2, 2))) % 16
                limb(x, y, side, wi + 1, [rng.randint(2, 4), rng.randint(2, 3)], 1 if wi < 1 else 0)
            di = (di + (1 if r % 2 == 0 else -1)) % 16       # wind back and forth
        paths.append((pts, WIDTHS[wi], WIDTHS[min(3, wi + 1)]))

    cx, cy = CANOPY_CENTER
    edge = TREE_CX - TRUNK_HALF                          # 136: the trunk's left side
    limb(edge, cy + 8, 8, 0, [4, 4, 4, 4], 3)            # out to the left edge
    limb(edge + 16, cy - 16, 10, 0, [3, 3, 3], 2)        # up-left to the corner
    limb(cx - 40, cy - 24, 12, 0, [2, 2], 1)             # straight up
    limb(edge - 8, cy + 40, 7, 0, [4, 3, 4, 4], 3)       # down and out the left side
    limb(edge - 24, cy + 56, 5, 0, [3, 4, 4, 4], 2)      # down the far left, toward the roots
    paths.sort(key=lambda p: -p[1])                      # thick limbs first, thin ones on top
    colors = (1, 2, 3, 4, 6)
    for x0, y0, x1, y1 in ((cx, cy + 32, edge, cy + 8), (cx, cy + 32, edge + 16, cy - 16),
                           (cx, cy + 32, cx - 40, cy - 24)):   # the crown: limb bases fanning out
        stroke(px, [(x0, y0), (x1, y1)], 48, 48, colors)
    for pts, w0, w1 in paths:
        stroke(px, pts, w0, w1, colors)
    for pts, w0, w1 in paths:                            # disease along the big limbs
        if w0 >= 26:
            for x, y in pts[2::4]:
                if 0 <= x < 256 and 0 <= y < S and px[y][x]:
                    blot(px, x, y - 3, 5, 3, 8, 9)
    for y in range(S):
        for x in range(256):
            px[y][mirror_x(x)] = px[y][x]
    img = Image.new("P", (S, S), 0)
    img.putpalette([c for rgb in TREE_PAL for c in rgb])
    img.putdata([v for row in px for v in row])
    return img


def make_bark():
    return draw_sheet(BARK_SPRITES, BARK_ORDER, BARK_PAL)


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
                     ("breeze_sprite.png", make_breeze), ("bark_sprite.png", make_bark),
                     ("canopy.png", make_canopy), ("tree_trunk.png", make_trunk),
                     ("clamp_sprite.png", make_clamp)):
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
