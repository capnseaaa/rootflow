"""Convert art/*.png, the font, and the starting maps into src/data.h."""
import os
import random
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
OUT = os.path.join(ROOT, "src", "data.h")

# Metatile indices (must match make_art.py META and main.c enum)
(DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE, THORN, HOMESITE,
 CHEST, CHEST_OPEN, WRECK, DRY_GRASS, PEBBLES, MENHIR) = range(20)

MAP_W = MAP_H = 32
PLAYER_START = (6, 18)
SPIRIT_SPOT = (13, 9)
RIPPLE_SPOT = (14, 24)
STONE_SPOT = (27, 16)
# Hidden spring: interior rectangle (x0, y0, x1, y1), walled by a ring of rocks.
POCKET = (26, 2, 30, 6)
BREEZE_SPOT = (27, 2)          # sleeps inside the hidden spring
# Thorn grove in the bottom-left corner: interior x 0..6, y 26..31, walled by thorns.
THORN_GROVE = (0, 26, 6, 31)
# Areas: 0 = the valley, 1 = Windy Steppe (east of the valley), 2 = Stone Hollow (south).
VALLEY, EAST, SOUTH = 0, 1, 2
NUM_AREAS = 3
EAST_GATE_ROWS = (19, 20, 21)      # valley x=31 <-> steppe x=0
SOUTH_GATE_COLS = (14, 15, 16)     # valley y=31 <-> hollow y=0
# Home sites, one per spirit kind (Sprout, Ripple, Pebble, Breeze): (area, x, y).
HOME_SITES = [(VALLEY, 3, 28), (VALLEY, 28, 5), (SOUTH, 16, 15), (EAST, 20, 15)]
# Treasure chests (area, x, y); the reward for each is in main.c (chest_reward).
CHESTS = [(VALLEY, 5, 30), (EAST, 28, 4), (SOUTH, 26, 27)]
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
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%:!.?'+/-,"


def font_words():
    """Tile 0 is transparent. Glyph tiles: color 2 background, color 1 text."""
    words = [0] * 8
    for ch in FONT_CHARS:
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
    for cx, cy in (PLAYER_START, SPIRIT_SPOT, RIPPLE_SPOT, STONE_SPOT):
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
    place_objects(m, VALLEY)
    return m


def open_around(m, x, y):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if 0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H and m[y + dy][x + dx] in (ROCK, DEAD_TREE, THORN):
            m[y + dy][x + dx] = DEAD


def place_objects(m, area):
    """Home sites, chests, and wrecks for one area, each with open ground beside it."""
    for a, x, y in HOME_SITES:
        if a == area:
            open_around(m, x, y)
            m[y][x] = HOMESITE
    for a, x, y in CHESTS:
        if a == area:
            open_around(m, x, y)
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
    for y in range(19, 23):                      # pond
        for x in range(7, 11):
            m[y][x] = WATER
    for x in SOUTH_GATE_COLS:                    # entry path from the valley
        for y in range(0, 5):
            m[y][x] = DEAD
    retile(m, DEAD_CRACK, PEBBLES)               # hollow ground: loose pebbles
    # landmark: a ring of standing stones around open ground
    cx, cy = 6, 13
    for dx, dy in ((0, -2), (2, -1), (2, 1), (0, 2), (-2, 1), (-2, -1)):
        m[cy + dy][cx + dx] = MENHIR
    for y in range(cy - 1, cy + 2):
        for x in range(cx - 1, cx + 2):
            m[y][x] = DEAD
    place_objects(m, SOUTH)
    return m


def retile(m, old, new):
    for row in m:
        for x, t in enumerate(row):
            if t == old:
                row[x] = new


def make_maps():
    maps = [make_valley(), make_east(), make_south()]
    return [v for m in maps for row in m for v in row]


def c_array(ctype, name, values, per_line=8, fmt="0x{:08X}"):
    lines = [f"static const {ctype} {name}[{len(values)}] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(fmt.format(v) for v in values[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


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
    for im in (terrain, player, sprout, fx, ripple, pebble, breeze):
        assert im.mode == "P", "PNGs must be indexed color"
    # The player is 12 tall frames: down, up, side (faces right) x stand, walk1, walk2, walk3.
    assert player.size == (192, 32), f"player_sprite.png must be 192x32 (12 frames of 16x32), not {player.size}"
    # Sprite VRAM layout in 8x8 tiles; main.c uses these offsets.
    sheets = [("PLAYER", player, 32), ("SPROUT", sprout, 16), ("FX", fx, 16),
              ("RIPPLE", ripple, 16), ("PEBBLE", pebble, 16), ("BREEZE", breeze, 16)]
    sprite_words, tile_defs = [], []
    for name, img, cell_h in sheets:
        tile_defs.append(f"#define TILE_{name} {len(sprite_words) // 8}")
        sprite_words += strip_cells(img, cell_h)
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
    out.append("#define POCKET_X0 {}\n#define POCKET_Y0 {}\n#define POCKET_X1 {}\n#define POCKET_Y1 {}".format(*POCKET))
    out.append(f"#define BREEZE_SPOT_X {BREEZE_SPOT[0]}\n#define BREEZE_SPOT_Y {BREEZE_SPOT[1]}")
    out.append(f"#define NUM_AREAS {NUM_AREAS}")
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
    out.append(c_array("unsigned int", "sprite_tiles", sprite_words))
    out.append(c_array("unsigned int", "font_tiles", font_words()))
    out.append(c_array("unsigned char", "start_maps", make_maps(), per_line=32, fmt="{}"))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n\n".join(out) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
