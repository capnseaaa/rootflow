"""Convert art/*.png, the font, and the starting maps into src/data.h."""
import os
import random
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
OUT = os.path.join(ROOT, "src", "data.h")

# Metatile indices (must match make_art.py META and main.c enum)
(DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE, THORN, HOMESITE,
 CHEST, CHEST_OPEN, WRECK, DRY_GRASS, PEBBLES, MENHIR, DRY_RIVER, DRY_BRIDGE, PATH, FLOWERBED, FLOWERBED_EMPTY,
 SAPLING, FRUIT_TREE, FRUIT_TREE_BARE, LANTERN, BENCH, DAM, LEVER, LEVER_PULLED, LOST_STONE, BLOSSOM, FOUNTAIN,
 STATUE, PINWHEEL) = range(38)
# after the pumps (38-39), 16 home tiles (40-55) and 9 home animation frames (56-64): v0.13 tiles
STUMP, STUMP_GRASS, CHEST_GRASS, CHEST_OPEN_GRASS, WRECK_GRASS = range(65, 70)
# v0.14: solid ground under the Great Tree's trunk (the trunk itself is its own layer), its roots,
# and Bark's quest pieces
TRUNK_BLOCK, ROOT_TIP_DEAD, ROOT_TIP_ALIVE, CLAMP, DRY_POOL = range(70, 75)
TRUNK_GRASS = 75
MUSH_LAMP, LEAF_SWING, ACORN_BENCH, FLOWER_ARCH, PEBBLE_PATH, DEW_POND = range(76, 82)
ROCK_GRASS = 82
SLUDGE, LILY_PAD, REEDS, VAEL_PIPE = range(83, 87)   # v0.18 the drowned marsh
# v0.18.1: the marsh's own metatiles (loaded only there, into the Great Tree's tile slots)
MARSH_META_FIRST = 87
BOG_WATER, BOG_WATER_B, BOG_PAD, MOSS, MUSHROOMS, SWAMP_TREE, SWAMP_TREE_ALIVE, LOG = range(87, 95)
SWAMP_TOP, SWAMP_TOP_ALIVE = 95, 96                # the crown above a swamp tree (two tiles tall)
# 97-111 bog shores, 112-126 clear water shores: picked by the game when it draws, never in a map
# v0.19 the burnt orchard's own (loaded like the marsh's); DARKNESS is never in a map (BG2 draws the dark)
(BURNT_TREE, BURNT_FRUIT_TREE, BURNT_LOG, LANTERN_COLD, LANTERN_LIT, CAMPFIRE_COLD, CAMPFIRE_LIT, LOG_SEAT,
 SMOKE_STACK, BURNT_FENCE, DARKNESS, ASH_LANE, CHARRED_BRUSH) = range(127, 140)
# v0.20 the mushroom grove's own (loaded like the marsh's)
(GIANT_MUSH, GIANT_MUSH_BLUE, GLOWCAPS, FAIRY_RING, HOLLOW_LOG, CRACKED_ROCK, FALLEN_CAP, TINY_CHEST,
 TINY_CHEST_OPEN) = range(140, 149)
ROOM_VOID = 149                                     # v0.20 the Leafen homes' insides start here (main.c lists them)
MOSS_TRAIL, GIANT_STEM, GIANT_STEM_BLUE = 180, 181, 182   # v0.20.4 the grove redone; caps 183-194 (BG3, in-game)
(MENHIR_GRASS, ARCH_L, ARCH_R, ARCH_OPEN, ARCH_RUBBLE, SEAL_STONE, SEAL_GLOW, CRATER, FALLEN_STAR, BUD_CLOSED,
 BUD_OPEN) = range(195, 206)                        # v0.21 the Mirror Meadow's own
MOON_PATH, MENHIR_LIT = 206, 207                    # v0.21.4 Nox's night: moonlight over the dry river, a lit stone

MAP_W = MAP_H = 32
PLAYER_START = (6, 18)
SPIRIT_SPOT = (13, 9)
RIPPLE_SPOT = (14, 24)
# Pebble sleeps in the Windy Steppe, past a dry riverbed (Ripple bridges it), among boulders.
STONE_AREA, STONE_SPOT = 1, (27, 23)
DRY_RIVER_X = (13, 14)            # steppe: a dry riverbed, two tiles wide, top to bottom
# Hidden spring: interior rectangle (x0, y0, x1, y1), walled by a ring of rocks.
POCKET = (26, 2, 30, 6)
# Ripple's quest: a rusted Vael dam at the head of the steppe's dry river, and its lever.
DAM_LEVER = (15, 1)
# Pebble's quest: its lost standing stone, walled in by rocks in the valley's top-left corner.
LOST_STONE_AT = (2, 2)
# Breeze sleeps in Stone Hollow, sealed in a boulder pocket (Pebble breaks in).
BREEZE_AREA, BREEZE_SPOT = 2, (3, 26)
BREEZE_POCKET = (2, 25, 5, 28)   # interior rectangle; walled by rocks
# Thorn grove in the bottom-left corner: interior x 0..6, y 26..31, walled by thorns.
THORN_GROVE = (0, 26, 6, 31)
# Areas: 0 = the valley, 1 = Windy Steppe (east of the valley), 2 = Stone Hollow (south).
VALLEY, EAST, SOUTH, WEST, HOLLOW_IN, MARSH, ORCHARD, GROVE, INTERIOR = 0, 1, 2, 3, 4, 5, 6, 7, 8
MEADOW, MIRROR = 9, 10             # v0.21 the Mirror Meadow (north of the steppe) and its reflection
NUM_AREAS = 11                     # 4: the hollow inside the Great Tree (v0.16); 5: the drowned marsh (v0.18);
                                   # 6: the burnt orchard (v0.19), north of the Great Tree
