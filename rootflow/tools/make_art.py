"""Draw the toy's pixel art and save it as indexed PNGs in art/.

The PNGs are the source of truth after the first run: edit them in any
pixel editor (keep the 16-color palette order), then rebuild.
Run with --force to regenerate them from this script.
"""
import os
import math
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
META += ["trunk_grass"]   # v0.16.3: the healed trunk's solid cells look like grass (its base)
# v0.17: the Leafen's gifts, six small things to build (on grass)
META += ["mush_lamp", "leaf_swing", "acorn_bench", "flower_arch", "pebble_path", "dew_pond"]
META += ["rock_grass"]   # v0.17.3: a rock with grass grown up round it (blooms still stop at rocks)
# v0.18: the drowned marsh (its ground is the dead ground, tinted muddy by the marsh's palette)
META += ["sludge", "lily_pad", "reeds", "vael_pipe"]
# v0.18.1: the marsh's own pieces. They are loaded into video memory only in the marsh (into the Great
# Tree's tile slots, which the marsh does not use). Bog water and its pad use BG bank 6 (own colors).
MARSH_META = ["bog_water", "bog_water_b", "bog_pad", "moss", "mushrooms", "swamp_tree", "swamp_tree_alive", "log",
              "swamp_top", "swamp_top_alive"] + [f"bog_edge_{k}" for k in range(1, 16)] + \
             [f"water_edge_{k}" for k in range(1, 16)]   # v0.18.1: shores for clear water too (Caleb)
# bog_edge_k: bog water with a ragged mossy shore on each side where land is (k bits: 1 north, 2 east,
# 4 south, 8 west). The map holds BOG_WATER; the game picks the edge by the neighbors when it draws.
META += MARSH_META

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
    """Where a root of the Great Tree ends (v0.16.7, Caleb: drawn root ends did not link with the
    roots above, so none is drawn): dead ground while it sleeps, grass with fresh shoots once awake."""
    if not alive:
        return dead(rng)
    px = grass(rng, tufts=1)
    dots(px, [(4, 9), (5, 8), (10, 6), (11, 5), (7, 12), (8, 11)], 8)   # shoots
    dots(px, [(4, 10), (10, 7), (7, 13)], 6)
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


def rock(rng, on_grass=False):
    px = grass(rng) if on_grass else dead(rng)
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


def sludge(rng):
    """Vael's sludge: thick gray water with slow bubbles (dead colors, tinted by the area)."""
    px = blank(1)
    for y in range(16):
        for x in range(16):
            if (x * 2 + y * 3) % 13 == 0:
                px[y][x] = 2
            if (x + y * 2) % 17 == 0:
                px[y][x] = 15
    for _ in range(3):                                # bubbles
        x, y = rng.randrange(2, 13), rng.randrange(2, 13)
        px[y][x] = px[y][x + 1] = 3
        px[y + 1][x] = px[y + 1][x + 1] = 2
    return px


def lily_pad(rng):
    """A lily pad on clean water (walkable): a round green pad with a notch and a little flower."""
    px = water(rng, 0)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 6.5) ** 2 + ((y - 8) / 5.5) ** 2
            notch = x > 8 and y < 8 and abs((y - 8) + (x - 8)) < 1.2      # a thin cut toward the top right
            if d < 1 and not notch:
                px[y][x] = 15 if d > 0.78 else 8 if x + y < 13 else 7 if d < 0.5 else 6
    px[6][5] = px[5][6] = px[6][6] = 14
    px[5][5] = 13
    return px


def reeds(rng):
    """Dead cattails standing in the mud (solid; YOU cuts them): thin pale stems, brown heads."""
    px = dead(rng)
    for x0 in (2, 5, 8, 11, 13):
        h = rng.randrange(8, 13)
        lean = rng.choice((-1, 0, 1))
        for k in range(h):
            x = x0 + (lean if k > h * 0.6 else 0)
            px[15 - k][x] = 15 if k < 1 else 3
        top = 15 - h
        x = x0 + lean
        for k in range(3):                            # the cattail's brown head
            if 0 <= top + k < 16:
                px[top + k][x] = 12
        if top - 1 >= 0:
            px[top - 1][x] = 3
    return px


def bog_water(rng, phase):
    """Dark bog water (BG bank 6): deep green-black with slow ripples and a few pale glints."""
    px = blank(1)
    for y in range(16):
        for x in range(16):
            if (x * 3 + y * 5 + phase * 7) % 23 == 0 or (x - y * 2 + phase * 4) % 19 == 0:
                px[y][x] = 2
    r = random.Random(77 + phase)
    for _ in range(3):
        x, y = r.randrange(1, 14), r.randrange(16)
        px[y][x] = px[y][x + 1] = 3
    return px


def bog_pad(rng):
    """A lily pad on bog water (BG bank 6: 6-8 greens, 13 pink, 14 cream, 15 outline)."""
    px = bog_water(rng, 0)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 6.5) ** 2 + ((y - 8) / 5.5) ** 2
            notch = x > 8 and y < 8 and abs((y - 8) + (x - 8)) < 1.2
            if d < 1 and not notch:
                px[y][x] = 15 if d > 0.78 else 8 if x + y < 13 else 7 if d < 0.5 else 6
    px[6][5] = px[5][6] = px[6][6] = 14
    px[5][5] = 13
    return px


def moss(rng):
    """Mossy mud: the marsh's ground with soft clumps of moss (it grows like dead ground)."""
    px = dead(rng)
    for _ in range(4):
        cx, cy = rng.randrange(1, 15), rng.randrange(1, 15)
        for y in range(cy - 1, cy + 2):
            for x in range(cx - 2, cx + 2):
                if 0 <= x < 16 and 0 <= y < 16 and rng.random() < 0.8:
                    px[y][x] = 6 if (x + y) % 3 else 7
    return px


def mushrooms(rng):
    """Mossy mud with a few pale mushrooms."""
    px = moss(rng)
    for cx, cy in ((4, 10), (10, 6), (11, 12)):
        px[cy + 1][cx] = px[cy + 2][cx] = 14
        for x in range(cx - 1, cx + 2):
            px[cy][x] = 13 if x == cx else 5
        px[cy - 1][cx] = 13
        px[cy][cx - 2] = px[cy][cx + 2] = 15
    return px


def swamp_tree(rng, alive):
    """The foot of a swamp cypress (its crown is the tile above, swamp_top): a thick trunk flaring
    into knobbly roots in the mud, moss hanging from low branches."""
    px = moss(rng) if alive else dead(rng)
    for y in range(16):                               # trunk, flaring at the foot
        w = 2 if y < 9 else 3 if y < 13 else 4
        for x in range(8 - w, 8 + w):
            px[y][x] = 15 if x in (8 - w, 7 + w) else 12 if x < 8 else 1
    for (x, y) in ((2, 15), (3, 14), (13, 15), (12, 14), (1, 15), (14, 15)):   # roots
        px[y][x] = 12
    px[5][9] = px[6][9] = 5 if not alive else 7                # a knot / a sprig
    for x0, top, n in ((3, 0, 5), (12, 0, 6), (4, 0, 3)):      # moss hanging from the crown above
        for k in range(n):
            px[top + k][x0] = 7 if alive else 3
    return px


