"""Convert art/*.png, the font, and the starting map into src/data.h."""
import os
import random
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
OUT = os.path.join(ROOT, "src", "data.h")

# Metatile indices (must match make_art.py META and main.c enum)
DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE, THORN, HOMESITE = range(14)

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
# Home sites, one per spirit kind (Sprout, Ripple, Pebble, Breeze).
HOME_SITES = [(3, 28), (28, 5), (29, 10), (4, 11)]


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
    words = []
    for i in range(img.width // 16):
        for ty in (0, 8):
            for tx in (0, 8):
                words += tile_words(img, i * 16 + tx, ty)
    return words


# ---- 5x7 font ----
FONT = {
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
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%:!.?'"


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
def make_map():
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
    # home sites: the marker plus open, non-solid ground on its four sides
    for x, y in HOME_SITES:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if m[y + dy][x + dx] in (ROCK, DEAD_TREE, THORN):
                m[y + dy][x + dx] = DEAD
        m[y][x] = HOMESITE
    return [v for row in m for v in row]


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
    player = Image.open(os.path.join(ART, "player.png"))
    sprout = Image.open(os.path.join(ART, "sprout.png"))
    fx = Image.open(os.path.join(ART, "fx.png"))       # drawn with player.png's palette (bank 0)
    sprites2 = Image.open(os.path.join(ART, "sprites2.png"))
    sprites3 = Image.open(os.path.join(ART, "sprites3.png"))
    sprites4 = Image.open(os.path.join(ART, "sprites4.png"))
    for im in (terrain, player, sprout, fx, sprites2, sprites3, sprites4):
        assert im.mode == "P", "PNGs must be indexed color"
    pal2 = palette_of(sprites2)
    pal3 = palette_of(sprites3)
    pal4 = palette_of(sprites4)
    gray = sleep_palette(pal2)

    out = ["// Generated by tools/build_data.py. Do not edit by hand.", "#pragma once", ""]
    out.append(f"#define MAP_W {MAP_W}\n#define MAP_H {MAP_H}")
    out.append(f"#define PLAYER_START_X {PLAYER_START[0]}\n#define PLAYER_START_Y {PLAYER_START[1]}")
    out.append(f"#define SPIRIT_SPOT_X {SPIRIT_SPOT[0]}\n#define SPIRIT_SPOT_Y {SPIRIT_SPOT[1]}")
    out.append(f"#define RIPPLE_SPOT_X {RIPPLE_SPOT[0]}\n#define RIPPLE_SPOT_Y {RIPPLE_SPOT[1]}")
    out.append(f"#define STONE_SPOT_X {STONE_SPOT[0]}\n#define STONE_SPOT_Y {STONE_SPOT[1]}")
    out.append("#define POCKET_X0 {}\n#define POCKET_Y0 {}\n#define POCKET_X1 {}\n#define POCKET_Y1 {}".format(*POCKET))
    out.append(f"#define BREEZE_SPOT_X {BREEZE_SPOT[0]}\n#define BREEZE_SPOT_Y {BREEZE_SPOT[1]}")
    out.append(c_array("unsigned char", "home_site_x", [x for x, _ in HOME_SITES], fmt="{}"))
    out.append(c_array("unsigned char", "home_site_y", [y for _, y in HOME_SITES], fmt="{}"))
    out.append(f'static const char FONT_CHARS[] = "{FONT_CHARS}";')
    out.append(c_array("unsigned short", "bg_pal", palette_of(terrain), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal", palette_of(player), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_sprout", palette_of(sprout), fmt="0x{:04X}"))
    out.append(c_array("unsigned int", "terrain_tiles", strip_16x16(terrain)))
    out.append(c_array("unsigned short", "obj_pal2", pal2, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal_sleep", gray, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal3", pal3, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal3_sleep", sleep_palette(pal3), fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal4", pal4, fmt="0x{:04X}"))
    out.append(c_array("unsigned short", "obj_pal4_sleep", sleep_palette(pal4), fmt="0x{:04X}"))
    out.append(c_array("unsigned int", "sprite_tiles",
                       strip_16x16(player) + strip_16x16(sprout) + strip_16x16(fx)
                       + strip_16x16(sprites2) + strip_16x16(sprites3)
                       + strip_16x16(sprites4)))
    out.append(c_array("unsigned int", "font_tiles", font_words()))
    out.append(c_array("unsigned char", "start_map", make_map(), per_line=32, fmt="{}"))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("\n\n".join(out) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
