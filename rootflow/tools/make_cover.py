"""Compose the cover image (art/cover.png) from the game's own art.

Uses terrain.png, player_sprite.png, sprout_sprite.png, fx_sprite.png, ripple_sprite.png, and the 5x7 font, so the
cover follows any hand edits to the art. Output is 640x640 (160x160 x4).
"""
import math
import os
import random
from PIL import Image

from build_data import FONT

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
OUT = os.path.join(ART, "cover.png")

SIZE = 160          # logical pixels
SCALE = 4
GRID = SIZE // 16   # metatiles per side

DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE = range(12)
SPR_RIPPLE = 0      # cell in ripple_sprite.png

CREAM = (240, 226, 182)
OUTLINE = (28, 36, 32)
GREEN = (156, 204, 101)


def rgba(path):
    """Indexed PNG -> RGBA with palette index 0 transparent."""
    im = Image.open(path)
    out = im.convert("RGBA")
    px, src = out.load(), im.load()
    for y in range(im.height):
        for x in range(im.width):
            if src[x, y] == 0:
                px[x, y] = (0, 0, 0, 0)
    return out


def cell(sheet, i):
    return sheet.crop((i * 16, 0, i * 16 + 16, 16))


def make_map():
    rng = random.Random(7)
    cx, cy, river = 4, 6, 8
    m = [[DEAD] * GRID for _ in range(GRID)]
    for y in range(GRID):
        for x in range(GRID):
            d = math.hypot(x - cx, y - cy)
            if x == river:
                m[y][x] = BRIDGE if y == 6 else (WATER if y % 2 else WATER_B)
            elif d <= 2.3:
                m[y][x] = rng.choice([GRASS, GRASS, GRASS_TUFT, FLOWERS])
            elif d <= 3.3:
                m[y][x] = DIRT
            else:
                m[y][x] = rng.choice([DEAD, DEAD, DEAD_CRACK])
    for x, y in [(2, 5), (6, 5), (5, 8)]:
        m[y][x] = TREE
    for x, y in [(0, 4), (1, 9), (6, 2), (9, 3), (1, 1), (9, 8)]:
        m[y][x] = DEAD_TREE
    m[4][9] = ROCK
    m[7][4] = FLOWERS
    return m


def draw_text(img, text, y, scale, color):
    """Centered 5x7 text with a 1-pixel drop shadow (in text pixels)."""
    w = len(text) * 6 * scale - scale
    x0 = (SIZE - w) // 2
    for dx, dy, col in [(scale, scale, OUTLINE), (0, 0, color)]:
        for i, ch in enumerate(text):
            rows = FONT[ch].split()
            for ry, row in enumerate(rows):
                for rx, bit in enumerate(row):
                    if bit == "1":
                        bx = x0 + i * 6 * scale + rx * scale + dx
                        by = y + ry * scale + dy
                        for py in range(scale):
                            for qx in range(scale):
                                img.putpixel((bx + qx, by + py), col + (255,))


def main():
    terrain = rgba(os.path.join(ART, "terrain.png"))
    player = rgba(os.path.join(ART, "player_sprite.png"))
    sprout = rgba(os.path.join(ART, "sprout_sprite.png"))
    fx = rgba(os.path.join(ART, "fx_sprite.png"))
    ripple = rgba(os.path.join(ART, "ripple_sprite.png"))

    img = Image.new("RGBA", (SIZE, SIZE), OUTLINE + (255,))
    for y, row in enumerate(make_map()):
        for x, t in enumerate(row):
            img.alpha_composite(cell(terrain, t), (x * 16, y * 16))

    # Dark band behind the title.
    band = Image.new("RGBA", (SIZE, 34), OUTLINE + (200,))
    img.alpha_composite(band, (0, 8))
    # Dark strip behind the tagline.
    strip = Image.new("RGBA", (SIZE, 13), OUTLINE + (200,))
    img.alpha_composite(strip, (0, SIZE - 15))

    for spr, pos in [(cell(sprout, 0), (48, 94)),
                     (cell(player, 0), (64, 94)),
                     (cell(ripple, SPR_RIPPLE), (110, 112)),
                     (cell(fx, 0), (56, 80)),
                     (cell(fx, 0), (78, 104))]:
        img.alpha_composite(spr, pos)

    draw_text(img, "ROOTFLOW", 12, 3, CREAM)
    draw_text(img, "SMALL ROOTS. BIG CHANGE.", SIZE - 12, 1, GREEN)

    img = img.convert("RGB").resize((SIZE * SCALE, SIZE * SCALE), Image.NEAREST)
    img.save(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
