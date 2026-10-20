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
        "thorn", "homesite"]

# ---- water spirit palette (OBJ bank 1). The game builds a gray copy for sleep. ----
OBJ2_PAL = [
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


def make_terrain():
    rng = random.Random(7)
    tiles = [
        dead(rng), dead_crack(rng), dirt(rng), grass(rng, tufts=0),
        grass(rng, tufts=5), grass(rng, tufts=1, flowers=3), tree(rng),
        dead_tree(rng), rock(rng), water(rng, 0), water(rng, 1), bridge(rng),
        thorn(rng), homesite(rng),
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
}
# The main sheet is split into three files: the player, Sprout, and effects.
# player.png and fx.png share OBJ bank 0; Sprout has its own bank (7).
PLAYER_ORDER = ["down0", "down1", "up0", "up1", "side0", "side1"]
SPROUT_ORDER = ["spirit0", "spirit1"]
FX_ORDER = ["sparkle"]

# Water spirit "Ripple" and the sleep bubble, drawn with OBJ2_PAL.
SPRITES2 = {
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
SPRITE2_ORDER = ["ripple0", "ripple1", "zzz"]

# ---- stone spirit palette (OBJ bank 3). The game builds a gray copy for sleep. ----
OBJ3_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline / eyes
    (196, 184, 164),  # 2 stone light
    (150, 138, 122),  # 3 stone mid
    (104, 94, 86),    # 4 stone dark
    (184, 240, 138),  # 5 moss light
    (106, 196, 106),  # 6 moss mid
] + [(0, 0, 0)] * 9

# Stone spirit "Pebble": a round boulder with a moss cap, drawn with OBJ3_PAL.
SPRITES3 = {
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
SPRITES3["stone1"] = ["." * 16] + SPRITES3["stone0"][:-1]   # bob frame: one pixel lower
SPRITE3_ORDER = ["stone0", "stone1"]

# ---- wind spirit palette (OBJ bank 5). The game builds a gray copy for sleep. ----
OBJ4_PAL = [
    (255, 0, 255),    # 0 transparent
    (28, 36, 32),     # 1 outline / eyes
    (244, 250, 255),  # 2 white
    (190, 228, 240),  # 3 pale cyan
    (120, 188, 210),  # 4 teal
] + [(0, 0, 0)] * 11

# Wind spirit "Breeze": a small cloud puff with a curling tail, drawn with OBJ4_PAL.
SPRITES4 = {
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
SPRITES4["breeze1"] = ["." * 16] + SPRITES4["breeze0"][:-1]
SPRITE4_ORDER = ["breeze0", "breeze1"]


def make_sprites4():
    return draw_sheet(SPRITES4, SPRITE4_ORDER, OBJ4_PAL)


def make_sprites3():
    return draw_sheet(SPRITES3, SPRITE3_ORDER, OBJ3_PAL)


def make_sprites2():
    return draw_sheet(SPRITES2, SPRITE2_ORDER, OBJ2_PAL)


def make_player():
    return draw_sheet(SPRITES, PLAYER_ORDER, OBJ_PAL)


def make_sprout():
    return draw_sheet(SPRITES, SPROUT_ORDER, OBJ_PAL)


def make_fx():
    return draw_sheet(SPRITES, FX_ORDER, OBJ_PAL)


def split_old_sheet():
    """Older builds kept the player, Sprout, and the sparkle in one sprites.png.
    Cut it into player.png, sprout.png, and fx.png, keeping every pixel and the palette."""
    old_path = os.path.join(ART, "sprites.png")
    if not os.path.exists(old_path):
        return
    old = Image.open(old_path)
    start = 0
    for name, order in (("player.png", PLAYER_ORDER), ("sprout.png", SPROUT_ORDER), ("fx.png", FX_ORDER)):
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
        split_old_sheet()
    for name, fn in (("terrain.png", make_terrain), ("player.png", make_player),
                     ("sprout.png", make_sprout), ("fx.png", make_fx),
                     ("sprites2.png", make_sprites2), ("sprites3.png", make_sprites3),
                     ("sprites4.png", make_sprites4)):
        path = os.path.join(ART, name)
        if force or not os.path.exists(path):
            fn().save(path)
            print("wrote", path)
        elif name == "terrain.png":
            append_new_metatiles(path)


if __name__ == "__main__":
    main()