def swamp_top(rng, alive):
    """A swamp cypress's crown: crooked bare branches dripping with moss (dead), or leafy with bright
    moss (alive). Solid, like the foot below it."""
    px = moss(rng) if alive else dead(rng)
    for y in range(6, 16):                            # the trunk going up
        for x in range(6, 10):
            px[y][x] = 15 if x in (6, 9) else 12 if x < 8 else 1
    for (y, x0, dx) in ((10, 6, -1), (8, 9, 1), (5, 6, -1), (4, 9, 1), (2, 7, -1)):   # crooked branches
        x = x0
        for k in range(6):
            if 0 <= x < 16:
                px[max(0, y - k // 2)][x] = 12
            x += dx
    if alive:                                         # leafy clumps
        for cx, cy in ((3, 3), (12, 2), (8, 1), (2, 8), (13, 7)):
            for y in range(cy - 2, cy + 2):
                for x in range(cx - 2, cx + 3):
                    if 0 <= x < 16 and 0 <= y < 16:
                        px[y][x] = 8 if y < cy else 7
    for x0, top, n in ((2, 9, 7), (4, 6, 9), (11, 5, 10), (13, 8, 8), (9, 3, 5)):   # long hanging moss
        for k in range(n):
            if top + k < 16:
                px[top + k][x0] = (7 if k % 3 else 8) if alive else (3 if k % 3 else 2)
    return px


def water_edge(mask):
    """Clear water (BG bank 0) with the same ragged shore as the bog: a wet muddy bank (dirt colors,
    which suit dead and living ground alike) and a dark rim where it meets the water."""
    px = water(random.Random(6), 0)
    r = random.Random(200 + mask)
    sides = [(1, lambda i, d: (i, d)), (2, lambda i, d: (15 - d, i)), (4, lambda i, d: (i, 15 - d)),
             (8, lambda i, d: (d, i))]
    for bit, at in sides:
        if not mask & bit:
            continue
        for i in range(16):
            depth = 3 + int(1.6 * math.sin(i * 0.9 + bit) + 1.2 * math.sin(i * 2.3 + bit * 3))
            depth = max(1, min(6, depth))
            for d in range(depth + 1):
                x, y = at(i, d)
                px[y][x] = 15 if d == depth else (4 if r.random() < 0.7 else 5)
    return px


def bog_edge(mask):
    """Bog water (BG bank 6) with a ragged shore of mud and moss along each side that has land.
    Bank 6 colors: 1-3 water, 15 dark rim, 9-11 mud (the marsh's ground), 6-8 moss."""
    px = bog_water(random.Random(5), 0)
    r = random.Random(100 + mask)
    sides = [(1, lambda i, d: (i, d)), (2, lambda i, d: (15 - d, i)), (4, lambda i, d: (i, 15 - d)),
             (8, lambda i, d: (d, i))]
    for bit, at in sides:
        if not mask & bit:
            continue
        for i in range(16):
            depth = 3 + int(1.6 * math.sin(i * 0.9 + bit) + 1.2 * math.sin(i * 2.3 + bit * 3))
            depth = max(1, min(6, depth))
            for d in range(depth + 1):
                x, y = at(i, d)
                if d == depth:
                    px[y][x] = 15                     # the dark wet rim
                elif r.random() < 0.3:
                    px[y][x] = 6 if r.random() < 0.6 else 7   # moss on the bank
                else:
                    px[y][x] = 10 if r.random() < 0.7 else (9 if r.random() < 0.5 else 11)
    return px


def log(rng):
    """A fallen log across the mud (solid; YOU cuts it for wood)."""
    px = moss(rng)
    for y in range(6, 12):
        for x in range(1, 15):
            px[y][x] = 15 if y in (6, 11) else 5 if y == 7 else 4 if y < 10 else 12
    for y in range(6, 12):                            # the cut ends, with rings
        px[y][1] = px[y][14] = 15
    for y in range(7, 11):
        px[y][13] = 5; px[y][12] = 4
    px[8][13] = px[9][13] = 12
    px[5][6] = px[5][7] = px[4][7] = 7                # a sprig of moss on top
    return px


def vael_pipe(rng):
    """A Vael pipe jutting out of the mud and pouring sludge (Pebble breaks it). v0.18.2 (Caleb: make
    them obvious): a big rusty pipe with a red warning light. Drawn with the sludge's bank (BG bank 5):
    1-3 sludge, 4/5 metal, 12 red light, 13 rust, 9-11 mud, 15 outline."""
    px = blank(10)
    for _ in range(20):
        px[rng.randrange(16)][rng.randrange(16)] = rng.choice((9, 11))
    for y in range(1, 11):                            # the pipe, rising out of the mud
        for x in range(3, 13):
            edge = x in (3, 12) or y == 1
            px[y][x] = 15 if edge else 4 if x < 6 else 5 if x < 10 else 13
    for x in range(2, 14):                            # a riveted collar
        px[3][x] = 15; px[4][x] = 13 if x % 3 else 4
    for y in range(1, 3):                             # the dark mouth
        for x in range(5, 11):
            px[y][x] = 1
    px[6][9] = px[6][10] = 12; px[7][9] = px[7][10] = 12   # the red warning light
    px[5][9] = 15
    for y in range(9, 16):                            # sludge spilling down the front and spreading
        w = 2 + (y - 9) // 2
        for x in range(8 - w, 8 + w):
            if 0 <= x < 16:
                px[y][x] = 1 if (x + y) % 3 else 2
    px[12][6] = px[14][9] = 3
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


def mush_lamp(rng):
    """Dewdrop's gift: a glowing mushroom lamp (BG colors: 14 cream, 13 orange, 15 outline, 12 trunk)."""
    px = grass(rng)
    for y in range(9, 15):
        px[y][7] = px[y][8] = 14
        px[y][6] = px[y][9] = 15
    for y in range(3, 9):
        for x in range(2, 14):
            d = ((x - 7.5) / 6) ** 2 + ((y - 8.5) / 5.5) ** 2
            if d < 1:
                px[y][x] = 15 if d > 0.75 or y == 8 else 13
    px[5][5] = px[6][10] = px[4][8] = 14
    px[14][6] = px[14][9] = 15
    return px


def leaf_swing(rng):
    """Twig's gift: a leaf hanging on two vines from a little bar."""
    px = grass(rng)
    for x in range(2, 14):
        px[2][x] = 15; px[3][x] = 12
    for y in range(4, 10):
        px[y][4] = px[y][11] = 6
    for x in range(3, 13):                            # the leaf seat, light with a pale vein
        px[10][x] = 15; px[11][x] = 14 if x in (5, 8, 11) else 8; px[12][x] = 15
    px[11][3] = px[11][12] = 15
    for y in range(3, 15):
        px[y][2] = px[y][13] = 15
    return px


def acorn_bench(rng):
    """Mossy's gift: a bench cut from a big acorn."""
    px = grass(rng)
    for y in range(7, 14):
        for x in range(2, 14):
            d = ((x - 7.5) / 6) ** 2 + ((y - 13) / 6) ** 2
            if d < 1 and y < 13:
                px[y][x] = 15 if d > 0.8 or y == 12 else 5 if y < 10 else 4
    for x in range(3, 13):
        px[6][x] = 15 if x in (3, 12) else 12
        px[5][x] = 15
    px[13][4] = px[13][11] = 15
    return px


def flower_arch(rng):
    """Posy's gift: an arch of flowers."""
    px = grass(rng)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 6.5) ** 2 + ((y - 15) / 12.5) ** 2
            if 0.62 < d < 1:
                px[y][x] = 6 if (x * 3 + y) % 4 else 13
    for (x, y) in ((4, 5), (11, 5), (7, 2), (2, 10), (13, 10)):
        px[y][x] = 14
    return px


def pebble_path(rng):
    """Nub's gift: round stepping pebbles set in the grass (walkable, quick like a path)."""
    px = grass(rng)
    for cx, cy in ((4, 4), (11, 6), (5, 11), (11, 13)):
        for y in range(16):
            for x in range(16):
                d = ((x - cx) / 3) ** 2 + ((y - cy) / 2.3) ** 2
                if d < 1:
                    px[y][x] = 15 if d > 0.7 else 14 if y < cy else 3
    return px


def dew_pond(rng):
    """Pip's gift: a tiny pond in a ring of stones, with a lily pad."""
    px = grass(rng)
    for y in range(16):
        for x in range(16):
            d = ((x - 7.5) / 6.5) ** 2 + ((y - 8.5) / 5.5) ** 2
            if d < 1:
                px[y][x] = 3 if d > 0.72 else 15 if d > 0.6 else 10 if (x + y) % 5 else 11
    for (x, y) in ((6, 8), (7, 8), (6, 9), (7, 9), (8, 9)):
        px[y][x] = 7
    px[8][8] = 6
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
    tiles += [grass(rng, tufts=0)]
    tiles += [mush_lamp(rng), leaf_swing(rng), acorn_bench(rng), flower_arch(rng), pebble_path(rng), dew_pond(rng)]
    tiles += [rock(rng, on_grass=True)]
    tiles += [sludge(rng), lily_pad(rng), reeds(rng), vael_pipe(rng)]
    tiles += [bog_water(rng, 0), bog_water(rng, 1), bog_pad(rng), moss(rng), mushrooms(rng), swamp_tree(rng, False),
              swamp_tree(rng, True), log(rng), swamp_top(rng, False), swamp_top(rng, True)]
    tiles += [bog_edge(k) for k in range(1, 16)]
    tiles += [water_edge(k) for k in range(1, 16)]
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
#   curved base and a dark opening at its foot. Its bark repeats every 16 px down the straight
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
    Depends on y only through y % 16, so the straight trunk repeats."""
    import math
    u = max(-1.0, min(1.0, (x - TREE_CX) / TRUNK_HALF))
    a = math.asin(u)                                  # angle around the trunk
    light = 0.62 - 0.42 * u - 0.18 * u * u
    col = a * 9 / math.pi + 9                         # grooves, closer together toward the edges
    g, ci = col % 1, int(col)
    yy = y % 16
    if g < 0.14 or (g < 0.22 and (yy + ci * 5) % 8 < 2):
        return 5                                        # a groove, ragged at the edges
    if abs(u) > 0.97:
        return 1
    cy = (ci * 7) % 16                                  # a short, slanted crack on every other plate
    if ci % 2 == 0 and 0.3 < g < 0.7 and yy == (cy + int(g * 4)) % 16:
        return 2
    if g > 0.78:
        light -= 0.18                                   # each plate darkens toward its groove
    if light > 0.86 and (yy + ci) % 4:
        return 6
    return 4 if light > 0.58 else 3 if light > 0.32 else 2


# Roots (left half; the right half is its mirror image): grid points from the base outward.
# Each: (points, width at the base, width at the end). Ends that point down get a root tip tile
# on the map (ROOT_TIPS); the clamps sit on the first root (CLAMP_SPOTS, in pixels).
def mirror_x(x):
    return 511 - x


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
# Clamps on the roots that run toward you (Caleb, v0.14): (x, y) on a vertical run.
FRONT_CLAMPS = [(88, 416), (200, 376), (311, 376)]
# Every clamp: (x, y, band) where band "v" crosses a sideways root and "h" a root running down.
ALL_CLAMPS = ([(x, y, "v") for x, y in CLAMP_SPOTS] + [(mirror_x(x), y, "v") for x, y in CLAMP_SPOTS]
              + [(x, y, "h") for x, y in FRONT_CLAMPS])


def stroke(px, pts, w0, w1, colors, S=512, bark=False):
    """A thick path through grid points, tapering from w0 to w1, shaded light on top. colors:
    (outline, dark, mid, light, highlight). bark=True (roots): the trunk's bark instead, lit from
    the top-left like the trunk, with grooves running along the root (they depend only on the
    distance across the root, so a straight run still repeats)."""
    out, dark, mid, light, hi = colors
    segs = list(zip(pts, pts[1:]))
    total = sum(max(abs(b[0] - a[0]), abs(b[1] - a[1])) for a, b in segs) or 1
    for outline in (True, False):
        done = 0
        for (x0, y0), (x1, y1) in segs:
            n = max(abs(x1 - x0), abs(y1 - y0)) or 1
            ln = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5 or 1
            nx, ny = -(y1 - y0) / ln, (x1 - x0) / ln       # across the root
            facing = nx * -0.6 + ny * -0.8                 # how much that side faces the light
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
                        elif bark:
                            o = (xx - x) * nx + (yy - y) * ny
                            v = o / max(1.0, r)
                            b = 0.56 + 0.44 * v * facing - 0.3 * v * v
                            if abs(v) > 0.9:
                                c = 1
                            elif (o + 70) % 7 < 1.3:
                                c = 5                              # a groove along the root
                            else:
                                c = 6 if b > 0.86 else 4 if b > 0.58 else 3 if b > 0.32 else 2
                            px[yy][xx] = c
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
        stroke(px, pts, w0, w1, rootc, bark=True)
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
    for x0, y0 in ((214, 140), (322, 220)):              # rot oozing down the bark (no holes: they read as eyes)
        for y in range(y0, y0 + 40):
            for x in (x0 + (y - y0) // 13, x0 + 1 + (y - y0) // 13):
                if px[y][x]:
                    px[y][x] = 11 if (y - y0) % 9 else 10
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


# ---- v0.16: the hollow inside the tree, and the living tree (trunk with Bark's face, leaves) ----
HOLLOW_C, HOLLOW_RX, HOLLOW_RY = (256, 240), 216, 168      # the chamber (an ellipse, in pixels)
HOLLOW_EXIT = (232, 280)                                   # the corridor out, x range, down to the bottom
HOLLOW_PAL = [
    (255, 0, 255),    # 0 clear
    (14, 10, 10),     # 1 outline / deep shadow
    (46, 30, 24),     # 2 wall dark
    (70, 48, 36),     # 3 wall
    (98, 70, 50),     # 4 wall light
    (30, 20, 16),     # 5 groove
    (88, 60, 40),     # 6 floor dark
    (116, 84, 56),    # 7 floor
    (146, 110, 74),   # 8 floor light
    (80, 200, 190),   # 9 glow fungus
    (190, 250, 230),  # 10 glow, bright
    (52, 24, 58),     # 11 rot dark
    (118, 52, 128),   # 12 rot
    (232, 196, 120),  # 13 light from outside
    (24, 16, 12),     # 14 floor shadow
    (0, 0, 0),
]


def make_hollow():
    """The hollow: a wide wooden chamber inside the trunk. Bark walls (repeating every 16 px), a
    wood floor with grain, a dark rim, glowing fungus on the walls, rot veins creeping to the
    middle (where Gloomheart waits), and a corridor down to the light outside."""
    import math
    S = 512
    cx, cy = HOLLOW_C
    px = [[0] * S for _ in range(S)]
    for y in range(S):
        for x in range(S):
            d = ((x - cx) / HOLLOW_RX) ** 2 + ((y - cy) / HOLLOW_RY) ** 2
            corridor = HOLLOW_EXIT[0] <= x < HOLLOW_EXIT[1] and y > cy
            if d < 1 or corridor:
                c = 7 if (y % 16) not in (0, 9) else 6           # floor boards with grain
                if (y % 16) == 0 and (x + (y // 16) * 24) % 48 == 0:
                    c = 14                                       # a board's end
                if ((x % 16) * 3 + (y % 16) * 7) % 37 == 0:
                    c = 8                                        # grain flecks (repeat every 16 px)
                if d > 0.86 and not corridor:
                    c = 14 if d > 0.93 else 6                    # the dark rim along the wall
                px[y][x] = c
            else:
                g = (x + (y // 16) * 3) % 8                      # bark walls (repeat every 8 px across)
                c = 5 if g == 0 else 2 if g < 4 else 3 if g < 7 else 4
                if d < 1.08 and not corridor:
                    c = 1                                        # the wall's edge
                px[y][x] = c
    for y in range(S):                                           # the corridor's walls
        for x in (HOLLOW_EXIT[0] - 1, HOLLOW_EXIT[1]):
            if y > cy + HOLLOW_RY * 0.9:
                px[y][x] = 1
    for y in range(S - 60, S):                                   # light from outside
        for x in range(HOLLOW_EXIT[0], HOLLOW_EXIT[1]):
            if ((x % 16) + (y % 16)) % (3 if y > S - 24 else 5) == 0:
                px[y][x] = 13
    rng = random.Random(77)
    for a in range(0, 360, 30):                                  # glowing fungus on the walls
        r = math.radians(a + rng.randint(-8, 8))
        fx, fy = int(cx + (HOLLOW_RX + 10) * math.cos(r)), int(cy + (HOLLOW_RY + 10) * math.sin(r))
        if fy > cy + 100 and HOLLOW_EXIT[0] - 20 < fx < HOLLOW_EXIT[1] + 20:
            continue
        for dy in range(-3, 4):
            for dx in range(-5, 6):
                if 0 <= fx + dx < S and 0 <= fy + dy < S and (dx / 5) ** 2 + (dy / 3) ** 2 < 1:
                    px[fy + dy][fx + dx] = 10 if dy < 0 and abs(dx) < 3 else 9
    for k in range(5):                                           # rot veins toward the middle
        a = k * math.pi * 2 / 5 + 0.3
        x, y = cx + 200 * math.cos(a), cy + 156 * math.sin(a)
        for t in range(110):
            x += (cx - x) * 0.012 + math.sin(t * 0.3 + k) * 0.8
            y += (cy - y) * 0.012 + math.cos(t * 0.27 + k) * 0.8
            for dy in (0, 1):
                for dx in (0, 1):
                    xx, yy = int(x) + dx, int(y) + dy
                    if 0 <= xx < S and 0 <= yy < S and px[yy][xx] in (6, 7, 8, 14):
                        px[yy][xx] = 12 if dx == dy else 11
    img = Image.new("P", (S, S), 0)
    img.putpalette([c for rgb in HOLLOW_PAL for c in rgb])
    img.putdata([v for row in px for v in row])
    return img


ALIVE_PAL = [
    (255, 0, 255),    # 0 clear
    (22, 20, 14),     # 1 outline
    (78, 52, 34),     # 2 bark dark
    (112, 78, 50),    # 3 bark
    (148, 108, 70),   # 4 bark light
    (52, 34, 24),     # 5 groove
    (186, 146, 98),   # 6 bark highlight
    (12, 10, 8),      # 7 the opening
    (34, 86, 40),     # 8 leaf dark
    (62, 132, 56),    # 9 leaf
    (104, 176, 74),   # 10 leaf light
    (168, 214, 108),  # 11 leaf highlight
    (90, 130, 60),    # 12 moss
    (234, 222, 186),  # 13 eye white
    (236, 150, 170),  # 14 blossom
    (0, 0, 0),
]
FACE_C = (256, 230)                                        # Bark's eyes (his face runs down to the trunk's base)


def make_tree_alive():
    """The Great Tree, healed: the same trunk and roots, warm healthy bark, moss at the base, and
    Bark's face on the front (half-grumpy, kinder now: level brows, a small smile)."""
    img = make_trunk()
    px = [list(img.tobytes()[y * 512:(y + 1) * 512]) for y in range(512)]
    for y in range(512):                                     # the disease heals
        for x in range(512):
            c = px[y][x]
            if c in (8, 9, 12):
                px[y][x] = 3
            elif c in (10, 11):
                px[y][x] = 2
            elif c == 13:
                px[y][x] = 12
    for y in range(BASE_Y - 40, 512):                         # the opening grows shut: bark again
        for x in range(TREE_CX - 30, TREE_CX + 30):
            if px[y][x] in (7, 1) and abs(x - TREE_CX) < 26 and y < trunk_bottom(x) - 2:
                px[y][x] = bark_color(x, y)
    fx, fy = FACE_C
    import math

    def put(x, y, c):
        if 0 <= x < 512 and 0 <= y < 512 and px[y][x]:
            px[y][x] = c

    def groove(x0, y0, x1, y1, lit=True):
        """A carved crease: a dark line with a lit lip just above it (light from the top-left)."""
        n = max(abs(x1 - x0), abs(y1 - y0)) or 1
        for t in range(n + 1):
            x, y = x0 + (x1 - x0) * t // n, y0 + (y1 - y0) * t // n
            put(x, y, 5)
            put(x, y + 1, 2)
            if lit:
                put(x, y - 1, 6)
    def mass(cx, cy, rx, ry, tilt=0.0, grain="v"):
        """A rounded bulge of bark, lit from the top-left: light, mid, dark, with a dark rim below.
        grain: wood grain running down ("v") or sideways ("h")."""
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                u, v = (x - cx) / rx, (y - cy - tilt * (x - cx)) / ry
                d = u * u + v * v
                if d < 1:
                    lit = -0.55 * u - 0.85 * v
                    c = 6 if lit > 0.55 else 4 if lit > 0.05 else 3 if lit > -0.45 else 2
                    g = (x + int(3 * math.sin(y * 0.4))) % 5 if grain == "v" else (y + int(2 * math.sin(x * 0.3))) % 4
                    if g == 0 and c in (6, 4, 3):
                        c = {6: 4, 4: 3, 3: 2}[c]                # wood grain through the bulge
                    put(x, y, 1 if d > 0.88 and v > 0 else c)

    for y in (fy - 62, fy - 50):                                  # two deep forehead wrinkles
        for x in range(fx - 60, fx + 61):
            yy = y + int(4 * math.sin(x * 0.07 + y))
            put(x, yy - 1, 6)
            put(x, yy, 5)
            put(x, yy + 1, 5)
            put(x, yy + 2, 2)
    for sx in (-1, 1):
        ex = fx + sx * 50
        mass(ex, fy - 2, 27, 11)                                  # a heavy wooden lid
        for x in range(ex - 22, ex + 23):                         # the eye: a closed, sleepy slit
            y = fy + 7 + int(((x - ex) / 22) ** 2 * 4)
            put(x, y - 1, 6)
            put(x, y, 1)
            put(x, y + 1, 1)
            put(x, y + 2, 5)
        mass(ex, fy - 20, 34, 11, tilt=sx * -0.22)                # a heavy brow, sloping to the middle
        for k in range(2):                                        # two short cracks at the outer corner
            ox = ex + sx * 27
            for t in range(9):
                x, y = ox + sx * t, fy - 2 + k * 7 + (t // 3) * (1 if k else -1)
                put(x, y, 5)
                put(x, y + 1, 2)
    for y in range(fy - 14, fy + 44):                             # a broad nose
        w = 9 + max(0, y - fy - 10) // 6
        for x in range(fx - w, fx + w + 1):
            put(x, y, 1 if abs(x - fx) == w else 6 if x < fx - w // 2 else 4 if x < fx else 3 if x < fx + w // 2 else 2)
    mass(fx, fy + 46, 17, 11)                                     # its round tip
    for sx in (-1, 1):
        for t in range(5):
            put(fx + sx * (8 + t), fy + 54 + t // 2, 1)          # nostrils
    for x in range(fx - 20, fx + 21):                              # the mouth, in the moustache's shadow
        put(x, fy + 80 + abs(x - fx) // 8, 1)
    for sx in (-1, 1):                                             # a wooden moustache, drooping at the ends
        mass(fx + sx * 50, fy + 74, 22, 10, tilt=sx * 0.35, grain="h")
        mass(fx + sx * 24, fy + 64, 26, 11, tilt=sx * 0.18, grain="h")
    # the base, like a tree's foot: wavy wrinkles across the lower trunk, bulging root buttresses
    for yb in (BASE_Y - 14, BASE_Y + 6, BASE_Y + 24):
        for x in range(TREE_CX - 220, TREE_CX + 221):
            if abs(x - fx) < 90 and yb < fy + 100:
                continue                                           # not across the face
            y = yb + int(3 * math.sin(x * 0.06 + yb))
            if 0 <= y < 512 and px[y][x] in (2, 3, 4, 6):
                put(x, y - 1, 6)
                put(x, y, 5)
                put(x, y + 1, 2)
    for y in range(BASE_Y, 512):                                   # the flared foot: rounded folds of wood
        for x in range(512):
            d = abs(x - TREE_CX)
            if d <= TRUNK_HALF or d > trunk_half(y) - 1 or y > trunk_bottom(x) - 2 or not px[y][x]:
                continue                                           # only the flare, not the trunk or the roots
            f = ((d - TRUNK_HALF) % 22) / 22                       # where we are across one fold (0..1)
            c = 6 if f < 0.18 else 4 if f < 0.45 else 3 if f < 0.75 else 2 if f < 0.92 else 5
            if x > TREE_CX:
                c = {6: 4, 4: 3, 3: 2, 2: 2, 5: 5}[c]              # the right side is in shade
            px[y][x] = c
    out = Image.new("P", (512, 512), 0)
    out.putpalette([c for rgb in ALIVE_PAL for c in rgb])
    out.putdata([v for row in px for v in row])
    return out


def leaf_texture(x, y):
    """Leaf clusters repeating every 16 px: dark gaps, mid leaves, light tips."""
    u, v = x % 16, y % 16
    blobs = ((3, 3), (11, 2), (7, 9), (14, 12), (1, 13))
    best = 99
    for bx, by in blobs:
        for ox in (-16, 0, 16):
            for oy in (-16, 0, 16):
                d = ((u - bx - ox) ** 2 + (v - by - oy) ** 2) ** 0.5
                best = min(best, d)
    if best < 1.6:
        return 11
    if best < 3.2:
        return 10
    if best < 5.2:
        return 9
    return 8


def leaf_cell(x, y):
    """Dense small leaves repeating every 16 px: leaf shapes in two greens over dark gaps.
    No highlights: the light is soft under the crown."""
    u, v = x % 16, y % 16
    leaves = ((2, 3, 1), (9, 1, -1), (13, 7, 1), (5, 10, -1), (11, 13, 1), (1, 14, -1), (7, 6, 1))
    for lx, ly, sl in leaves:
        for ox in (-16, 0, 16):
            for oy in (-16, 0, 16):
                a, b = u - lx - ox, v - ly - oy
                if ((a + sl * b * 0.6) / 3.2) ** 2 + (b / 1.8) ** 2 < 1:
                    return 10 if (lx + ly) % 3 == 0 else 9
    return 8


def make_leaves():
    """The healed tree's crown: one leafy mass with a ragged, scalloped edge (no round puffs),
    dense small-leaf texture, darker toward its lower edge, no gloss; a couple of ragged holes.
    Left half drawn; the right half mirrors it."""
    import math
    S = 512
    px = [[0] * S for _ in range(S)]

    def bottom(x):                                    # the crown's lower edge: over the trunk, then down the side
        base = 146 if x > 126 else 146 + (126 - x) * 1.05
        return base + 9 * math.sin(x * 0.11) + 5 * math.sin(x * 0.29 + 1.3)   # gentle lumps

    def hole(x, y):
        for hx, hy, hr in ((70, 90, 15), (176, 54, 11)):
            a = math.atan2(y - hy, x - hx)
            if (x - hx) ** 2 + (y - hy) ** 2 < (hr * (1 + 0.15 * math.sin(3 * a + hx))) ** 2:
                return True
        return False
    for y in range(S):
        for x in range(256):
            b = bottom(x)
            if y < b and not hole(x, y):
                c = 8 if y > b - 4 else leaf_cell(x, y)               # shadow along the lower edge
                if ((x // 16) * 2 + (y // 16) * 3) % 5 == 0 and c == 10:
                    c = 9                                             # darker clumps here and there
                px[y][x] = c
    for y in range(S):                                                # a dark outline where the leaves end
        for x in range(256):
            if px[y][x] and any(0 <= x + dx < 256 and 0 <= y + dy < S and px[y + dy][x + dx] == 0
                                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                px[y][x] = 1
    for y in range(S):
        for x in range(256):
            px[y][mirror_x(x)] = px[y][x]
    img = Image.new("P", (S, S), 0)
    img.putpalette([c for rgb in ALIVE_PAL for c in rgb])
    img.putdata([v for row in px for v in row])
    return img


def make_clamp():
    """The Vael iron clamps (Pebble breaks them): one 32 x 32 sprite per clamp in ALL_CLAMPS, side
    by side. Each ring is sized to its root (measured on art/tree_trunk.png): it wraps all the way
    around, a little wider than the root, with rounded ends, shaded as it turns (lit top-left)."""
    pal = [(255, 0, 255), (24, 22, 24), (70, 74, 82), (110, 114, 124), (158, 162, 170), (204, 206, 210),
           (150, 72, 40), (214, 120, 60)] + [(0, 0, 0)] * 8
    trunk = Image.open(os.path.join(ART, "tree_trunk.png"))
    img = Image.new("P", (32 * len(ALL_CLAMPS), 32), 0)
    img.putpalette([c for rgb in pal for c in rgb])
    for i, (x, y, band) in enumerate(ALL_CLAMPS):
        dx, dy = (0, 1) if band == "v" else (1, 0)         # measure across the root
        a = b = 0
        while trunk.getpixel((x - dx * (a + 1), y - dy * (a + 1))):
            a += 1
        while trunk.getpixel((x + dx * (b + 1), y + dy * (b + 1))):
            b += 1
        half = (a + b) / 2 + 2                             # the ring reaches past the root's edges
        mid = (b - a) / 2
        for py in range(32):
            for pxx in range(32):
                along, across = (pxx - 16, py - 16 - mid) if band == "v" else (py - 16, pxx - 16 - mid)
                if abs(along) > 5:
                    continue
                reach = half - (along / 5) ** 2 * 1.6          # rounded ends: the ring curves away
                if abs(across) > reach:
                    continue
                edge = abs(along) > 4 or abs(across) > reach - 1
                t = across / half                              # -1 .. 1 around the root
                light = 0.62 - 0.5 * t - 0.25 * along / 5
                c = 1 if edge else 5 if light > 0.95 else 4 if light > 0.7 else 3 if light > 0.42 else 2
                if not edge and (pxx * 3 + py * 5 + i) % 13 == 0:
                    c = 6                                      # rust
                img.putpixel((32 * i + pxx, py), c)
        for k in (-0.55, 0.0, 0.55):                           # rivets down the ring
            rx, ry = (16, int(16 + mid + k * half)) if band == "v" else (int(16 + mid + k * half), 16)
            if 0 <= rx < 32 and 0 <= ry < 32:
                img.putpixel((32 * i + rx, ry), 7)
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


# ---- combat (v0.15): attacks, small enemies, mini bosses ----
COMBAT_PAL = [
    (255, 0, 255),    # 0 clear
    (24, 20, 28),     # 1 outline
    (52, 120, 52),    # 2 vine dark
    (120, 200, 90),   # 3 vine light
    (40, 96, 178),    # 4 water dark
    (110, 178, 240),  # 5 water light
    (236, 246, 255),  # 6 foam, gust, spark
    (96, 88, 82),     # 7 rock dark
    (156, 146, 132),  # 8 rock light
    (190, 220, 230),  # 9 gust pale
    (110, 72, 44),    # 10 wood
    (168, 120, 76),   # 11 wood light
    (62, 30, 72),     # 12 blight dark
    (128, 64, 140),   # 13 blight
    (250, 210, 70),   # 14 eye
    (138, 144, 156),  # 15 metal
]
COMBAT_ORDER = ["whip_h", "whip_v", "water", "rock", "gust", "slam", "blob0", "blob1", "drone0", "drone1",
                "spark", "leaf", "spore", "mote", "ringdot", "shadow", "stars", "glint", "bubble", "splash", "petal"]
# v0.17 the Leafen: five looks (two-leaf sprout, tall leaf, flower bud, acorn cap, dewdrop), each standing,
# stepping, swimming and climbing; six tiny homes; the shiny pebble. All in the combat palette (OBJ bank 12).
LEAFEN_LOOKS = ["sprout", "leaf", "bud", "acorn", "dew"]
LEAFEN_POSES = ["stand", "step", "swim", "climb"]
LEAFEN_HOMES = ["mushroom", "tent", "acornhut", "pot", "pebblehut", "dewhut"]
LEAFEN_ORDER = [f"lf_{l}_{p}" for l in LEAFEN_LOOKS for p in LEAFEN_POSES] + [f"lh_{h}" for h in LEAFEN_HOMES] + ["shiny"]
LEAFEN_ORDER += ["peek"]          # v0.17.3: Nub's leaves and the top of his head, poking out of the rocks
LEAFEN_ORDER += ["firefly"]       # v0.18.1: a marsh firefly (a warm glow)
LEAFEN_ORDER += ["swimring", "swimring2"]   # v0.18.1: the ripples round the swimming wizard (two frames)
LEAFEN_ORDER += [f"orb_{k}" for k in range(6)]  # v0.18.1: the spirits as little glowing orbs while you swim
LEAFEN_ORDER += ["wake_h", "wake_d", "wake_u"]  # v0.18.1: the wake behind the swimming wizard
LEAFEN_ORDER += [f"mini_{k}" for k in range(6)]  # v0.18.4: a spirit in the pouch: a tiny orb circling his head
LEAFEN_PAL = [                     # their own palette (OBJ bank 11, the clamps' once the tree is healed)
    (255, 0, 255),    # 0 clear
    (34, 46, 30),     # 1 outline
    (68, 148, 72),    # 2 body shade
    (124, 210, 104),  # 3 body
    (204, 242, 168),  # 4 face
    (24, 28, 24),     # 5 eyes
    (255, 255, 255),  # 6 white
    (244, 146, 160),  # 7 blush, mushroom cap
    (46, 126, 54),    # 8 leaf dark
    (164, 232, 92),   # 9 leaf light
    (112, 72, 42),    # 10 wood dark
    (180, 128, 76),   # 11 wood light
    (252, 220, 84),   # 12 yellow
    (128, 196, 246),  # 13 water light
    (56, 114, 198),   # 14 water dark
    (158, 152, 142),  # 15 stone
]


def _canvas(n=16):
    return [[0] * n for _ in range(n)]


def _disc(px, cx, cy, r, c, edge=None):
    n = len(px)
    for y in range(n):
        for x in range(n):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if d <= r:
                px[y][x] = edge if edge is not None and d > r - 1 else c


def combat_cell(name):
    px = _canvas()
    if name == "whip_h":                       # a vine lashing out to the right
        for x in range(1, 16):
            y = int(8 + 3 * __import__("math").sin(x * 0.55))
            for dy in (-1, 0, 1):
                px[y + dy][x] = 2 if dy else 3
        for lx, ly in ((5, 4), (11, 11), (14, 6)):
            px[ly][lx] = px[ly][lx - 1] = 3
    elif name == "whip_v":
        for y in range(1, 16):
            x = int(8 + 3 * __import__("math").sin(y * 0.55))
            for dx in (-1, 0, 1):
                px[y][x + dx] = 2 if dx else 3
        for lx, ly in ((4, 5), (11, 11), (6, 14)):
            px[ly][lx] = px[ly - 1][lx] = 3
    elif name == "water":
        _disc(px, 7.5, 8, 5.5, 4, 1)
        _disc(px, 7, 7.5, 3.5, 5)
        px[5][5] = px[5][6] = px[6][5] = 6
    elif name == "rock":
        _disc(px, 7.5, 8, 5.5, 7, 1)
        _disc(px, 6.5, 7, 3.5, 8)
        px[5][5] = 6
        px[10][9] = px[9][10] = 1
    elif name == "gust":                       # a swirl of wind
        import math
        for t in range(60):
            a = t * 0.21
            r = 1.5 + t * 0.11
            x, y = int(7.5 + r * math.cos(a)), int(8 + r * math.sin(a) * 0.8)
            if 0 <= x < 16 and 0 <= y < 16:
                px[y][x] = 6 if t % 3 else 9
    elif name == "slam":                       # one quarter of a burst ring (drawn 4 times, flipped)
        for y in range(16):
            for x in range(16):
                d = ((x - 15.5) ** 2 + (y - 15.5) ** 2) ** 0.5
                if 11 <= d < 15:
                    px[y][x] = 11 if d < 13 else 10
                elif 15 <= d < 16:
                    px[y][x] = 1
    elif name in ("blob0", "blob1"):           # a blight blob: purple rot with eyes
        sq = name == "blob1"
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) / (6.5 if sq else 5.8)) ** 2 + ((y - (10 if sq else 9.5)) / (4.5 if sq else 5.5)) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.78 else 13 if y < (9 if sq else 8) else 12
        ey = 9 if sq else 8
        px[ey][5] = px[ey][9] = 14
        px[ey + 1][5] = px[ey + 1][9] = 1
        px[ey - 3][7] = 13
    elif name in ("drone0", "drone1"):         # a Vael drone: a metal disc, a red eye, a rotor
        _disc(px, 7.5, 9, 5.5, 15, 1)
        _disc(px, 7.5, 9, 2.2, 1)
        px[9][7] = px[9][8] = 14
        rot = 2 if name == "drone0" else 5
        for x in range(7 - rot, 9 + rot):
            px[2][x] = 1
        px[3][7] = px[3][8] = 1
    elif name == "leaf":                       # a leaf, swirling round the wizard when Bark gives his power
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) / 5.5) ** 2 + ((y - 7.5) / 3) ** 2
                if abs((x - 7.5) * 0.6 - (y - 7.5) * 0.8) < 3 and d < 1.6:
                    px[y][x] = 1 if d > 1.25 else 3 if x < 8 else 2
        for k in range(4, 12):
            px[k][k] = 2
    elif name == "spore":                      # Gloomheart's spore: a dark rot ball with a pale glow
        _disc(px, 7.5, 7.5, 4.6, 13, edge=1)
        _disc(px, 7.5, 7.5, 2.6, 12)
        px[6][6] = px[6][7] = px[7][6] = 14
    elif name == "mote":                       # a small rot mote (warnings, dust, shards)
        _disc(px, 7.5, 7.5, 2.2, 13, edge=12)
        px[7][7] = 6
    elif name == "ringdot":                    # a dot of the rot ring
        _disc(px, 7.5, 7.5, 3.2, 13, edge=12)
        _disc(px, 7.5, 7.5, 1.4, 14)
    elif name == "shadow":                     # the shadow where the Gloomheart will land
        for y in range(16):
            for x in range(16):
                if ((x - 7.5) / 7.5) ** 2 + ((y - 9.5) / 4) ** 2 < 1:
                    px[y][x] = 1
    elif name == "stars":                      # dazed: three little stars
        for sx, sy in ((3, 6), (8, 3), (13, 7)):
            px[sy][sx] = 6
            for d in (-1, 1):
                px[sy + d][sx] = px[sy][sx + d] = 14
    elif name == "petal":                      # Lily's attack: a little spinning lily blossom (v0.18.2)
        for (cx, cy) in ((7.5, 4.5), (10.5, 7.5), (7.5, 10.5), (4.5, 7.5)):
            _disc(px, cx, cy, 2.2, 6, edge=1)
        _disc(px, 7.5, 7.5, 1.6, 14)
    elif name == "splash":                     # a tiny splash: a few white flecks flicking up (the swimmer's wake)
        for (x, y) in ((6, 9), (9, 9), (7, 7), (8, 8), (5, 10), (10, 10)):
            px[y][x] = 6
        px[8][7] = px[9][8] = 5
    elif name == "bubble":                     # a water droplet / bubble
        _disc(px, 7.5, 7.5, 1.8, 5, edge=4)
        px[7][7] = 6
    elif name == "glint":                      # a green spark of life (Bark floating, the hollow healed)
        _disc(px, 7.5, 7.5, 2.4, 3, edge=2)
        px[7][7] = px[8][8] = px[7][8] = px[8][7] = 6
    elif name == "spark":                      # a hit
        for k in range(-5, 6):
            px[8][7 + k] = px[8 + k][7] = 6
        for k in range(-3, 4):
            if 0 <= 8 + k < 16:
                px[8 + k][7 + k] = 6 if abs(k) < 2 else 9
                px[8 - k][7 + k] = 6 if abs(k) < 2 else 9
    return px


def leafen_cell(name):
    px = _canvas()
    if name.startswith("lf_"):
        _, look, pose = name.split("_")
        leafen(px, look, pose)
    elif name.startswith("lh_"):
        leafen_home(px, name[3:])
    elif name.startswith("orb_"):              # a spirit as an orb: its color, a bright spot, a soft halo
        core, light, halo = [(3, 4, 2), (13, 6, 14), (15, 6, 1), (6, 6, 13), (11, 12, 10), (7, 6, 1)][int(name[4:])]
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
                if d < 2.4:                       # small (Caleb): a 5-pixel orb with a 1-pixel halo
                    px[y][x] = core
                elif d < 3.3:
                    px[y][x] = halo
        px[6][7] = px[7][6] = light
    elif name.startswith("mini_"):             # a pouch spirit: smaller than a swim orb (Caleb), 3 px with a soft rim
        core, light, halo = [(3, 4, 2), (13, 6, 14), (15, 6, 1), (6, 6, 13), (11, 12, 10), (7, 6, 1)][int(name[5:])]
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) ** 2 + (y - 7.5) ** 2) ** 0.5
                if d < 1.0:
                    px[y][x] = core
                elif d < 1.9:
                    px[y][x] = halo
        px[7][7] = light
    elif name.startswith("swimring"):          # soft, broken ripples in water blues where he meets the water
        wide = 7.4 if name.endswith("2") else 6.6
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) / wide) ** 2 + ((y - 11) / 2.2) ** 2
                if 0.66 < d < 1 and (x * 5 + y * 3 + (name.endswith("2"))) % 7 not in (0, 1):   # gaps in the ring
                    px[y][x] = 6 if y > 11 and abs(x - 7.5) < 3 else 13 if y > 10 else 14
    elif name.startswith("wake_"):             # the V of a wake behind him as he swims (h: sideways, d/u)
        kind = name[5:]
        for k in range(1, 8):
            if kind == "h":                       # moving right: the V opens to the left, behind him
                for (x, y) in ((7 - k, 11 - k // 2), (7 - k, 11 + k // 2)):
                    if 0 <= x < 16 and 0 <= y < 16:
                        px[y][x] = 13 if k < 5 else 14
            else:                                 # d: moving down, the V opens upward; u: opens downward
                sy = -1 if kind == "d" else 1
                for (x, y) in ((7 - k // 2, 11 + sy * k), (8 + k // 2, 11 + sy * k)):
                    if 0 <= x < 16 and 0 <= y < 16:
                        px[y][x] = 13 if k < 5 else 14
    elif name == "firefly":                    # a tiny glow: a bright core and a soft halo
        px[7][7] = px[7][8] = px[8][7] = px[8][8] = 12
        for (x, y) in ((6, 7), (6, 8), (9, 7), (9, 8), (7, 6), (8, 6), (7, 9), (8, 9)):
            px[y][x] = 4
    elif name == "peek":                       # a buried Leafen: two leaves and the top of a head
        for y in range(11, 16):
            for x in range(16):
                d = ((x - 7.5) / 4.6) ** 2 + ((y - 15) / 3.7) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.7 else 3
        px[10][7] = px[10][8] = 8
        px[9][6] = px[9][9] = 9; px[9][5] = px[9][10] = 9
        px[8][5] = px[8][10] = 9; px[8][4] = px[8][11] = 8
    else:                                      # Dewdrop's shiny pebble, glinting
        for y in range(16):
            for x in range(16):
                d = ((x - 7.5) / 4.2) ** 2 + ((y - 11) / 3.2) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.72 else 6 if x + y < 15 else 13 if x + y < 19 else 15
        for k in (-2, -1, 1, 2):
            px[4][11 + k] = 12
            px[4 + k][11] = 12
        px[4][11] = 6
    return px


def leafen(px, look, pose):
    """A Leafen (redrawn small and cute at Caleb's ask): about 11 px tall, a big round head with a pale
    face, big eyes with a shine, pink cheeks, a tiny body, and its look on top."""
    swim, climb = pose == "swim", pose == "climb"
    bob = 1 if pose == "step" else 0
    hy = (11 if swim else 9) - bob                    # head center
    if not swim:                                      # tiny body and feet
        for y in (hy + 4, hy + 5):
            for x in range(6, 10):
                px[y][x] = 1 if x in (6, 9) else 3 if x == 7 else 2
        if pose == "step":
            px[15][5] = px[15][10] = 1
        else:
            px[15][6] = px[15][9] = 1
        if climb:                                     # arms up, holding on
            for y in (hy - 1, hy):
                px[y][3] = px[y][12] = 3
            px[hy - 2][3] = px[hy - 2][12] = 1
        else:
            px[hy + 4][5] = px[hy + 4][10] = 3      # little arms
    for y in range(16):                               # the head
        for x in range(16):
            d = ((x - 7.5) / 4.6) ** 2 + ((y - hy) / 3.7) ** 2
            if d < 1:
                px[y][x] = 1 if d > 0.7 else 2 if (x >= 10 and y >= hy) else 3
    if not climb:
        for y in range(16):                           # a pale face
            for x in range(16):
                if ((x - 7.5) / 3.0) ** 2 + ((y - hy - 0.8) / 2.0) ** 2 < 1:
                    px[y][x] = 4
        for ex in (6, 9):                             # eyes: dark with a white shine on top
            px[hy][ex] = 6
            px[hy + 1][ex] = 5
        px[hy + 2][5] = px[hy + 2][10] = 7           # cheeks
        px[hy + 2][7] = px[hy + 2][8] = 2            # a small smile
    top = hy - 4
    if look == "sprout":                              # two round leaves on a stem
        px[top][7] = px[top][8] = 8
        px[top - 1][6] = px[top - 1][9] = 9; px[top - 1][5] = px[top - 1][10] = 9
        px[top - 2][5] = px[top - 2][10] = 9; px[top - 2][4] = px[top - 2][11] = 8
    elif look == "leaf":                              # one tall leaf
        px[top][7] = 8
        for y, xs in ((top - 1, (7, 8)), (top - 2, (7, 8, 9)), (top - 3, (8, 9))):
            for x in xs:
                px[y][x] = 9
        px[top - 2][9] = 8
    elif look == "bud":                               # a yellow flower
        px[top][7] = 8
        px[top - 2][7] = 6
        for (x, y) in ((6, top - 2), (8, top - 2), (7, top - 1), (7, top - 3)):
            px[y][x] = 12
    elif look == "acorn":                             # an acorn cap with a stalk
        for x in range(4, 12):
            px[top + 1][x] = 10
        for x in range(5, 11):
            px[top][x] = 11
        px[top - 1][8] = 10
    else:                                             # dew: a little drop
        px[top - 2][7] = 13
        px[top - 1][6] = 13; px[top - 1][7] = 6; px[top - 1][8] = 14
        px[top][7] = 14
    if swim:                                          # ripples round the swimmer
        for x in range(2, 14):
            px[14][x] = 6 if x % 3 else 13
            px[15][x] = 13 if x % 2 else 14


def leafen_home(px, kind):
    """A tiny Leafen home, smaller than a map tile: it sits at the bottom of a 16x16 cell."""
    def box(x0, y0, x1, y1, c, edge=1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                px[y][x] = edge if x in (x0, x1) or y in (y0, y1) else c
    def door(x, y):
        px[y][x] = px[y][x + 1] = px[y + 1][x] = px[y + 1][x + 1] = 1
    if kind == "mushroom":                            # a pink cap with white spots on a pale stem
        box(5, 9, 10, 15, 4)
        door(7, 13)
        for y in range(3, 10):
            for x in range(2, 14):
                d = ((x - 7.5) / 6) ** 2 + ((y - 9) / 6) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.78 or y == 9 else 7
        px[5][6] = px[6][10] = px[7][4] = 6
    elif kind == "tent":                              # a leaf folded into a tent
        for y in range(4, 16):
            w = (y - 4) // 2 + 1
            for x in range(8 - w, 8 + w):
                px[y][x] = 1 if x in (8 - w, 8 + w - 1) or y == 15 else 9 if x < 8 else 8
        for y in range(11, 15):
            px[y][7] = px[y][8] = 1
    elif kind == "acornhut":                          # an acorn with a round door
        for y in range(6, 16):
            for x in range(3, 13):
                d = ((x - 7.5) / 5) ** 2 + ((y - 11) / 5) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.75 else 11
        box(3, 5, 12, 7, 10)
        px[4][7] = px[4][8] = 10
        door(7, 12)
    elif kind == "pot":                               # a flower pot with a flower roof
        box(4, 10, 11, 15, 11)
        for x in range(5, 11):
            px[11][x] = 10
        for (x, y) in ((7, 4), (6, 5), (8, 5), (7, 6), (5, 4), (9, 4), (7, 3)):
            px[y][x] = 12
        px[5][7] = 6
        for y in range(7, 10):
            px[y][7] = 8
        door(7, 13)
    elif kind == "pebblehut":                         # stacked pebbles with moss
        box(4, 9, 11, 15, 15)
        box(5, 5, 10, 9, 15)
        px[5][6] = px[5][7] = px[9][5] = px[9][10] = 9
        door(7, 13)
    else:                                             # dewhut: a water-drop dome
        for y in range(3, 16):
            for x in range(3, 13):
                w = min(5.0, (y - 2) * 0.7)
                if abs(x - 7.5) < w:
                    edge = abs(x - 7.5) > w - 1.2 or y == 15
                    px[y][x] = 1 if edge else 13 if x < 8 else 14
        px[6][7] = px[7][6] = 6
        door(7, 13)


def make_leafen():
    img = Image.new("P", (16 * len(LEAFEN_ORDER), 16), 0)
    img.putpalette([c for rgb in LEAFEN_PAL for c in rgb])
    for i, name in enumerate(LEAFEN_ORDER):
        for y, row in enumerate(leafen_cell(name)):
            for x, c in enumerate(row):
                img.putpixel((16 * i + x, y), c)
    return img


def make_combat():
    img = Image.new("P", (16 * len(COMBAT_ORDER), 16), 0)
    img.putpalette([c for rgb in COMBAT_PAL for c in rgb])
    for i, name in enumerate(COMBAT_ORDER):
        for y, row in enumerate(combat_cell(name)):
            for x, c in enumerate(row):
                img.putpixel((16 * i + x, y), c)
    return img


BOSS_PAL = [
    (255, 0, 255),    # 0 clear
    (24, 18, 22),     # 1 outline
    (70, 74, 84),     # 2 metal dark
    (116, 122, 134),  # 3 metal
    (172, 178, 188),  # 4 metal light
    (156, 76, 40),    # 5 rust
    (236, 56, 48),    # 6 red eye
    (48, 74, 40),     # 7 thorn dark
    (92, 128, 60),    # 8 thorn
    (226, 214, 160),  # 9 thorn tip
    (78, 56, 40),     # 10 mud dark
    (124, 92, 62),    # 11 mud
    (170, 136, 96),   # 12 mud light
    (58, 26, 64),     # 13 rot dark
    (122, 58, 132),   # 14 rot
    (236, 220, 96),   # 15 glow
]
BOSS_ORDER = ["clamper0", "clamper1", "thorn0", "thorn1", "mud0", "mud1", "rot0", "rot1", "gloom0", "gloom1",
              "shield"]


def boss_cell(name):
    import math
    px = _canvas(32)
    f = name.endswith("1")
    if name.startswith("clamper"):             # a squat Vael machine with clamp jaws
        for y in range(10, 30):
            for x in range(5, 27):
                edge = x in (5, 26) or y in (10, 29)
                px[y][x] = 1 if edge else 4 if y < 13 else 3 if y < 24 else 2
        for x in range(7, 25, 4):
            px[27][x] = px[27][x + 1] = 1          # tread
        for y in range(15, 19):
            for x in range(13, 19):
                px[y][x] = 6 if 14 <= x <= 17 and y in (16, 17) else 1   # the eye
        gap = 4 if f else 1
        for side in (-1, 1):                       # jaws
            for y in range(2 + (0 if f else 2), 12):
                x0 = 16 + side * (gap + 2)
                for x in range(min(x0, x0 + side * 5), max(x0, x0 + side * 5) + 1):
                    if 0 <= x < 32:
                        px[y][x] = 1 if y in (2 + (0 if f else 2), 11) else 3
        for k in range(3, 29, 6):
            px[20][k] = 5
    elif name.startswith("thorn"):             # a bramble ball with spikes and eyes
        for y in range(32):
            for x in range(32):
                d = ((x - 15.5) ** 2 + (y - 17) ** 2) ** 0.5
                a = math.atan2(y - 17, x - 15.5)
                spike = 11.5 + (3.5 if f else 2) * max(0.0, math.cos(a * 7))
                if d < spike:
                    px[y][x] = 9 if d > spike - 1.5 and d > 11 else 1 if d > 10.5 else 8 if (x + y) % 5 else 7
        for ex in (11, 20):
            px[15][ex] = px[15][ex + 1] = 6
            px[16][ex] = px[16][ex + 1] = 1
    elif name.startswith("mud"):               # a hulking mud blob, cracked and drying
        sq = 2 if f else 0
        for y in range(32):
            for x in range(32):
                d = ((x - 15.5) / (13 + sq)) ** 2 + ((y - 18 - sq / 2) / (12 - sq)) ** 2
                if d < 1:
                    px[y][x] = 1 if d > 0.86 else 12 if y < 12 + sq else 11 if y < 22 else 10
        for k in range(8):
            px[14 + k][8 + (k % 3)] = 10
            px[16 + k // 2][22 - (k % 2)] = 10
        for ex in (11, 20):
            px[16 + sq][ex] = px[16 + sq][ex + 1] = 15
            px[17 + sq][ex] = px[17 + sq][ex + 1] = 1
    elif name.startswith("gloom"):             # Gloomheart: a dark, pulsing heart of rot with one eye
        beat = 1.0 if f else 0.88
        for y in range(32):
            for x in range(32):
                u, v = (x - 15.5) / (13 * beat), (17 - y) / (13 * beat)
                d = (u * u + (v - abs(u) * 0.6 + 0.2) ** 2)          # a heart shape
                if d < 1:
                    px[y][x] = 1 if d > 0.82 else 14 if d < 0.25 else 13
        for k in range(6):                                           # veins
            for t in range(10):
                x = int(15.5 + (t + 3) * math.cos(k * 1.05)), int(16 + (t + 3) * math.sin(k * 1.05))
                if 0 <= x[0] < 32 and 0 <= x[1] < 32 and px[x[1]][x[0]] == 13:
                    px[x[1]][x[0]] = 14
        for y in range(13, 19):
            for x in range(12, 20):
                if ((x - 15.5) / 3.5) ** 2 + ((y - 15.5) / 2.5) ** 2 < 1:
                    px[y][x] = 15 if abs(x - 15.5) > 1 else 1             # the eye
    elif name == "shield":                     # a ring around Gloomheart (colored per spirit at run time)
        for y in range(32):
            for x in range(32):
                d = ((x - 15.5) ** 2 + (y - 15.5) ** 2) ** 0.5
                if 13 <= d < 16:
                    px[y][x] = 1 if d >= 15.2 or d < 13.6 else 3 if (x + y) % 6 < 3 else 2
    else:                                      # a rot root rising from the ground, a glowing mouth
        lean = 2 if f else -1
        for y in range(3, 31):
            w = 4 + (y - 3) * 0.32
            cx = 15.5 + lean * math.sin(y * 0.2)
            for x in range(32):
                d = abs(x - cx)
                if d < w:
                    px[y][x] = 1 if d > w - 1 else 14 if x < cx - w * 0.3 else 13
        for x in range(10, 22):
            px[30][x] = 1
        for y in range(13, 17):
            for x in range(13, 19):
                px[y][x] = 15 if y in (14, 15) and 14 <= x <= 17 else 1
        px[9][13] = px[9][18] = 15
    return px


def make_boss():
    img = Image.new("P", (32 * len(BOSS_ORDER), 32), 0)
    img.putpalette([c for rgb in BOSS_PAL for c in rgb])
    for i, name in enumerate(BOSS_ORDER):
        for y, row in enumerate(boss_cell(name)):
            for x, c in enumerate(row):
                img.putpixel((32 * i + x, y), c)
    return img


LILY_PAL = [                       # v0.18 Lily, the water-lily spirit (OBJ bank 10)
    (255, 0, 255),    # 0 clear
    (36, 44, 48),     # 1 outline, eyes
    (44, 120, 64),    # 2 pad dark
    (92, 184, 96),    # 3 pad
    (252, 252, 248),  # 4 petal white
    (252, 186, 206),  # 5 petal pink
    (222, 112, 154),  # 6 petal deep pink
    (252, 222, 96),   # 7 center yellow
    (248, 146, 168),  # 8 cheeks
    (136, 204, 250),  # 9 water
    (78, 150, 222),   # 10 water dark
] + [(0, 0, 0)] * 5


def lily_cell(frame):
    """Lily (v0.18.4, Caleb's pick "pad hat"): a soft pink body wearing a lily pad as a wide hat, notched
    like a real pad, with a small lily blooming on top. Frame 1 bobs: the hat dips a pixel.
    Outlines are drawn outside each shape so the thin petals keep their color."""
    px = _canvas()
    def ell(cx, cy, rx, ry):
        return {(x, y) for y in range(16) for x in range(16) if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 < 1}
    def paint(shape, fill):
        for (x, y) in shape:
            px[y][x] = fill(x, y)
        for (x, y) in shape:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < 16 and 0 <= ny < 16 and (nx, ny) not in shape and px[ny][nx] == 0:
                    px[ny][nx] = 1
    d = frame
    body = ell(7.5, 11.0, 4.4, 3.6)
    paint(body, lambda x, y: 6 if y >= 13 else 5)
    hat = ell(7.5, 6.6 + 0.4 * d, 6.4, 1.8) - {(7, 7 + d), (8, 7 + d), (7, 8 + d)}   # the pad's notch
    for (x, y) in hat:
        px[y][x] = 0
    paint(hat, lambda x, y: 2 if y >= 7 + d else 3)
    # the bloom: a white middle petal, pink side petals, a yellow heart
    bloom = {(7, 1), (8, 1), (7, 2), (8, 2), (7, 3), (8, 3), (7, 4), (8, 4),   # middle petal
             (4, 2), (5, 3), (5, 4), (6, 4), (6, 3),                          # side petals
             (11, 2), (10, 3), (10, 4), (9, 4), (9, 3)}
    bloom = {(x, y + d) for (x, y) in bloom} - hat
    paint(bloom, lambda x, y: 4 if (7 <= x <= 8 and y <= 2 + d) or y <= 2 + d else 5)
    px[4 + d][7] = px[4 + d][8] = 7
    for x in (6, 9):                                   # eyes under the brim
        px[10][x] = px[11][x] = 1
    px[12][5] = px[12][10] = 8                         # cheeks
    px[12][7] = px[12][8] = 1                          # a small smile
    if frame:
        for x in (2, 3, 12, 13):
            px[15][x] = 9
    return px


def make_swim_hands():
    """v0.18.1: the wizard's hands treading water (two frames, drawn with his own palette, OBJ bank 0):
    coat sleeves and hands at the waterline, out to the sides, then in."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from player_palette import PAL
    img = Image.new("P", (32, 16), 0)
    img.putpalette([c for rgb in PAL for c in rgb])
    for f, (lx, rx) in enumerate(((1, 12), (3, 10))):
        for hx in (lx, rx):
            for y in range(9, 12):                    # a sleeve, then the hand beyond it
                for x in range(hx, hx + 3):
                    edge = y in (9, 11) or x in (hx, hx + 2)
                    inner = x == hx + 1 or (hx == lx and x == hx + 2) or (hx == rx and x == hx)
                    img.putpixel((16 * f + x, y), 1 if edge and not inner else 3)
            hand = hx - 1 if hx == lx else hx + 3
            for y in (10, 11):
                img.putpixel((16 * f + hand, y), 7 if y == 10 else 8)
            img.putpixel((16 * f + hand, 9), 1)
            img.putpixel((16 * f + hand, 12), 1)
    return img


# v0.18.4 the pouch, after Caleb's reference: a low, wide, floppy sack slumped on the ground, with a thick
# rolled rim, a drawstring at its neck whose ends dangle, Bark's leaf on the front, and the spirits as
# little balls sitting in its mouth. It fills out as spirits go in: 0-4 of them. Per state: the rim's half
# width and half height, the body's half width at its base and its height. The bottom sits on row 62.
POUCH_STATES = [(12, 4.0, 25, 18), (16, 4.6, 27, 19), (20, 5.0, 29, 20), (25, 5.3, 30, 21), (29, 5.5, 31, 22)]
POUCH_FRONT_Y = 32                     # the front layer holds rows 32-63 of the pouch


def pouch_mouth_y(state):
    """The rim's middle row: the balls sit with their middles just above it."""
    _, rry, _, h = POUCH_STATES[state]
    return 62 - h - round(rry)


def pouch_state(img, ox, state):
    """One state of the spirit pouch at column ox of the sheet, in two layers so the balls sit in its
    mouth: rows 0-63 the back (the sack, the rim and the dark opening, behind the balls), rows 64-95
    the front (pouch rows 32-63: the near half of the rim and the body, over the balls)."""
    R, rry, B, H = POUCH_STATES[state]
    cx, base = 31.5, 62
    top = base - H                                               # where the rim meets the body
    ry_ = pouch_mouth_y(state)                                   # the rim's middle
    rng = random.Random(800 + state)
    def in_rim(x, y):
        return ((x - cx) / (R + 0.5)) ** 2 + ((y - ry_) / (rry + 0.5)) ** 2 < 1
    def in_hole(x, y):
        return ((x - cx) / (R - 2.4)) ** 2 + ((y - ry_ + 0.8) / (rry - 1.7)) ** 2 < 1
    feet = {}                                                    # the base's ragged edge: a pixel up here and there
    for x in range(64):
        feet[x] = base - (1 if rng.random() < 0.18 else 0)
    def body_w(y, side):
        t = (y - top) / H
        if t < 0 or t > 1:
            return 0
        n0 = R * 0.7 + 1                                          # a neck under the rim (it overhangs)
        tt = max(0.0, (t - 0.18) / 0.82)                          # pinched by the string for a few rows,
        w = n0 + (B - n0) * math.sin(math.pi / 2 * tt) ** 0.7    # then it slumps out like a mound
        if t > 0.8:
            w += 1.5 * (t - 0.8) / 0.2                           # the cloth pools out on the ground
        w += (0.8 if side < 0 else -0.4) * t                     # leaning a little to the left
        w += 0.6 * math.sin(t * 9 + (0 if side < 0 else 2))      # soft lumps in its sides
        return w
    def in_body(x, y):
        return y <= feet.get(x, base) and -body_w(y, -1) < x - cx < body_w(y, 1)
    sack = {(x, y) for y in range(64) for x in range(64) if in_body(x, y) or in_rim(x, y)}
    def edge(shape, x, y):
        return any((x + dx, y + dy) not in shape for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
    back = {}
    for (x, y) in sack:
        if edge(sack, x, y):
            c = 1
        elif in_rim(x, y):
            c = 11 if y < ry_ + 1 else 10                        # the rolled lip: lit on top
        else:
            w = body_w(y, 1)                                     # lit from the left: the right side in shade,
            c = 10 if x - cx > w * 0.45 + 1 or y >= base - 1 else 11   # following the sack's curve
        back[(x, y)] = c
    for (x, y) in sack:                                          # the dark opening, a lighter inner wall at its back
        if in_hole(x, y):
            back[(x, y)] = 5 if y >= ry_ - rry + 1.6 else 10
    def line(x0, y0, x1, y1, c=10):
        n = int(max(abs(x1 - x0), abs(y1 - y0), 1))
        for i in range(n + 1):
            x, y = round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n)
            if (x, y) in sack and not edge(sack, x, y) and not in_rim(x, y):
                back[(x, y)] = c
    # floppy creases: fanning down from the neck, and folds where it sits on the ground
    line(cx - R * 0.5, top + 2, cx - B * 0.75, base - 3)
    line(cx - R * 0.15, top + 2, cx - B * 0.3, top + H * 0.6)
    line(cx + R * 0.35, top + 2, cx + B * 0.45, top + H * 0.55)
    line(cx - B * 0.55, base - 2, cx - B * 0.35, base - 4)
    line(cx + B * 0.1, base - 2, cx + B * 0.28, base - 4)
    if state == 0:
        line(cx - B * 0.6, top + H * 0.45, cx - B * 0.2, top + H * 0.6)   # empty: slack folds across it
    # Bark's leaf, low on the right: pointed, tilted, a vein and a stem
    lx, ly, a, L = cx + B * 0.52, top + H * 0.62, -0.45, 4.6
    leaf = set()
    for y in range(64):
        for x in range(64):
            u, v = (x - lx) * math.cos(a) + (y - ly) * math.sin(a), -(x - lx) * math.sin(a) + (y - ly) * math.cos(a)
            if abs(u) < L and abs(v) < L * 0.55 * (1 - (u / L) ** 2) ** 0.8 and (x, y) in sack:
                leaf.add((x, y))
    for (x, y) in leaf:
        up = -(x - lx) * math.sin(a) + (y - ly) * math.cos(a) < 0
        back[(x, y)] = 1 if edge(leaf, x, y) else 9 if up else 8
    for t in range(-int(L) + 1, int(L)):
        q = (round(lx + t * math.cos(a)), round(ly + t * math.sin(a)))
        if q in leaf and not edge(leaf, *q):
            back[q] = 8
    for t in range(1, 3):
        q = (round(lx - (L + t) * math.cos(a)), round(ly - (L + t) * math.sin(a)))
        if q in sack:
            back[q] = 1
    # the drawstring: round the neck just under the rim, its ends dangling down the front
    sy = ry_ + round(rry) + 1
    for x in range(64):
        if (x, sy) in sack and not edge(sack, x, sy) and not in_rim(x, sy):
            back[(x, sy)] = 12
    kx = round(cx - R * 0.35)                                     # a knot, and two ends hanging from it
    back[(kx, sy + 1)] = back[(kx + 1, sy + 1)] = 12
    for (x0, y0, x1, y1) in ((kx, sy + 2, kx - 2, sy + 6), (kx + 1, sy + 2, kx + 2, sy + 7)):
        n = int(max(abs(x1 - x0), abs(y1 - y0), 1))
        for i in range(n + 1):
            q = (round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n))
            if q in sack and not edge(sack, *q):
                back[q] = 12
    rx_ = round(cx + body_w(sy + 1, 1)) - 1                      # a loop of it hanging off the right side
    for q in ((rx_, sy + 1), (rx_ + 1, sy + 2), (rx_ + 1, sy + 3), (rx_, sy + 4)):
        back[q] = 12
    for (x, y), c in back.items():
        img.putpixel((ox + x, y), c)
    fy = 64 - POUCH_FRONT_Y                                      # the front layer: the near lip and the body
    for (x, y), c in back.items():
        if y >= POUCH_FRONT_Y and not in_hole(x, y) and (y >= ry_ or not in_rim(x, y)):
            img.putpixel((ox + x, y + fy), c)


def make_pouch():
    """v0.18.4 the spirit pouch Bark gives you, drawn in the pause menu with the Leafen's palette: five
    states side by side (0-4 spirits in it), each 64 wide, 96 tall (see pouch_state)."""
    img = Image.new("P", (64 * len(POUCH_STATES), 96), 0)
    img.putpalette([c for rgb in LEAFEN_PAL for c in rgb])
    for st in range(len(POUCH_STATES)):
        pouch_state(img, 64 * st, st)
    return img


# v0.18.4 each spirit as a little ball, for the pouch (Caleb's reference): its body colors (light, main,
# shade, in its own palette's indices), a face, and a tiny mark so you can tell them apart.
BALL_COLORS = [(9, 10, 11), (2, 3, 4), (2, 3, 4), (2, 3, 4), (2, 3, 4), (5, 5, 6)]   # Sprout..Lily
def ball_cell(kind):
    px = _canvas()
    lt, mid, sh = BALL_COLORS[kind]
    cx, cy, r = 7.5, 9.0, 5.4
    ball = {(x, y) for y in range(16) for x in range(16) if (x - cx) ** 2 + (y - cy) ** 2 < r * r}
    if kind == 1:                                        # Ripple: a drop's point on top
        ball |= {(7, 3), (8, 3), (7, 2)}
    for (x, y) in ball:
        e = any((x + dx, y + dy) not in ball for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        d = (x - cx) * 0.6 + (y - cy)
        px[y][x] = 1 if e else sh if d > 2.4 else mid
    for (x, y) in ((5, 6), (6, 5), (5, 7)):               # a shine, top left
        if px[y][x] not in (0, 1):
            px[y][x] = 4 if kind == 5 else lt
    px[9][6] = px[9][9] = px[8][6] = px[8][9] = 1          # eyes
    px[11][7] = px[11][8] = 1                              # a small smile
    px[10][6] = px[10][9] = px[10][6]
    if kind == 0:                                        # Sprout: two little leaves on top
        for (x, y) in ((6, 2), (5, 2), (9, 2), (10, 2), (7, 3), (8, 3)):
            px[y][x] = 10 if y == 2 else 11
    if kind == 2:                                        # Pebble: its little sprout
        px[2][8] = px[2][9] = 6; px[3][8] = 5
    if kind == 4:                                        # Bark: a leaf on his head
        px[2][9] = px[2][10] = 5; px[3][8] = 5
    if kind == 5:                                        # Lily: a tiny white bloom with a yellow heart
        px[2][7] = px[2][8] = 4; px[3][6] = px[3][9] = 4; px[3][7] = px[3][8] = 7
    return px


def make_balls():
    """One 16x16 ball per spirit, each drawn in its own spirit's palette indices (so it is drawn with
    that spirit's colors). The file's own palette is Lily's, only for viewing."""
    img = Image.new("P", (16 * len(BALL_COLORS), 16), 0)
    img.putpalette([c for rgb in LILY_PAL for c in rgb])
    for k in range(len(BALL_COLORS)):
        for y, row in enumerate(ball_cell(k)):
            for x, c in enumerate(row):
                img.putpixel((16 * k + x, y), c)
    return img


def make_lily():
    img = Image.new("P", (32, 16), 0)
    img.putpalette([c for rgb in LILY_PAL for c in rgb])
    for f in range(2):
        for y, row in enumerate(lily_cell(f)):
            for x, c in enumerate(row):
                img.putpixel((16 * f + x, y), c)
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
                     ("clamp_sprite.png", make_clamp), ("combat_sprite.png", make_combat),
                     ("boss_sprite.png", make_boss), ("hollow.png", make_hollow),
                     ("tree_alive.png", make_tree_alive), ("leaves.png", make_leaves),
                     ("leafen_sprite.png", make_leafen), ("lily_sprite.png", make_lily), ("pouch_sprite.png", make_pouch), ("ball_sprite.png", make_balls),
                     ("swim_sprite.png", make_swim_hands)):
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