# v0.19 the burnt orchard: in from the Great Tree region's north edge into a small pocket at its south edge,
# walled off by fallen burnt trees (only YOU clears them); everything past them is dark.
ORCHARD_POCKET = (11, 28, 20, 31)                  # the pocket you arrive in (x0, y0, x1, y1)
ORCHARD_WALL_ROW = 27
ORCHARD_LANTERNS = [(14, 23), (16, 18), (14, 12), (16, 6), (10, 14), (5, 16), (21, 10), (26, 8)]   # (26, 8): Wick's
ORCHARD_FIRES = [(19, 24), (7, 4)]                 # cold campfires, a log seat on each side
ORCHARD_STACKS = [(25, 20), (6, 10), (24, 3)]      # Vael smoke stacks (Pebble smashes them)
ORCHARD_NEST = (4, 2)                              # Flick's burnt nest (v0.19 step 2), in a clearing
ORCHARD_LEAFEN = [(27, 10), (23, 21)]              # Wick (in the dark by a lantern), Soot (by a smoke stack)
# v0.20 the mushroom grove (south-east): in from the steppe's south edge and from Stone Hollow's east edge (Caleb:
# both). Cap is squashed under a fallen cap in its middle. Fairy rings: where Cap shrinks you (and grows you back).
GROVE_CAP = (16, 16)
GROVE_RINGS = [(8, 8), (24, 22), (12, 26)]
GROVE_LEAFEN = [(24, 6), (6, 20)]                   # Tuft (in a nook you reach through a hollow log), Button
GROVE_LOG = (24, 9)                                 # the hollow log: the nook's way in (tiny only)
GROVE_NOOK = (27, 27)                               # a tiny chest behind a cracked rock (step 4)
# v0.19 (step 4) a cold campfire with a log seat each side in the old regions too (Flick lights them; cook
# there). Not in the Great Tree's region: its tile set fills video memory (the orchard has its own two).
CAMPFIRES = [(0, 26, 16), (1, 8, 18), (2, 3, 17), (5, 12, 16)]   # off the ways people walk
# v0.18 the drowned marsh, south-west: in from the Great Tree region's south edge (these columns)
MARSH_GATE_COLS = (14, 15, 16, 17)
MARSH_PIPES = [(7, 10), (23, 6), (12, 24)]         # Vael pipes on the banks, each pouring into its sludge
MARSH_LEAFEN = [(17, 26), (26, 26), (7, 12)]       # Bog (isle in sludge), Reedy (in cattails), Puddle (by sludge)
MARSH_BANK_ROW = 19                                # the way to Stone Hollow, along the river's north bank
WEST_GATE_ROWS = (12, 13, 14)      # valley x=0 <-> the Great Tree's region x=31 (opens when Bark joins)
# The Great Tree (west): Bark's quest pieces, placed on the roots drawn by make_art.py (v0.14).
# Clamps are sprites sitting on the side roots (pixel spots); under each, a solid CLAMP cell.
import make_art as _art  # noqa: E402
import rooms  # noqa: E402
CLAMP_PX = [(x, y) for x, y, _band in _art.ALL_CLAMPS]   # one 32x32 sprite each, in this order
CLAMPS = [(x // 16, y // 16) for x, y in CLAMP_PX]
ROOT_TIPS = []                                       # one cell past each root that ends pointing down
for _pts, _w0, _w1 in _art.ROOTS:
    _x, _y = _pts[-1]
    if _pts[-1][1] > _pts[-2][1] and 0 <= _x < 256:
        for _xx in (_x, _art.mirror_x(_x)):
            ROOT_TIPS.append((_xx // 16, (_y + _w1 // 2 + 15) // 16))
# v0.17.1 (Caleb): the root pool is a little bigger and an irregular oval (it fits between the roots)
ROOT_POOL = [(15, 25), (16, 25), (14, 26), (15, 26), (16, 26), (17, 26), (14, 27), (15, 27), (16, 27), (17, 27),
             (18, 27), (15, 28), (16, 28), (17, 28)]
THORN_WALL = [(x, 21) for x in range(13, 19)]       # brambles in front of the trunk's opening
EAST_GATE_ROWS = (19, 20, 21)      # valley x=31 <-> steppe x=0
SOUTH_GATE_COLS = (14, 15, 16)     # valley y=31 <-> hollow y=0
# Home sites, one per spirit kind (Sprout, Ripple, Pebble, Breeze): (area, x, y).
HOME_SITES = [(VALLEY, 3, 28), (VALLEY, 28, 5), (SOUTH, 16, 15), (EAST, 20, 15)]
# Treasure chests (area, x, y); the reward for each is in main.c (chest_reward).
CHESTS = [(VALLEY, 5, 30), (EAST, 28, 4), (SOUTH, 26, 27), (VALLEY, 27, 2)]   # last: in the hidden spring
# Rusted Vael machines with lore (area, x, y).
WRECKS = [(VALLEY, 18, 6), (EAST, 9, 25), (SOUTH, 5, 6)]


def rgb15(r, g, b):
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def palette_of(img):
    raw = img.getpalette()[:48]
    return [rgb15(*raw[i * 3:i * 3 + 3]) for i in range(16)]


def tile_words(img, ox, oy):
    """One 8x8 4bpp tile as eight 32-bit words."""
    words = []
    for y in range(8):
        w = 0
        for x in range(8):
            w |= (img.getpixel((ox + x, oy + y)) & 15) << (4 * x)
        words.append(w)
    return words


def strip_16x16(img):
    """Each 16x16 cell becomes 4 tiles in order TL, TR, BL, BR."""
    return strip_cells(img, 16)


def strip_cells(img, cell_h):
    """Each 16 x cell_h cell becomes 8x8 tiles in row-major order (the GBA's 1D sprite layout)."""
    words = []
    for i in range(img.width // 16):
        for ty in range(0, cell_h, 8):
            for tx in (0, 8):
                words += tile_words(img, i * 16 + tx, ty)
    return words


# ---- 5x7 font ----
FONT = {
    "+": "00000 00100 00100 11111 00100 00100 00000",
    "/": "00001 00010 00010 00100 01000 01000 10000",
    "-": "00000 00000 00000 11111 00000 00000 00000",
    ",": "00000 00000 00000 00000 00110 00100 01000",
    "A": "01110 10001 10001 11111 10001 10001 10001",
    "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110",
    "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111",
    "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111",
    "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110",
    "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001",
    "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001",
    "N": "10001 10001 11001 10101 10011 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110",
    "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101",
    "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110",
    "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110",
    "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 10101 01010",
    "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100",
    "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110",
    "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111",
    "3": "11111 00010 00100 00010 00001 10001 01110",
    "4": "00010 00110 01010 10010 11111 00010 00010",
    "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110",
    "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110",
    "9": "01110 10001 10001 01111 00001 00010 01100",
    "%": "11001 11010 00010 00100 01000 01011 10011",
    ":": "00000 01100 01100 00000 01100 01100 00000",
    "!": "00100 00100 00100 00100 00100 00000 00100",
    ".": "00000 00000 00000 00000 00000 01100 01100",
    "?": "01110 10001 00001 00010 00100 00000 00100",
    "'": "00100 00100 01000 00000 00000 00000 00000",
    " ": "00000 00000 00000 00000 00000 00000 00000",
}
# Window pieces (8x8, colors: 2 window, 3 gold, 4 dark gold, 0 clear) for the Emerald-style menu,
# plus the cursor arrow (1 = text color). Each is a character in FONT_CHARS.
TL = ["00444444", "04333333", "43333333", "43344444", "43342222", "43342222", "43342222", "43342222"]
WINDOW8 = {
    "[": TL,
    "]": [r[::-1] for r in TL],
    "{": TL[::-1],
    "}": [r[::-1] for r in TL[::-1]],
    "^": ["44444444", "33333333", "33333333", "44444444"] + ["22222222"] * 4,
    "_": ["22222222"] * 4 + ["44444444", "33333333", "33333333", "44444444"],
    "(": ["43342222"] * 8,
    ")": ["22224334"] * 8,
    ">": ["22222222", "21222222", "21122222", "21112222", "21122222", "21222222", "22222222", "22222222"],
    "<": ["22222222", "22222212", "22222112", "22221112", "22222112", "22222212", "22222222", "22222222"],
    # hearts (5 = heart red, 4 = outline): full and empty
    "*": ["22222222", "25525522", "55555552", "55555552", "25555522", "22555222", "22252222", "22222222"],
    "@": ["22222222", "24424422", "42242242", "42222242", "24222422", "22424222", "22242222", "22222222"],
}
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%:!.?'+/-,[]{}^_()>*@<"


def font_words():
    """Tile 0 is transparent. Glyph tiles: color 2 background, color 1 text."""
    words = [0] * 8
    for ch in FONT_CHARS:
        if ch in WINDOW8:
            for row in WINDOW8[ch]:
                words.append(sum(int(c) << (4 * x) for x, c in enumerate(row)))
            continue
        rows = FONT[ch].split()
        for y in range(8):
            w = 0
            for x in range(8):
                c = 2
                if 1 <= x <= 5 and y < 7 and rows[y][x - 1] == "1":
                    c = 1
                w |= c << (4 * x)
            words.append(w)
    return words


# ---- title banner: "ROOTFLOW" at 3x scale with a drop shadow ----
# Each letter fills a 16x24 cell (2 x 3 tiles). Colors: 1 = cream letter, 3 = dark shadow, 0 = clear.
BANNER_TEXT = "ROOTFLOW"


def banner_words():
    w, h = 16 * len(BANNER_TEXT), 24
    px = [[0] * w for _ in range(h)]
    for i, ch in enumerate(BANNER_TEXT):
        rows = FONT[ch].split()
        for dx, dy, col in ((1, 1, 3), (0, 0, 1)):
            for ry, row in enumerate(rows):
                for rx, bit in enumerate(row):
                    if bit == "1":
                        for yy in range(3):
                            for xx in range(3):
                                x = i * 16 + 1 + rx * 3 + xx + dx
                                y = 1 + ry * 3 + yy + dy
                                if x < w and y < h:
                                    px[y][x] = col
    words = []
    # tile order: row of tiles by row of tiles (map columns = 2 per letter, 3 tile rows)
    for ty in range(3):
        for tx in range(w // 8):
            for y in range(8):
                v = 0
                for x in range(8):
                    v |= px[ty * 8 + y][tx * 8 + x] << (4 * x)
                words.append(v)
    return words


# ---- starting map ----
def make_valley():
    rng = random.Random(2026)
    m = [[DEAD] * MAP_W for _ in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(MAP_W):
            r = rng.random()
            if r < 0.30:
                m[y][x] = DEAD_CRACK
            elif r < 0.36:
                m[y][x] = DEAD_TREE
            elif r < 0.40:
                m[y][x] = ROCK
    # winding river, 2 tiles wide
    x = 21
    for y in range(MAP_H):
        for dx in (0, 1):
            m[y][x + dx] = WATER
        if y % 2 == 0:
            x = max(18, min(26, x + rng.choice([-1, 0, 1])))
            m[y][x] = WATER
            m[y][x + 1] = WATER
    # a small pond on the near side
    for y in range(24, 27):
        for x in range(9, 13):
            m[y][x] = WATER
    # hidden spring on the far bank: a closed ring of rocks around a dead grove
    x0, y0, x1, y1 = POCKET
    for y in range(y0 - 1, y1 + 2):
        for x in range(x0 - 1, x1 + 2):
            inside = x0 <= x <= x1 and y0 <= y <= y1
            m[y][x] = (DEAD if (x + y) % 3 else DEAD_CRACK) if inside else ROCK
    for x, y in ((28, 3), (29, 3), (28, 4), (29, 4)):
        m[y][x] = WATER
    for x, y in ((26, 2), (30, 6), (26, 5)):
        m[y][x] = DEAD_TREE
    # thorn grove: a dead orchard behind a wall of brambles (map edges close the rest)
    x0, y0, x1, y1 = THORN_GROVE
    for y in range(y0 - 1, y1 + 1):
        for x in range(x0, x1 + 2):
            inside = x0 <= x <= x1 and y0 <= y <= y1
            m[y][x] = (DEAD_TREE if (x * 7 + y * 3) % 5 == 0 else DEAD) if inside else THORN
    # keep the start and spirit areas open
    for cx, cy in (PLAYER_START, SPIRIT_SPOT, RIPPLE_SPOT):
        for y in range(cy - 1, cy + 2):
            for x in range(cx - 1, cx + 2):
                m[y][x] = DEAD
    # exits: open ground to the east edge (far bank) and to the south edge
    for y in EAST_GATE_ROWS:
        for x in range(26, 32):
            if m[y][x] != WATER:
                m[y][x] = DEAD
    for x in SOUTH_GATE_COLS:
        for y in range(27, 32):
            m[y][x] = DEAD
    for y in WEST_GATE_ROWS:                     # the way west, to the Great Tree
        for x in range(0, 5):
            m[y][x] = DEAD
    stumpify(m, 2027, skip=[POCKET, THORN_GROVE])
    lx, ly = LOST_STONE_AT                        # Pebble's lost stone, sealed in by rocks
    for y in range(ly - 1, ly + 2):
        for x in range(lx - 1, lx + 2):
            m[y][x] = ROCK
    m[ly][lx] = LOST_STONE
    place_objects(m, VALLEY)
    return m


def stumpify(m, seed, skip=()):
    """About a third of the scattered dead trees are only stumps (v0.13). Both block the way and
    both grow back into trees when a bloom reaches them. skip: (x0, y0, x1, y1) areas to keep."""
    rng = random.Random(seed)
    for y in range(MAP_H):
        for x in range(MAP_W):
            if m[y][x] != DEAD_TREE or any(x0 - 1 <= x <= x1 + 1 and y0 - 1 <= y <= y1 + 1
                                           for x0, y0, x1, y1 in skip):
                continue
            if rng.random() < 0.35:
                m[y][x] = STUMP


def open_around(m, x, y, solid=(ROCK, DEAD_TREE, THORN, STUMP)):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if 0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H and m[y + dy][x + dx] in solid:
            m[y + dy][x + dx] = DEAD


def place_objects(m, area):
    """Home sites, chests, and wrecks for one area, each with open ground beside it."""
    for a, x, y in HOME_SITES:
        if a == area:
            open_around(m, x, y)
            m[y][x] = HOMESITE
    for a, x, y in CHESTS:
        if a == area:
            open_around(m, x, y, solid=(DEAD_TREE,))   # chests sit inside walls: never open them
            m[y][x] = CHEST
    for a, x, y in WRECKS:
        if a == area:
            open_around(m, x, y)
            m[y][x] = WRECK


def scatter(rng, crack=0.30, tree=0.06, rock=0.04, thorn=0.0):
    m = [[DEAD] * MAP_W for _ in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(MAP_W):
            r = rng.random()
            if r < crack:
                m[y][x] = DEAD_CRACK
            elif r < crack + tree:
                m[y][x] = DEAD_TREE
            elif r < crack + tree + rock:
                m[y][x] = ROCK
            elif r < crack + tree + rock + thorn:
                m[y][x] = THORN
    return m


def ring(m, x0, y0, x1, y1, wall, inner=DEAD):
    """A closed wall around the interior rectangle x0..x1, y0..y1."""
    for y in range(y0 - 1, y1 + 2):
        for x in range(x0 - 1, x1 + 2):
            if 0 <= x < MAP_W and 0 <= y < MAP_H:
                inside = x0 <= x <= x1 and y0 <= y <= y1
                m[y][x] = inner if inside else wall


def make_east():
    """Windy Steppe: dry dead grassland crossed by thorn hedges. Breeze's nook sits inside
    a thorn ring; a chest hides in a thorn-walled corner; one small pond."""
    rng = random.Random(3030)
    m = scatter(rng, crack=0.25, tree=0.05, rock=0.02, thorn=0.03)
    for y0 in (6, 12, 26):                       # long thorn hedges with gaps
        for x in range(2, 30):
            if (x + y0) % 9 not in (0, 1, 2):
                m[y0][x] = THORN
    ring(m, 18, 13, 22, 17, THORN)               # Breeze's nook
    ring(m, 26, 2, 30, 6, THORN)                 # chest corner (map edge closes the top)
    for y in range(20, 23):                      # small pond
        for x in range(23, 26):
            m[y][x] = WATER
    for y in EAST_GATE_ROWS:                     # entry path from the valley
        for x in range(0, 5):
            m[y][x] = DEAD
    retile(m, DEAD_CRACK, DRY_GRASS)             # steppe ground: tall dry grass
    stumpify(m, 3031)
    for y in range(MAP_H):                       # the dry riverbed splits the steppe
        for x in DRY_RIVER_X:
            m[y][x] = DRY_RIVER
    for x in DRY_RIVER_X:                        # Vael's dam holds the water back at the north end
        m[0][x] = DAM
    lx, ly = DAM_LEVER
    open_around(m, lx, ly)
    m[ly][lx] = LEVER
    # Pebble's resting place: a clearing among boulders (it looks like one of them while asleep)
    sx, sy = STONE_SPOT
    for y in range(sy - 1, sy + 2):
        for x in range(sx - 1, sx + 2):
            m[y][x] = DEAD
    for x, y in ((sx - 2, sy - 1), (sx + 2, sy), (sx - 1, sy + 2), (sx + 1, sy - 2), (sx + 2, sy + 2)):
        m[y][x] = ROCK
    place_objects(m, EAST)
    return m


def make_south():
    """Stone Hollow: a rocky basin. Pebble's cairn sits inside a ring of boulders, a chest
    hides behind a double rock wall, and a pond feeds big blooms."""
    rng = random.Random(4040)
    m = scatter(rng, crack=0.25, tree=0.04, rock=0.12)
    ring(m, 14, 13, 18, 17, ROCK)                # Pebble's cairn
    ring(m, 25, 26, 28, 29, ROCK)                # chest: double wall
    for x in range(23, 31):
        for y in (24, 31):
            if 0 <= y < MAP_H:
                m[y][x] = ROCK
    for y in range(24, 32):
        m[y][23] = ROCK
    for y in range(18, 24):                      # a round pond that the river runs into (v0.13)
        for x in range(5, 13):
            if ((x - 8.5) / 3.6) ** 2 + ((y - 20.5) / 2.8) ** 2 < 1:
                m[y][x] = WATER
    for x in SOUTH_GATE_COLS:                    # entry path from the valley
        for y in range(0, 5):
            m[y][x] = DEAD
    river_south(m)
    retile(m, DEAD_CRACK, PEBBLES)               # hollow ground: loose pebbles
    stumpify(m, 4042, skip=[BREEZE_POCKET])
    ring(m, *BREEZE_POCKET, ROCK)                # Breeze's boulder pocket
    # landmark: a ring of standing stones around open ground
    cx, cy = 6, 13
    for dx, dy in ((0, -2), (2, -1), (2, 1), (0, 2), (-2, 1), (-2, -1)):
        m[cy + dy][cx + dx] = MENHIR
    for y in range(cy - 1, cy + 2):
        for x in range(cx - 1, cx + 2):
            m[y][x] = DEAD
    place_objects(m, SOUTH)
    return m


def river_south(m):
    """The valley's river goes on into Stone Hollow (Caleb, v0.12.1): it enters at the top in the
    same columns where it leaves the valley, winds south past Pebble's cairn, then turns west
    through the pond to the west edge. Breeze's pocket and the chest lie beyond it (Ripple bridges)."""
    rng = random.Random(4041)
    valley = make_valley()
    x = min(c for c in range(MAP_W) if valley[MAP_H - 1][c] == WATER)
    turn = 19
    for y in range(turn + 2):                    # south, two tiles wide
        if y and y % 2 == 0 and y < turn:
            x = max(21, min(25, x + rng.choice([-1, 0, 1])))
        for dx in (0, 1):
            m[y][x + dx] = WATER
    y = turn
    for cx in range(x + 1, -1, -1):              # then west, two tiles tall
        if cx < x and cx % 2 == 0:
            y = max(19, min(22, y + rng.choice([-1, 0, 1])))
        for dy in (0, 1):
            m[y + dy][cx] = WATER


def retile(m, old, new):
    for row in m:
        for x, t in enumerate(row):
            if t == old:
                row[x] = new


def make_west():
    """The Great Tree's region: dead ground full of Vael's logging stumps. The giant trunk (its
    own layer, art/tree_trunk.png) stands on solid ground; roots spread below it, clamped and
    dry, with brambles in front of the opening at its foot."""
    rng = random.Random(5050)
    m = scatter(rng, crack=0.30, tree=0.05, rock=0.03)
    stumpify(m, 5051)
    for y in range(MAP_H):                       # more stumps: Vael logged the whole wood
        for x in range(MAP_W):
            if m[y][x] in (DEAD, DEAD_CRACK) and rng.random() < 0.06:
                m[y][x] = STUMP
    trunk = Image.open(os.path.join(ART, "tree_trunk.png"))
    for y in range(MAP_H):
        for x in range(MAP_W):
            near = any(trunk.getpixel((min(511, x * 16 + dx), min(511, y * 16 + dy)))
                       for dx in (0, 8, 15) for dy in (0, 8, 15))
            if y >= 16 or near:                      # open ground by and below the tree
                m[y][x] = DEAD if (x + y) % 3 else DEAD_CRACK
            cx, cy = x * 16 + 8, y * 16 + 8
            if (cy >= _art.TRUNK_TOP and abs(cx - _art.TREE_CX) <= _art.trunk_half(cy)
                    and cy <= _art.trunk_bottom(cx)):
                m[y][x] = TRUNK_BLOCK                # the trunk stands here: solid (its roots are not)
    for y in range(28, MAP_H):                   # a few stumps again at the bottom
        for x in range(MAP_W):
            if rng.random() < 0.05 and m[y][x] in (DEAD, DEAD_CRACK):
                m[y][x] = STUMP
    for y in WEST_GATE_ROWS:                     # the way back east, to the valley
        for x in range(24, 32):
            m[y][x] = DEAD
    for y in range(17, MAP_H):                   # the roots (drawn on the trunk layer) lie on open ground
        for x in range(MAP_W):
            if m[y][x] == STUMP or m[y][x] == DEAD_TREE or m[y][x] == ROCK:
                if trunk.getpixel((x * 16 + 8, y * 16 + 8)):
                    m[y][x] = DEAD
    for x, y in CLAMPS:
        m[y][x] = CLAMP
    for x, y in ROOT_TIPS:
        m[y][x] = ROOT_TIP_DEAD
    for x, y in THORN_WALL:
        m[y][x] = THORN
    for x, y in ROOT_POOL:
        m[y][x] = DRY_POOL
    return m


def make_marsh():
    """The drowned marsh (v0.18.1, after Caleb's swamp references): dark bog water everywhere, winding
    mossy banks with swamp trees, cattails, logs and mushrooms; Stone Hollow's river flowing in from the
    east; three Vael pipes pouring sludge into the water; three Leafen in trouble."""
    rng = random.Random(6061)
    m = [[BOG_WATER] * MAP_W for _ in range(MAP_H)]
    def land(x, y):
        if 0 <= x < MAP_W and 0 <= y < MAP_H:
            m[y][x] = DEAD
    def isle(cx, cy, rx, ry):
        for y in range(MAP_H):
            for x in range(MAP_W):
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if d < 1 - 0.3 * rng.random():
                    land(x, y)
    banks = set()                                     # the walkway cells (kept free of trees and logs)
    def path(points):                                 # a winding bank, two wide
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            n = max(abs(x1 - x0), abs(y1 - y0))
            for k in range(n + 1):
                x = round(x0 + (x1 - x0) * k / n) + (rng.random() < 0.25) * rng.choice((-1, 1))
                y = round(y0 + (y1 - y0) * k / n)
                for dx in (0, 1):
                    for dy in (0, 1):
                        land(x + dx, y + dy)
                        banks.add((x + dx, y + dy))
    for _ in range(16):                               # mossy islands
        isle(rng.randrange(2, 30), rng.randrange(2, 30), rng.uniform(1.6, 3.6), rng.uniform(1.4, 3.0))
    path([(15, 0), (15, 5), (10, 7), (5, 8), (5, 14), (9, 16), (15, 15), (22, 15), (27, 13), (28, 8), (24, 4),
          (18, 3)])
    path([(15, 15), (14, 21), (9, 22), (6, 27), (11, 29), (20, 29), (25, 28), (27, 23), (26, 17)])
    path([(22, 15), (26, 17), (29, MARSH_BANK_ROW - 1), (31, MARSH_BANK_ROW - 1)])
    for y in range(0, 4):                             # the way in from the Great Tree
        for x in range(13, 19):
            land(x, y)
    for x in range(20, MAP_W):                        # Stone Hollow's river winds in from the east (rows 20-22 at
        mid = 21 + round(1.4 * __import__("math").sin((MAP_W - 1 - x) * 0.55))   # the edge) and fades into the bog
        if x >= MAP_W - 2:
            mid = 21
        for y in range(mid - 1, mid + 2):
            m[y][x] = BOG_WATER                       # v0.18.2 (Caleb): only bog water and sludge in the marsh
    for x in range(26, MAP_W):
        land(x, MARSH_BANK_ROW)
    # sludge: each pipe stands on a bank and fouls the water next to it
    for (px_, py_), (sx, sy, rx, ry) in zip(MARSH_PIPES, ((9.5, 10, 2.6, 2.2), (23, 8.5, 2.4, 1.8), (16, 25.5, 4, 2.4))):
        for y in range(MAP_H):
            for x in range(MAP_W):
                if m[y][x] == BOG_WATER and ((x - sx) / rx) ** 2 + ((y - sy) / ry) ** 2 < 1:
                    m[y][x] = SLUDGE
        land(px_, py_)
        m[py_][px_] = VAEL_PIPE
        for (x, y) in ((px_ + 1, py_), (px_ - 1, py_), (px_, py_ + 1), (px_, py_ - 1)):
            if 0 <= x < MAP_W and 0 <= y < MAP_H and m[y][x] == BOG_WATER:
                m[y][x] = SLUDGE                      # it always touches its sludge
    bx, by = MARSH_LEAFEN[0]                          # Bog: a one-tile isle in the sludge
    for y in range(by - 1, by + 2):
        for x in range(bx - 1, bx + 2):
            m[y][x] = SLUDGE
    m[by][bx] = DEAD
    rx_, ry_ = MARSH_LEAFEN[1]                        # Reedy: on a bank, in a ring of cattails
    for y in range(ry_ - 2, ry_ + 3):
        for x in range(rx_ - 2, rx_ + 3):
            land(x, y)
    for y in range(ry_ - 1, ry_ + 2):
        for x in range(rx_ - 1, rx_ + 2):
            if (x, y) != (rx_, ry_):
                m[y][x] = REEDS
    qx, qy = MARSH_LEAFEN[2]                          # Puddle: on the bank, sludge right beside her
    land(qx, qy); land(qx - 1, qy)
    if m[qy][qx + 1] != VAEL_PIPE:
        m[qy][qx + 1] = SLUDGE
    # dress the banks: moss, mushrooms, swamp trees, cattails by the water, logs, a few rocks
    keep = {(bx, by), (rx_, ry_), (qx, qy), (qx - 1, qy)}
    keep |= {(x, y) for y in range(0, 4) for x in range(13, 19)}
    for y in range(MAP_H):
        for x in range(MAP_W):
            if m[y][x] != DEAD or (x, y) in keep:
                continue
            wet = any(0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H and m[y + dy][x + dx] in (BOG_WATER, SLUDGE, WATER)
                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            r = rng.random()
            if r < 0.30:
                m[y][x] = MOSS
            elif r < 0.34:
                m[y][x] = MUSHROOMS
            elif r < 0.40:
                m[y][x] = DEAD_CRACK
            elif wet and r < 0.50 and not on_path(m, x, y):
                m[y][x] = REEDS
    for y in range(1, MAP_H):                         # trees (two tall) and logs only off the main banks
        for x in range(MAP_W):
            if m[y][x] in (DEAD, MOSS) and (x, y) not in keep and (x, y) not in banks:
                r = rng.random()
                if r < 0.22 and m[y - 1][x] in (DEAD, MOSS, MUSHROOMS, DEAD_CRACK) and (x, y - 1) not in keep \
                        and (x, y - 1) not in banks:
                    m[y][x] = SWAMP_TREE
                    m[y - 1][x] = SWAMP_TOP
                elif r < 0.23:
                    m[y][x] = LOG
    for y in range(MAP_H):                            # lily pads floating on the open bog
        for x in range(MAP_W):
            if m[y][x] == BOG_WATER and rng.random() < 0.06 and all(
                    not (0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H) or m[y + dy][x + dx] != SLUDGE
                    for dx in range(-2, 3) for dy in range(-2, 3)):
                m[y][x] = BOG_PAD
    # everything that matters can be walked to: carve a bank to any that cannot (never through sludge)
    pipes = set(MARSH_PIPES)
    targets = [(qx, qy), (rx_, ry_ - 2), (MAP_W - 1, MARSH_BANK_ROW)]
    targets += [(x, y) for (px_, py_) in MARSH_PIPES for (x, y) in ((px_, py_ - 1), (px_ - 1, py_)) if m[y][x] != SLUDGE][:3]
    targets += [(bx, by - 3)]                         # the shore north of Bog's pool
    for tx, ty in targets:
        carve_to(m, tx, ty)
    for y in range(MAP_H):                            # a carved bank may cut a tree in half: tidy those
        for x in range(MAP_W):
            if m[y][x] == SWAMP_TOP and (y + 1 >= MAP_H or m[y + 1][x] != SWAMP_TREE):
                m[y][x] = MOSS
            if m[y][x] == SWAMP_TREE and (y == 0 or m[y - 1][x] != SWAMP_TOP):
                m[y][x] = MOSS
    return m



def carve_to(m, tx, ty):
    """Make (tx, ty) walkable and joined to the way in (15, 0): the cheapest way through bog water."""
    from collections import deque
    walk = {DEAD, DEAD_CRACK, MOSS, MUSHROOMS}
    m[ty][tx] = m[ty][tx] if m[ty][tx] in walk else DEAD
    seen = {(15, 0)}
    q = deque([(15, 0)])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if 0 <= n[0] < MAP_W and 0 <= n[1] < MAP_H and n not in seen and m[n[1]][n[0]] in walk:
                seen.add(n); q.append(n)
    if (tx, ty) in seen:
        return
    prev = {(tx, ty): None}                           # search from the target, through anything but sludge
    q = deque([(tx, ty)])
    while q:
        c = q.popleft()
        if c in seen:
            while c is not None:
                if m[c[1]][c[0]] not in walk:
                    m[c[1]][c[0]] = DEAD
                c = prev[c]
            return
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (c[0] + dx, c[1] + dy)
            if 0 <= n[0] < MAP_W and 0 <= n[1] < MAP_H and n not in prev and m[n[1]][n[0]] not in (SLUDGE, VAEL_PIPE, WATER):
                prev[n] = c
                q.append(n)


def on_path(m, x, y):
    """True when a tile is part of a two-wide bank (keep those clear so the marsh can be walked)."""
    open_ = lambda xx, yy: 0 <= xx < MAP_W and 0 <= yy < MAP_H and m[yy][xx] in (DEAD, MOSS, MUSHROOMS, DEAD_CRACK)
    return (open_(x - 1, y) and open_(x + 1, y)) or (open_(x, y - 1) and open_(x, y + 1))


def make_orchard():
    """The burnt orchard (v0.19): ash ground in rows of burnt fruit trees, old lanes with cold lanterns and
    charred fences, burnt woods all round, two cold campfires, three Vael smoke stacks, and a small pocket at
    the south edge (the way in) walled off by a fallen burnt trunk."""
    rng = random.Random(7070)
    m = [[DEAD_CRACK if rng.random() < 0.25 else DEAD for _ in range(MAP_W)] for _ in range(MAP_H)]
    lanes = set()
    import math
    def lane(x0, y0, x1, y1):                         # old lanes, packed dark ash (Caleb: darker, greyish),
        n = max(abs(x1 - x0), abs(y1 - y0), 1)        # wandering a tile to either side (not ruler straight)
        phase, last = rng.uniform(0, 6.3), None
        for k in range(n + 1):
            x, y = round(x0 + (x1 - x0) * k / n), round(y0 + (y1 - y0) * k / n)
            wob = round(0.9 * math.sin(k * 0.45 + phase)) if 2 <= k <= n - 2 else 0
            if x0 == x1:
                x += wob
            else:
                y += wob
            if last and last[0] != x and last[1] != y:  # keep it joined where it steps aside
                m[last[1]][x] = ASH_LANE; lanes.add((x, last[1]))
            m[y][x] = ASH_LANE
            lanes.add((x, y))
            last = (x, y)
    lane(15, 26, 15, 3)                               # the main lane north from the wall, and its branches
    lane(15, 15, 4, 15); lane(4, 15, 4, 4)
    lane(15, 9, 27, 9); lane(27, 9, 27, 16)
    lane(15, 21, 25, 21)
    lane(15, 3, 8, 3)
    keep = set(lanes)
    def clear(x, y, r=1):
        for yy in range(y - r, y + r + 1):
            for xx in range(x - r, x + r + 1):
                if 0 <= xx < MAP_W and 0 <= yy < MAP_H:
                    keep.add((xx, yy))
    for (x, y) in ORCHARD_LANTERNS + ORCHARD_FIRES + ORCHARD_LEAFEN:
        clear(x, y)
    for (x, y) in ORCHARD_STACKS:
        clear(x, y)
    clear(*ORCHARD_NEST, r=1)
    x0, y0, x1, y1 = ORCHARD_POCKET
    for y in range(y0 - 1, MAP_H):
        for x in range(x0 - 3, x1 + 4):
            keep.add((x, y))
    # the orchard rows (Caleb: more of them): a burnt fruit tree every other tile along rows two apart, a few gone
    # to stumps or fallen dead trees; charred brush over much of the ground between
    for y in range(1, ORCHARD_WALL_ROW - 1):
        for x in range(1, MAP_W - 1):
            if (x, y) in keep:
                continue
            r_ = rng.random()
            if x % 2 == 1 and y % 3 == 1:
                m[y][x] = BURNT_FRUIT_TREE if r_ < 0.78 else STUMP if r_ < 0.88 else DEAD_TREE if r_ < 0.94 else m[y][x]
            elif r_ < 0.30:
                m[y][x] = CHARRED_BRUSH
    # charred fences along parts of the lanes
    for (x, y) in sorted(lanes):
        for (dx, dy) in ((0, 1), (1, 0)):
            fx, fy = x + dx, y + dy
            if (fx, fy) not in keep and 0 < fx < MAP_W - 1 and 0 < fy < ORCHARD_WALL_ROW - 1 and rng.random() < 0.18:
                m[fy][fx] = BURNT_FENCE
    # burnt woods all round the edge (two or three deep), and loose burnt trees
    for y in range(MAP_H):
        for x in range(MAP_W):
            edge = min(x, y, MAP_W - 1 - x, MAP_H - 1 - y)
            if (x, y) in keep and not (y >= y0 - 1 and (x < x0 or x > x1)):
                continue
            if edge == 0 or (edge == 1 and rng.random() < 0.8) or (edge == 2 and rng.random() < 0.35):
                m[y][x] = BURNT_TREE
            elif m[y][x] in (DEAD, DEAD_CRACK, CHARRED_BRUSH) and (x, y) not in keep and rng.random() < 0.09:
                m[y][x] = BURNT_TREE
    # the pocket: open ground, closed in by burnt woods on both sides and the fallen trunk to the north
    for y in range(y0, MAP_H):
        for x in range(x0, x1 + 1):
            m[y][x] = DEAD if (x + y) % 4 else DEAD_CRACK
        for x in list(range(x0 - 3, x0)) + list(range(x1 + 1, x1 + 4)):
            m[y][x] = BURNT_TREE
    for y in range(ORCHARD_WALL_ROW, MAP_H):           # beside the pocket, below the trunk: solid woods (no way round)
        for x in range(MAP_W):
            if not (x0 <= x <= x1 and y >= y0):
                m[y][x] = BURNT_TREE
    for x in range(x0 - 3, x1 + 4):
        m[ORCHARD_WALL_ROW][x] = BURNT_LOG
    for x in range(x0 - 3, x1 + 4):
        if m[ORCHARD_WALL_ROW - 1][x] in (DEAD, DEAD_CRACK) and abs(x - 15.5) > 2:
            m[ORCHARD_WALL_ROW - 1][x] = BURNT_TREE    # thick woods just past it, but the lane goes on
    # the pieces
    for (x, y) in ORCHARD_LANTERNS:
        m[y][x] = LANTERN_COLD
    for (x, y) in ORCHARD_FIRES:
        m[y][x] = CAMPFIRE_COLD
        m[y][x - 1] = m[y][x + 1] = LOG_SEAT
    for (x, y) in ORCHARD_STACKS:
        m[y][x] = SMOKE_STACK
    nx, ny = ORCHARD_NEST
    for y in range(ny - 1, ny + 2):
        for x in range(nx - 1, nx + 2):
            m[y][x] = DEAD
    for (x, y) in ORCHARD_LEAFEN:
        m[y][x] = DEAD
    # everything that matters can be reached from the pocket (the trunk counts as open: YOU clears it)
    from collections import deque
    walk = {DEAD, DEAD_CRACK, ASH_LANE, CHARRED_BRUSH, BURNT_LOG}
    start = (15, MAP_H - 1)
    def reach():
        seen = {start}; q = deque([start])
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if 0 <= n[0] < MAP_W and 0 <= n[1] < MAP_H and n not in seen and m[n[1]][n[0]] in walk:
                    seen.add(n); q.append(n)
        return seen
    targets = [(x, y + 1) for (x, y) in ORCHARD_LANTERNS] + [(x, y + 1) for (x, y) in ORCHARD_FIRES]
    targets += [(x, y + 1) for (x, y) in ORCHARD_STACKS] + [ORCHARD_NEST] + ORCHARD_LEAFEN
    for (tx, ty) in targets:
        seen = reach()
        if (tx, ty) in seen:
            continue
        if m[ty][tx] not in walk:
            m[ty][tx] = DEAD
        prev = {(tx, ty): None}; q = deque([(tx, ty)])
        while q:
            c = q.popleft()
            if c in seen:
                while c is not None:
                    if m[c[1]][c[0]] not in walk:
                        m[c[1]][c[0]] = DEAD
                    c = prev[c]
                break
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if 0 < n[0] < MAP_W - 1 and 0 < n[1] < MAP_H - 1 and n not in prev and \
                        m[n[1]][n[0]] not in (LANTERN_COLD, CAMPFIRE_COLD, LOG_SEAT, SMOKE_STACK, BURNT_LOG) and \
                        not (n[1] >= ORCHARD_WALL_ROW):
                    prev[n] = c; q.append(n)
    return m


def make_grove():
    """The mushroom grove (v0.20.4, redone: Caleb found the first one weak). Giant mushrooms on a loose grid, their
    caps drawn above you in-game (you walk under them), red and glowing blue; soft moss trails winding between
    them to everything that matters; glowing pools with mossy shores; glowcaps, mushroom clusters and logs; a
    clearing with the fallen cap Cap is under; Tuft's nook walled by a tight clump, its way in a hollow log;
    three fairy rings; a cracked rock hiding a tiny chest."""
    import math
    from collections import deque
    rng = random.Random(9191)
    m = [[MOSS if rng.random() < 0.8 else DEAD for _ in range(MAP_W)] for _ in range(MAP_H)]
    keep = set()
    def clear(x, y, r=1):
        for yy in range(y - r, y + r + 1):
            for xx in range(x - r, x + r + 1):
                if 0 <= xx < MAP_W and 0 <= yy < MAP_H:
                    keep.add((xx, yy))
    tx, ty = GROVE_LEAFEN[0]
    lx, ly = GROVE_LOG
    nx, ny = GROVE_NOOK
    trail = set()
    def walk(x0, y0, x1, y1):                         # a soft trail, wandering a little
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        ph = rng.uniform(0, 6.3)
        last = None
        for k in range(n + 1):
            x = round(x0 + (x1 - x0) * k / n); y = round(y0 + (y1 - y0) * k / n)
            w = round(1.2 * math.sin(k * 0.4 + ph)) if 2 < k < n - 2 else 0
            if abs(x1 - x0) >= abs(y1 - y0): y += w
            else: x += w
            x = max(0, min(MAP_W - 1, x)); y = max(0, min(MAP_H - 1, y))
            if last and last[0] != x and last[1] != y:
                trail.add((x, last[1]))
            trail.add((x, y)); last = (x, y)
    cx, cy = GROVE_CAP
    walk(16, 0, cx, cy - 2); walk(0, 16, cx - 2, cy); walk(cx + 2, cy, lx, ly + 1)
    walk(cx, cy + 2, GROVE_LEAFEN[1][0] + 1, GROVE_LEAFEN[1][1]); walk(cx + 2, cy + 2, nx, ny - 2)
    for (rx_, ry_) in GROVE_RINGS:
        walk(cx, cy, rx_, ry_)
    walk(5, 0, 8, 8); walk(26, 0, 22, 6); walk(0, 5, 8, 8); walk(0, 26, 12, 26)
    for (x, y) in trail:
        keep.add((x, y))
    clear(cx, cy, 2)
    for p_ in GROVE_RINGS:
        clear(*p_)
    clear(*GROVE_LEAFEN[1], r=2)
    for yy in range(ty - 3, ty + 5):
        for xx in range(tx - 3, tx + 4):
            keep.add((xx, yy))
    for yy in range(ny - 3, ny + 2):
        for xx in range(nx - 2, nx + 3):
            keep.add((xx, yy))
    # glowing pools (water with mossy shores, lit in-game)
    for (px_, py_, rx, ry) in ((10, 12, 3.4, 2.2), (20, 26, 3.6, 2.0), (27, 13, 2.6, 2.4)):
        for y in range(MAP_H):
            for x in range(MAP_W):
                if ((x - px_) / rx) ** 2 + ((y - py_) / ry) ** 2 < 1 and (x, y) not in keep:
                    m[y][x] = WATER
    # giant mushrooms on a loose grid: a stem every 4 tiles across, 3 down (their 3x2 caps never collide)
    for j in range(11):
        for i in range(8):
            x = 1 + i * 4 + rng.randrange(3) - (j % 2) * 2   # a looser grid: rows shifted, each one jittered
            y = 2 + j * 3 + rng.randrange(2) * (i % 2)
            if not (0 < x < MAP_W - 1 and 2 <= y < MAP_H):
                continue
            cap_cells = [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-2, -1)]
            if (x, y) in keep or m[y][x] == WATER or rng.random() < 0.25:
                continue
            m[y][x] = GIANT_STEM_BLUE if rng.random() < 0.4 else GIANT_STEM
    for y in range(MAP_H):                            # the east and south edges: a wall of them
        for x in range(MAP_W):
            if (x == MAP_W - 1 and y % 2 == 0) or (y == MAP_H - 1 and x % 2 == 0):
                m[y][x] = GIANT_STEM_BLUE if (x + y) % 3 == 0 else GIANT_STEM
            elif x == MAP_W - 1 or y == MAP_H - 1:
                m[y][x] = ROCK
    # dressing: trails, glowcaps, mushroom clusters, a few logs
    for (x, y) in trail:
        if m[y][x] in (MOSS, DEAD):
            m[y][x] = MOSS_TRAIL
    for y in range(MAP_H):
        for x in range(MAP_W):
            if m[y][x] not in (MOSS, DEAD) or (x, y) in keep:
                continue
            r_ = rng.random()
            if r_ < 0.10:
                m[y][x] = GLOWCAPS
            elif r_ < 0.14:
                m[y][x] = MUSHROOMS
            elif r_ < 0.16:
                m[y][x] = LOG
            elif r_ < 0.19:
                m[y][x] = GIANT_MUSH if rng.random() < 0.6 else GIANT_MUSH_BLUE   # small ones among the giants
    # the clearing and the fallen cap, the rings
    for y in range(cy - 2, cy + 3):
        for x in range(cx - 2, cx + 3):
            m[y][x] = MOSS
    m[cy][cx] = FALLEN_CAP
    for (x, y) in GROVE_RINGS:
        for yy in range(y - 1, y + 2):
            for xx in range(x - 1, x + 2):
                if m[yy][xx] not in (MOSS_TRAIL,):
                    m[yy][xx] = MOSS
        m[y][x] = FAIRY_RING
    # Tuft's nook: a tight clump of giant mushrooms, its only way in a hollow log
    for y in range(ty - 2, ty + 4):
        for x in range(tx - 2, tx + 3):
            inside = ty - 1 <= y <= ty + 2 and tx - 1 <= x <= tx + 1
            if inside:
                m[y][x] = MOSS
            elif y == ty - 2 and x in (tx - 2, tx + 2):   # v0.20.4 (Caleb): only two giants, at the back corners
                m[y][x] = GIANT_STEM if x < tx else GIANT_STEM_BLUE
            else:                                         # the rest small mushrooms (no stacked caps)
                m[y][x] = GIANT_MUSH if (x + y) % 2 else GIANT_MUSH_BLUE
    m[ly][lx] = HOLLOW_LOG
    m[ly + 1][lx] = MOSS_TRAIL
    # the tiny chest's nook: rocks round it, a cracked one the way in
    for y in range(ny - 1, ny + 2):
        for x in range(nx - 1, nx + 2):
            m[y][x] = ROCK
    m[ny][nx] = TINY_CHEST
    m[ny - 1][nx] = CRACKED_ROCK
    m[ny - 2][nx] = MOSS_TRAIL
    m[GROVE_LEAFEN[1][1]][GROVE_LEAFEN[1][0]] = MOSS
    # everything that matters can be reached at full size from both ways in
    walkset = {MOSS, DEAD, GLOWCAPS, FAIRY_RING, MOSS_TRAIL, MUSHROOMS}
    def reach():
        seen = {(16, 0), (0, 16)}; q = deque(seen)
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n_ = (x + dx, y + dy)
                if 0 <= n_[0] < MAP_W and 0 <= n_[1] < MAP_H and n_ not in seen and m[n_[1]][n_[0]] in walkset:
                    seen.add(n_); q.append(n_)
        return seen
    targets = [(cx, cy + 1), (lx, ly + 1), (nx, ny - 2), GROVE_LEAFEN[1], (5, 0), (26, 0), (0, 5), (0, 26)]
    targets += list(GROVE_RINGS)
    for (gx, gy) in targets:
        seen = reach()
        if (gx, gy) in seen:
            continue
        prev = {(gx, gy): None}; q = deque([(gx, gy)])
        while q:
            c = q.popleft()
            if c in seen:
                while c is not None:
                    if m[c[1]][c[0]] not in walkset:
                        m[c[1]][c[0]] = MOSS_TRAIL
                    c = prev[c]
                break
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n_ = (c[0] + dx, c[1] + dy)
                if 0 <= n_[0] < MAP_W - 1 and 0 <= n_[1] < MAP_H - 1 and n_ not in prev and \
                        m[n_[1]][n_[0]] not in (HOLLOW_LOG, CRACKED_ROCK, TINY_CHEST, FALLEN_CAP) and \
                        not (tx - 2 <= n_[0] <= tx + 2 and ty - 2 <= n_[1] <= ty + 3) and \
                        not (nx - 1 <= n_[0] <= nx + 1 and ny - 1 <= n_[1] <= ny + 1):
                    prev[n_] = c; q.append(n_)
    return m


def make_hollow_in():
    """The hollow inside the Great Tree: floor (walkable, never grows) inside the chamber and its
    corridor, solid elsewhere. The room itself is drawn by art/hollow.png on the trunk's layer."""
    cx, cy = _art.HOLLOW_C
    m = [[TRUNK_BLOCK] * MAP_W for _ in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(MAP_W):
            px_, py_ = x * 16 + 8, y * 16 + 8
            inside = ((px_ - cx) / (_art.HOLLOW_RX - 10)) ** 2 + ((py_ - cy) / (_art.HOLLOW_RY - 10)) ** 2 < 1
            corridor = _art.HOLLOW_EXIT[0] <= px_ < _art.HOLLOW_EXIT[1] and py_ > cy
            if inside or corridor:
                m[y][x] = DRY_BRIDGE
    return m


def tile_set(base, files):
    """Some 512 x 512 layers (art/<file>) -> one set of unique 8x8 tiles (flips reused), numbered
    from `base` (after the map's tiles), and a 64 x 64 map per layer in screenblock order
    (top-left, top-right, bottom-left, bottom-right 32 x 32 blocks)."""
    index, tiles = {tuple([0] * 64): 0}, [tuple([0] * 64)]
    maps, pal = [], None
    for name in files:
        img = Image.open(os.path.join(ART, name))
        assert img.mode == "P" and img.size == (512, 512)
        pal = palette_of(img)
        px = list(img.tobytes())                  # one palette index per pixel
        entries = {}
        for ty in range(64):
            for tx in range(64):
                t = tuple(px[(ty * 8 + y) * 512 + tx * 8 + x] & 15 for y in range(8) for x in range(8))
                found = None
                for flip, v in ((0, t), (1, tuple(t[y * 8 + 7 - x] for y in range(8) for x in range(8))),
                                (2, tuple(t[(7 - y) * 8 + x] for y in range(8) for x in range(8))),
                                (3, tuple(t[(7 - y) * 8 + 7 - x] for y in range(8) for x in range(8)))):
                    if v in index:
                        found = (index[v], flip)
                        break
                if found is None:
                    index[t] = len(tiles)
                    tiles.append(t)
                    found = (index[t], 0)
                entries[(tx, ty)] = (base + found[0]) | (found[1] << 10) | (TREE_BANK << 12)
        order = []
        for sb in range(4):
            ox, oy = (sb & 1) * 32, (sb >> 1) * 32
            order += [entries[(ox + x, oy + y)] for y in range(32) for x in range(32)]
        maps.append(order)
    assert base + len(tiles) <= 1024, f"{files} need {len(tiles)} tiles, room for {1024 - base}"
    words = [sum((t[y * 8 + x] & 15) << (4 * x) for x in range(8)) for t in tiles for y in range(8)]
    return pal, words, maps


TREE_BANK = 4                      # BG palette bank of both tree layers
ROOM_BANK = 8                      # v0.20.5: the first of the Leafen homes' BG banks (8-15)


WALKABLE = {DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, DRY_GRASS, PEBBLES, PATH, BRIDGE, DRY_BRIDGE,
            MOSS, MUSHROOMS, LILY_PAD, BOG_PAD, ASH_LANE, CHARRED_BRUSH, GLOWCAPS, FAIRY_RING, MOSS_TRAIL,
            ARCH_OPEN, CRATER, BUD_OPEN, MOON_PATH}


def reach_count(m, start):
    from collections import deque
    seen = {start}; q = deque([start])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if 0 <= n[0] < MAP_W and 0 <= n[1] < MAP_H and n not in seen and m[n[1]][n[0]] in WALKABLE:
                seen.add(n); q.append(n)
    return len(seen)


# v0.20 (step 4) a tiny secret in each old region: a nook of rocks with a tiny chest inside, its only way in a
# cracked rock (tiny only), and a fairy ring a few steps away to shrink in. The chests hold decorations for the
# Leafen homes (kinds 0 flower pot, 1 painting, 2 bunting, 3 candle); the grove's nook is in its own map.
NOOK_TARGETS = [(VALLEY, 27, 14, 0), (EAST, 24, 20, 1), (SOUTH, 24, 12, 2), (MARSH, 21, 3, 3), (ORCHARD, 9, 6, 0)]
GROVE_NOOK_KIND = 3
KEEP_CLEAR = {(0, 10, 25), (0, 26, 22), (1, 6, 20), (1, 20, 28), (2, 16, 4), (2, 28, 22), (5, 17, 26), (5, 26, 26),
              (5, 7, 12), (6, 27, 10), (6, 23, 21),   # the Leafen's spots
              (0, 6, 18), (0, 13, 9), (0, 14, 24), (1, 27, 23), (2, 3, 26)}   # the start, the first spirits' spots
NOOKS = []                                          # filled by add_nooks: (area, chest x, y, kind, ring x, ring y)


def add_nooks(maps):
    near = lambda a, x, y, d: any(aa == a and abs(xx - x) <= d and abs(yy - y) <= d for (aa, xx, yy) in
                                  KEEP_CLEAR | {(a2, x2, y2) for (a2, x2, y2) in HOME_SITES + CHESTS + WRECKS} |
                                  {(a3, x3, y3) for (a3, x3, y3) in CAMPFIRES})
    for (a, tx, ty, kind) in NOOK_TARGETS:
        m = maps[a]
        start = next((cx, cy) for cy in range(MAP_H) for cx in range(MAP_W) if m[cy][cx] in WALKABLE)
        before = reach_count(m, start)
        cands = []
        for y in range(2, MAP_H - 7):
            for x in range(2, MAP_W - 2):
                land = [(x, y + 1), (x, y + 2), (x, y + 3), (x, y + 4)]      # the door, in front of it, the ring, below
                block = [(x + dx, y + dy) for dy in (-1, 0, 1) for dx in (-1, 0, 1)] + [(x - 1, y + 2), (x + 1, y + 2)]
                ok_land = lambda t: t in WALKABLE and t not in (BRIDGE, DRY_BRIDGE, LILY_PAD, BOG_PAD)
                if not all(ok_land(m[cy][cx]) for (cx, cy) in land) or \
                        not all(ok_land(m[cy][cx]) or m[cy][cx] == BOG_WATER for (cx, cy) in block):
                    continue
                if near(a, x, y, 3) or near(a, x, y + 3, 2):
                    continue
                if a == VALLEY and 8 <= x <= 24 and 6 <= y <= 28:   # the first walks: to Sprout, Ripple, the river
                    continue
                if a == VALLEY and x <= 7 and 9 <= y <= 17:         # the way west to the Great Tree, kept clear
                    continue
                cands.append((abs(x - tx) + abs(y - ty), x, y))
        for _, x, y in sorted(cands):                  # the nearest spot that cuts nothing off
            saved = [row[:] for row in m]
            for yy in range(y - 1, y + 2):
                for xx in range(x - 1, x + 2):
                    m[yy][xx] = ROCK
            m[y][x] = TINY_CHEST
            m[y + 1][x] = CRACKED_ROCK                # the way in, from below
            m[y + 3][x] = FAIRY_RING
            if reach_count(m, start) >= before - 12:
                NOOKS.append((a, x, y, kind, x, y + 3))
                break
            m[:] = saved
        else:
            raise AssertionError(f"no room for a tiny nook in area {a}")
    gx, gy = GROVE_NOOK
    NOOKS.append((GROVE, gx, gy, GROVE_NOOK_KIND, -1, -1))


def add_campfires(maps):
    """v0.19: a cold campfire and its two log seats in each old region, checked not to cut anything off."""
    for a, x, y in CAMPFIRES:
        m = maps[a]
        start = next((cx, cy) for cy in range(MAP_H) for cx in range(MAP_W) if m[cy][cx] in WALKABLE)
        before = reach_count(m, start)
        assert all(m[y][x + dx] in WALKABLE for dx in (-1, 0, 1)), f"campfire spot {a} {x},{y} is not open"
        m[y][x] = CAMPFIRE_COLD
        m[y][x - 1] = m[y][x + 1] = LOG_SEAT
        assert reach_count(m, start) >= before - 3, f"the campfire at {a} {x},{y} cuts the way"


def make_meadow():
    """v0.21 the Mirror Meadow (step 1) and its reflection, the same place twice. The real meadow: silvery grass,
    Vael's cut clearings full of stumps, a dry riverbed (one bridge), still pools, a ring of standing stones round a
    sealed rune stone, Silver's nook behind a broken arch, a crater where a star fell, shut night flowers. The
    reflection: the same at night before Vael: woods where the stumps are, the river flowing (an old bridge
    further up instead), the arch whole, the fallen star, the flowers open."""
    import math
    rng = random.Random(4242)
    def noise(seed, scale):
        r = random.Random(seed)
        g = [[r.random() for _ in range(MAP_W // scale + 2)] for _ in range(MAP_H // scale + 2)]
        def f(x, y):
            gx, gy = x / scale, y / scale
            x0, y0 = int(gx), int(gy)
            tx, ty = gx - x0, gy - y0
            tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
            a = g[y0][x0] * (1 - tx) + g[y0][x0 + 1] * tx
            c = g[y0 + 1][x0] * (1 - tx) + g[y0 + 1][x0 + 1] * tx
            return a * (1 - ty) + c * ty
        return f
    n1, n2 = noise(1, 6), noise(2, 3)
    real = [[0] * MAP_W for _ in range(MAP_H)]
    mirr = [[0] * MAP_W for _ in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(MAP_W):
            v = n1(x, y) * 0.7 + n2(x, y) * 0.3
            real[y][x] = FLOWERS if v > 0.74 and rng.random() < 0.4 else GRASS_TUFT if v < 0.4 else GRASS
            mirr[y][x] = FLOWERS if v > 0.68 and rng.random() < 0.5 else GRASS_TUFT if v < 0.35 else GRASS
    def blob(cx, cy, rx, ry, wob, seed):
        r = random.Random(seed); ph = r.uniform(0, 6.3)
        out = []
        for y in range(MAP_H):
            for x in range(MAP_W):
                k = 1 + wob * math.sin(3 * math.atan2(y - cy, x - cx) + ph)
                if ((x - cx) / (rx * k)) ** 2 + ((y - cy) / (ry * k)) ** 2 < 1:
                    out.append((x, y))
        return out
    river = set()
    for t in range(400):
        y = t / 400 * (MAP_H + 2) - 1
        x = 20 + 3.2 * math.sin(y * 0.22 + 0.6)
        for dx in (-1, 0, 1):
            xi, yi = int(round(x + dx)), int(round(y))
            if 0 <= xi < MAP_W and 0 <= yi < MAP_H:
                river.add((xi, yi))
    for (cx, cy, r) in ((5, 6, 4.4), (27, 22, 4.0), (6, 25, 3.4), (12, 10, 2.6), (2, 15, 2.2)):
        for (x, y) in blob(cx, cy, r, r * 0.85, 0.15, int(cx * 7 + cy)):
            if (x, y) in river:
                continue
            real[y][x] = DEAD if rng.random() < 0.7 else DIRT          # Vael's cut clearing
            if rng.random() < 0.78:
                mirr[y][x] = TREE
                real[y][x] = STUMP if rng.random() < 0.8 else DEAD_TREE
    for (x, y) in river:
        real[y][x] = DRY_RIVER; mirr[y][x] = WATER
    for (x, y) in river:
        if y == 27: real[y][x] = DRY_BRIDGE
        if y == 9: mirr[y][x] = BRIDGE
    pools = MEADOW_POOLS
    for i, (cx, cy, rx, ry) in enumerate(pools):
        for (x, y) in blob(cx, cy, rx, ry, 0.18, i + 5):
            if (x, y) not in river:
                real[y][x] = WATER; mirr[y][x] = WATER
    cx, cy = MEADOW_SEAL
    for (x, y) in blob(cx, cy, 2.8, 2.2, 0, 0):
        real[y][x] = DIRT; mirr[y][x] = FLOWERS
    MEADOW_RING.clear()
    for k in range(10):
        a = 2 * math.pi * k / 10
        x, y = round(cx + 3.2 * math.cos(a)), round(cy + 2.6 * math.sin(a))
        real[y][x] = MENHIR_GRASS; mirr[y][x] = MENHIR_LIT if k % 2 == 0 else MENHIR_GRASS   # the shape, in the reflection
        MEADOW_RING.append((x, y))
    real[cy][cx] = SEAL_STONE; mirr[cy][cx] = SEAL_GLOW
    # Silver's nook (north-east): a wall, its way in an arch, broken (real) or whole (reflection); a pool inside
    for x in range(24, MAP_W):
        real[8][x] = ROCK; mirr[8][x] = TREE
    for y in range(0, 8):
        real[y][24] = ROCK; mirr[y][24] = TREE
    ax = MEADOW_ARCH[0]
    real[8][ax - 1], real[8][ax], real[8][ax + 1] = ARCH_L, ARCH_RUBBLE, ARCH_R
    mirr[8][ax - 1], mirr[8][ax], mirr[8][ax + 1] = ARCH_L, ARCH_OPEN, ARCH_R
    for y in range(0, 8):
        for x in range(25, MAP_W):
            real[y][x] = GRASS if (x + y) % 3 else GRASS_TUFT; mirr[y][x] = GRASS if (x + y) % 3 else FLOWERS
    for (x, y) in blob(29, 4, 1.6, 1.3, 0, 9):
        real[y][x] = WATER; mirr[y][x] = WATER
    sx, sy = MEADOW_STAR
    real[sy][sx] = CRATER; mirr[sy][sx] = FALLEN_STAR
    mirr[sy - 1][sx] = GRASS                          # v0.21.4: a way into the star through the reflection's wood
    for (x, y) in MEADOW_BUDS:
        real[y][x] = BUD_CLOSED; mirr[y][x] = BUD_OPEN
    for x in range(13, 19):                           # the way in from the steppe (south)
        for y in range(28, MAP_H):
            real[y][x] = GRASS; mirr[y][x] = GRASS
    for y in range(13, 20):                           # the way west (to Vael's HQ, later): shut
        real[y][0] = ROCK; mirr[y][0] = TREE
    for y in range(MAP_H):                            # the reflection has no way out but the pools
        for x in (0, MAP_W - 1):
            if mirr[y][x] in (GRASS, GRASS_TUFT, FLOWERS): mirr[y][x] = TREE
    for x in range(MAP_W):
        for y in (0, MAP_H - 1):
            if mirr[y][x] in (GRASS, GRASS_TUFT, FLOWERS): mirr[y][x] = TREE
    # every pool reachable on the real side from the way in (cut a way through stumps if needed)
    walk = WALKABLE | {WATER}
    from collections import deque
    seen = {(15, MAP_H - 1)}; q = deque(seen)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n_ = (x + dx, y + dy)
            if 0 <= n_[0] < MAP_W and 0 <= n_[1] < MAP_H and n_ not in seen and real[n_[1]][n_[0]] in walk:
                seen.add(n_); q.append(n_)
    for (pcx, pcy, _, _) in pools[:5]:
        assert (round(pcx), round(pcy)) in seen, f"pool at {pcx},{pcy} cut off"
    MEADOW_MOON.clear()                               # v0.21.4: at night moonlight crosses the dry river by the ring
    for y in (cy - 1, cy, cy + 1):
        for x in range(MAP_W):
            if real[y][x] == DRY_RIVER:
                MEADOW_MOON.append((x, y))
    assert MEADOW_MOON, "no dry river by the ring"
    return real, mirr


MEADOW_SEAL = (13, 16)                 # v0.21 the sealed rune stone in the ring of standing stones
# the pools (centre x, y, radii), in the order Glimmer leads you: from the way in to beside the stone ring
MEADOW_POOLS = [(10, 27, 3.0, 2.0), (4, 19, 2.4, 1.8), (13, 4, 2.6, 1.8), (27, 13, 2.6, 2.2), (15, 22, 2.0, 1.4)]
MEADOW_ARCH = (27, 8)                  # the arch into Silver's nook (broken on the real side)
MEADOW_STAR = (2, 4)                   # the fallen star (the reflection), a crater (real)
MEADOW_BUDS = [(29, 29), (27, 30), (30, 27), (28, 28)]
MEADOW_RING = []                       # the ten standing stones (make_meadow fills it); the even ones make the star
MEADOW_MOON = []                       # dry river cells the moonlight crosses at night (make_meadow fills it)


def make_maps():
    meadow, mirror = make_meadow()
    maps = [make_valley(), make_east(), make_south(), make_west(), make_hollow_in(), make_marsh(), make_orchard(),
            make_grove(), [[ROOM_VOID] * MAP_W for _ in range(MAP_H)],   # 8: inside a Leafen home (built in-game)
            meadow, mirror]
    add_campfires(maps)
    add_nooks(maps)
    return [v for m in maps for row in m for v in row]


def leafen_big_sheet():
    """v0.20.5 the Leafen at home, 125%: each look standing, then mid-hop (32x32 frames, the Leafen's colors)."""
    im = Image.new("P", (32 * 10, 32))
    im.putpalette([c for rgb in _art.LEAFEN_PAL for c in rgb])
    for i, look in enumerate(rooms.LEAFEN_LOOKS):
        for f, pose in enumerate(("stand", "step")):
            px = _art.leafen_big(look, pose)
            for y in range(32):
                for x in range(32):
                    im.putpixel(((i * 2 + f) * 32 + x, y), px[y][x])
    return im


def room_data(tile_base):
    """v0.20.5 the Leafen homes' insides (tools/rooms.py): each home's tiles, screen, colors, map, owner's cell and
    decoration spots, loaded when you walk in."""
    ids = {n: _art.META.index(n) for n in _art.INTERIOR_META}
    ids["sp_first"] = ids["sp_lilypool"]
    tiles, off, cnt, scr, dec, pal, logic = [], [], [], [], [], [], []
    for k in range(rooms.NUM_HOMES):
        w, e, d, p, n = rooms.pack(k, tile_base, ROOM_BANK)
        off.append(len(tiles) // 8); cnt.append(n)
        tiles += w; scr += e; dec += d; pal += p
        logic += rooms.logic(k, ids)
    L = [rooms.LAYOUTS[rooms.HOME_LAYOUT[k]] for k in range(rooms.NUM_HOMES)]
    out = [f"#define ROOM_BANK {ROOM_BANK}\n#define ROOM_BANKS {rooms.ROOM_BANKS}"]
    out.append(c_array("unsigned int", "room_tiles", tiles))
    out.append(c_array("unsigned short", "room_tile_off", off, fmt="{}"))
    out.append(c_array("unsigned short", "room_tile_n", cnt, fmt="{}"))
    out.append(c_array("unsigned short", "room_scr", scr, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "room_dec", dec, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "room_pal", pal, fmt="0x{:04X}"))
    out.append(c_array("unsigned char", "room_logic", logic, per_line=15, fmt="{}"))
    for nm, key, i in (("room_owner_x", "O", 0), ("room_owner_y", "O", 1), ("room_spot_ax", "a", 0),
                       ("room_spot_ay", "a", 1), ("room_spot_bx", "b", 0), ("room_spot_by", "b", 1)):
        out.append(c_array("unsigned char", nm, [l[key][i] for l in L], fmt="{}"))
    return out


def c_array(ctype, name, values, per_line=8, fmt="0x{:08X}"):
    lines = [f"static const {ctype} {name}[{len(values)}] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(fmt.format(v) for v in values[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


# Sprout peeking out of its tree house (grove level 2 base): Sprout's first frame moved by (dx, dy)
# and cut to the dark pixels of the hole, so only the part inside the hole shows. main.c plays
# these frames (PEEK_FRAMES) in the order of its peek table.
PEEK_SHIFTS = [(0, 8), (0, 4), (0, 0), (-2, 0), (2, 0)]


def sprout_peek(terrain, sprout):
    base = (40 + 3) * 16                         # HOME_FIRST (40) + grove (0) * 4 + level 2 base (3); v0.19: it used
                                                 # the sheet's width, which pointed elsewhere once tiles came after
    hole = {(x, y) for y in range(16) for x in range(16)
            if 3 <= x <= 12 and 1 <= y <= 14 and terrain.getpixel((base + x, y)) == 15}
    img = Image.new("P", (16 * len(PEEK_SHIFTS), 16), 0)
    img.putpalette(sprout.getpalette())
    for i, (dx, dy) in enumerate(PEEK_SHIFTS):
        for x, y in hole:
            sx, sy = x - dx, y - dy
            if 0 <= sx < 16 and 0 <= sy < 16 and sprout.getpixel((sx, sy)):
                img.putpixel((16 * i + x, y), sprout.getpixel((sx, sy)))
    return img


def sleep_palette(pal):
    """Sleeping spirits: same art, desaturated and dimmed. The outline stays."""
    gray = []
    for c in pal:
        r, g, b = (c & 31), (c >> 5) & 31, (c >> 10) & 31
        v = (r * 3 + g * 6 + b) // 10 * 3 // 4 + 4
        gray.append(v | (v << 5) | (v << 10))
    gray[1] = pal[1]
    return gray


def main():
    terrain = Image.open(os.path.join(ART, "terrain.png"))
    player = Image.open(os.path.join(ART, "player_sprite.png"))
    sprout = Image.open(os.path.join(ART, "sprout_sprite.png"))
    fx = Image.open(os.path.join(ART, "fx_sprite.png"))       # drawn with player_sprite.png's palette (bank 0)
    ripple = Image.open(os.path.join(ART, "ripple_sprite.png"))
    pebble = Image.open(os.path.join(ART, "pebble_sprite.png"))
    breeze = Image.open(os.path.join(ART, "breeze_sprite.png"))
    bark = Image.open(os.path.join(ART, "bark_sprite.png"))
    clamp = Image.open(os.path.join(ART, "clamp_sprite.png"))
    combat = Image.open(os.path.join(ART, "combat_sprite.png"))
    boss = Image.open(os.path.join(ART, "boss_sprite.png"))
    leafen = Image.open(os.path.join(ART, "leafen_sprite.png"))
    lily = Image.open(os.path.join(ART, "lily_sprite.png"))
    swim = Image.open(os.path.join(ART, "swim_sprite.png"))     # drawn with the player's palette (bank 0)
    pouch = Image.open(os.path.join(ART, "pouch_sprite.png"))   # v0.18.4: the pouch's states, the Leafen's palette
    ball = Image.open(os.path.join(ART, "ball_sprite.png"))     # v0.18.4: each spirit as a ball (its own palette)
    flick = Image.open(os.path.join(ART, "flick_sprite.png"))   # v0.19: Flick (awake x2, cold, ember, shivering)
    orch = Image.open(os.path.join(ART, "orch_sprite.png"))     # v0.19: the orchard's region set (smoke creatures)
    cap = Image.open(os.path.join(ART, "cap_sprite.png"))       # v0.20: Cap (idle x2, its puff, the lingering cloud x2)
    glimmer = Image.open(os.path.join(ART, "glimmer_sprite.png"))   # v0.21: Glimmer (flying x2, its glint)
    nox = Image.open(os.path.join(ART, "nox_sprite.png"))       # v0.21.4: Nox (flying x2, its starlight, asleep; Glimmer's colors)
    grove = Image.open(os.path.join(ART, "grove_sprite.png"))   # v0.20: the grove's region set (poison puffs, their cloud)
    tiny = Image.open(os.path.join(ART, "tiny_sprite.png"))     # v0.20: the wizard shrunk (12 frames, 8x16)
    for im in (terrain, player, sprout, fx, ripple, pebble, breeze, bark, clamp, combat, boss, leafen, lily, swim, pouch, ball, flick, orch, cap, grove, tiny):
        assert im.mode == "P", "PNGs must be indexed color"
    # The player is 12 tall frames: down, up, side (faces right) x stand, walk1, walk2, walk3.
    assert player.size == (192, 32), f"player_sprite.png must be 192x32 (12 frames of 16x32), not {player.size}"
    # Sprite VRAM layout in 8x8 tiles; main.c uses these offsets.
    sheets = [("PLAYER", player, 32), ("SPROUT", sprout, 16), ("FX", fx, 16),
              ("RIPPLE", ripple, 16), ("PEBBLE", pebble, 16), ("BREEZE", breeze, 16),
              ("PEEK", sprout_peek(terrain, sprout), 16), ("BARK", bark, 16), ("COMBAT", combat, 16),
              ("LEAFEN", leafen, 16), ("LILY", lily, 16), ("SWIM", swim, 16), ("BALL", ball, 16), ("FLICK", flick, 16), ("CAP", cap, 16),
              ("GLIMMER", glimmer, 16), ("NOX", nox, 16)]
    sprite_words, tile_defs = [], []
    for name, img, cell_h in sheets:
        tile_defs.append(f"#define TILE_{name} {len(sprite_words) // 8}")
        sprite_words += strip_cells(img, cell_h)
    # The pouch: one slot (the back 64x64, then the front 64x32) that main.c fills with the state for
    # how many spirits are in it (pouch_tiles: five states, each laid out like the slot).
    def pouch_words(st):
        w = []
        for ty in range(12):                              # rows 0-7: the back; 8-11: the front
            for tx in range(8):
                w += tile_words(pouch, st * 64 + tx * 8, ty * 8)
        return w
    tile_defs.append(f"#define TILE_TINY {len(sprite_words) // 8}")   # 8x16: two tiles a frame, top then bottom
    for f in range(tiny.width // 8):
        sprite_words += tile_words(tiny, f * 8, 0) + tile_words(tiny, f * 8, 8)
    tile_defs.append(f"#define TILE_POUCH {len(sprite_words) // 8}")
    tile_defs.append(f"#define TILE_POUCH_FRONT {len(sprite_words) // 8 + 64}")
    sprite_words += pouch_words(0)
    pouch_all = [w for st in range(pouch.width // 64) for w in pouch_words(st)]
    # v0.18.3: everything above is always loaded. Each region's own sprites (its bosses, enemies, spirit
    # attacks) form a set that loads at TILE_REGION when you walk in, so every region gets all the room after
    # the shared sheets. A set's sheets are 32x32 frames (4 x 4 tiles each) or 16x16 cells.
    region_base = len(sprite_words) // 8
    tile_defs.append(f"#define TILE_REGION {region_base}")
    region_sets = [("TREE", [("CLAMP", clamp, 32), ("BOSS", boss, 32)]),   # the Great Tree and its hollow
                   ("ORCH", [("SMOKE", orch, 16)]),                           # v0.19 the burnt orchard
                   ("GROVE", [("PUFF", grove, 16)]),                          # v0.20 the mushroom grove
                   ("ROOM", [("LFBIG", leafen_big_sheet(), 32)])]             # v0.20.5 the Leafen at home
    region_words = {}
    for set_name, set_sheets in region_sets:
        words = []
        for name, img, cell in set_sheets:
            tile_defs.append(f"#define TILE_{name} {region_base + len(words) // 8}")
            if cell == 16:
                words += strip_cells(img, 16)
                continue
            for i in range(img.width // 32):
                for ty in range(4):
                    for tx in range(4):
                        words += tile_words(img, i * 32 + tx * 8, ty * 8)
        assert region_base + len(words) // 8 <= 1024, f"region set {set_name} needs {len(words) // 8} tiles, room for {1024 - region_base}"
        region_words[set_name] = words
    pal_ripple = palette_of(ripple)
    pal_pebble = palette_of(pebble)
    pal_breeze = palette_of(breeze)
    gray = sleep_palette(pal_ripple)

    out = ["// Generated by tools/build_data.py. Do not edit by hand.", "#pragma once", ""]
    out.append(f"#define MAP_W {MAP_W}\n#define MAP_H {MAP_H}")
    out.append(f"#define PLAYER_START_X {PLAYER_START[0]}\n#define PLAYER_START_Y {PLAYER_START[1]}")
    out.append(f"#define SPIRIT_SPOT_X {SPIRIT_SPOT[0]}\n#define SPIRIT_SPOT_Y {SPIRIT_SPOT[1]}")
    out.append(f"#define RIPPLE_SPOT_X {RIPPLE_SPOT[0]}\n#define RIPPLE_SPOT_Y {RIPPLE_SPOT[1]}")
    out.append(f"#define STONE_SPOT_X {STONE_SPOT[0]}\n#define STONE_SPOT_Y {STONE_SPOT[1]}")
    out.append(f"#define STONE_AREA {STONE_AREA}\n#define BREEZE_AREA {BREEZE_AREA}")
    out.append(f"#define LEVER_X {DAM_LEVER[0]}\n#define LEVER_Y {DAM_LEVER[1]}")
    out.append("#define POCKET_X0 {}\n#define POCKET_Y0 {}\n#define POCKET_X1 {}\n#define POCKET_Y1 {}".format(*POCKET))
    out.append(f"#define BREEZE_SPOT_X {BREEZE_SPOT[0]}\n#define BREEZE_SPOT_Y {BREEZE_SPOT[1]}")
    out.append(f"#define NUM_AREAS {NUM_AREAS}")
    out.append(f"#define ORCHARD_NEST_X {ORCHARD_NEST[0]}\n#define ORCHARD_NEST_Y {ORCHARD_NEST[1]}")
    out.append(f"#define NUM_CAMPS {len(CAMPFIRES)}")
    out.append(f"#define GROVE_CAP_X {GROVE_CAP[0]}\n#define GROVE_CAP_Y {GROVE_CAP[1]}")
    out.append(f"#define NUM_STACKS {len(ORCHARD_STACKS)}")
    out.append(c_array("unsigned char", "stack_x", [x for x, _ in ORCHARD_STACKS], fmt="{}"))
    out.append(c_array("unsigned char", "stack_y", [y for _, y in ORCHARD_STACKS], fmt="{}"))
    out.append(c_array("unsigned char", "camp_area", [a for a, _, _ in CAMPFIRES], fmt="{}"))
    out.append(c_array("unsigned char", "camp_x", [x for _, x, _ in CAMPFIRES], fmt="{}"))
    out.append(c_array("unsigned char", "camp_y", [y for _, _, y in CAMPFIRES], fmt="{}"))
    out.append(c_array("unsigned char", "home_site_area", [a for a, _, _ in HOME_SITES], fmt="{}"))
    out.append(c_array("unsigned char", "home_site_x", [x for _, x, _ in HOME_SITES], fmt="{}"))
    out.append(c_array("unsigned char", "home_site_y", [y for _, _, y in HOME_SITES], fmt="{}"))
    out.append(f"#define NUM_CHESTS {len(CHESTS)}")
    out.append(c_array("unsigned char", "chest_area", [a for a, _, _ in CHESTS], fmt="{}"))
    out.append(c_array("unsigned char", "chest_x", [x for _, x, _ in CHESTS], fmt="{}"))
    out.append(c_array("unsigned char", "chest_y", [y for _, _, y in CHESTS], fmt="{}"))
    out.append(f"#define NUM_WRECKS {len(WRECKS)}")
    out.append(c_array("unsigned char", "wreck_area", [a for a, _, _ in WRECKS], fmt="{}"))
    out.append(c_array("unsigned char", "wreck_x", [x for _, x, _ in WRECKS], fmt="{}"))
    out.append(c_array("unsigned char", "wreck_y", [y for _, _, y in WRECKS], fmt="{}"))
    out.append(f'static const char FONT_CHARS[] = "{FONT_CHARS}";')
    out.append(c_array("unsigned short", "bg_pal", palette_of(terrain), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal", palette_of(player), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_sprout", palette_of(sprout), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_fx", palette_of(fx), fmt="0x{:04X}"))
    out.append("\n".join(tile_defs))
    out.append(c_array("unsigned int", "terrain_tiles", strip_16x16(terrain)))
    out.append(c_array("unsigned short", "obj_pal_ripple", pal_ripple, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_ripple_sleep", gray, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_pebble", pal_pebble, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_pebble_sleep", sleep_palette(pal_pebble), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_breeze", pal_breeze, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_breeze_sleep", sleep_palette(pal_breeze), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_bark", palette_of(bark), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_bark_sleep", sleep_palette(palette_of(bark)), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_clamp", palette_of(clamp), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_combat", palette_of(combat), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_boss", palette_of(boss), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_leafen", palette_of(leafen), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_lily", palette_of(lily), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_flick", palette_of(flick), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_orch", palette_of(orch), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_cap", palette_of(cap), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_glimmer", palette_of(glimmer), fmt="0x{:04X}"))
    out.append(f"#define NUM_MEADOW_POOLS {len(MEADOW_POOLS)}")   # v0.21 Glimmer's trail, pool to pool
    out.append(f"#define MEADOW_STAR_X {MEADOW_STAR[0]}\n#define MEADOW_STAR_Y {MEADOW_STAR[1]}")
    out.append(f"#define MEADOW_SEAL_X {MEADOW_SEAL[0]}\n#define MEADOW_SEAL_Y {MEADOW_SEAL[1]}")
    out.append(c_array("unsigned char", "meadow_pool_x", [round(p[0]) for p in MEADOW_POOLS], fmt="{}"))
    out.append(c_array("unsigned char", "meadow_pool_y", [round(p[1]) for p in MEADOW_POOLS], fmt="{}"))
    out.append(c_array("unsigned short", "obj_pal_grove", palette_of(grove), fmt="0x{:04X}"))
    out.append(c_array("unsigned int", "sprite_tiles", sprite_words))
    out.append(f"#define POUCH_STATES {pouch.width // 64}\n#define POUCH_FRONT_Y {_art.POUCH_FRONT_Y}")
    out.append(c_array("unsigned int", "pouch_tiles", pouch_all))
    out.append(c_array("unsigned char", "pouch_mouth_y", [_art.pouch_mouth_y(st) for st in range(pouch.width // 64)], fmt="{}"))
    for set_name, words in region_words.items():
        out.append(c_array("unsigned int", f"region_{set_name.lower()}_tiles", words))
    out.append(c_array("unsigned int", "font_tiles", font_words()))
    out.append(f"#define BANNER_W {2 * len(BANNER_TEXT)}   // tiles wide; 3 tiles tall")
    out.append(c_array("unsigned int", "banner_tiles", banner_words()))
    out.append(c_array("unsigned char", "start_maps", make_maps(), per_line=32, fmt="{}"))
    out.append(c_array("unsigned char", "meadow_ring_x", [p[0] for p in MEADOW_RING], fmt="{}"))   # v0.21.4 (after start_maps: make_meadow fills them)
    out.append(c_array("unsigned char", "meadow_ring_y", [p[1] for p in MEADOW_RING], fmt="{}"))
    out.append(f"#define NUM_MEADOW_MOON {len(MEADOW_MOON)}")
    out.append(c_array("unsigned char", "meadow_moon_x", [p[0] for p in MEADOW_MOON], fmt="{}"))
    out.append(c_array("unsigned char", "meadow_moon_y", [p[1] for p in MEADOW_MOON], fmt="{}"))
    out.append(f"#define NUM_NOOKS {len(NOOKS)}")
    for i, nm in enumerate(("nook_area", "nook_x", "nook_y", "nook_kind", "nook_ring_x", "nook_ring_y")):
        out.append(c_array("signed char", nm, [n[i] for n in NOOKS], fmt="{}"))
    # tree tiles follow the map's tiles in charblock 0; the 9 home animation frames are not loaded there
    base = (MARSH_META_FIRST - 9) * 4                 # the marsh's own metatiles load into the tree slots
    out += room_data(base)
    out.append(f"#define MARSH_META_FIRST {MARSH_META_FIRST}")
    out.append(f"#define TREE_BANK {TREE_BANK}\n#define TREE_TILE_BASE {base}")
    # Tile sets loaded into the same slots by area: the dead tree, the hollow, the living tree.
    for name, files in (("dead", ("tree_trunk.png", "canopy.png")), ("hollow", ("hollow.png",)),
                        ("alive", ("tree_alive.png", "leaves.png"))):
        pal, words, layer_maps = tile_set(base, files)
        out.append(c_array("unsigned short", f"pal_{name}", pal, fmt="0x{:04X}"))
        out.append(c_array("unsigned int", f"tiles_{name}", words))
        for i, lm in enumerate(layer_maps):
            out.append(c_array("unsigned short", f"map_{name}_{'trunk' if i == 0 else 'top'}", lm, fmt="0x{:04X}"))
    # v0.19.8 (Caleb): you walk under the Great Tree's roots, not over them: which 8x8 cells of its layer are
    # mostly drawn (a bit each, 64 x 64), for the dead tree and the healed one. The game hides you there.
    for name, file in (("dead", "tree_trunk.png"), ("alive", "tree_alive.png")):
        im = Image.open(os.path.join(ART, file))
        bits = [0] * (64 * 64 // 8)
        for cy in range(64):
            for cx in range(64):
                n = sum(1 for y in range(8) for x in range(8) if im.getpixel((cx * 8 + x, cy * 8 + y)))
                if n >= 32:
                    i = cy * 64 + cx
                    bits[i >> 3] |= 1 << (i & 7)
        out.append(c_array("unsigned char", f"root_mask_{name}", bits, fmt="{}"))
    out.append(f"#define NUM_CLAMPS {len(CLAMP_PX)}")
    out.append(c_array("unsigned short", "clamp_px_x", [x for x, _ in CLAMP_PX], fmt="{}"))
    out.append(c_array("unsigned short", "clamp_px_y", [y for _, y in CLAMP_PX], fmt="{}"))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n\n".join(out) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
