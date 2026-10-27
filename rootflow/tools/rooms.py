"""v0.20.5 the Leafen homes' insides, redone (Caleb liked the preview): each home is drawn as one 240x160 picture
(3/4 view: a back wall two tiles tall with a top edge and a window, side and front walls seen from above, a plank
floor, shaded furniture), cut into 8x8 tiles that load into the room-only tile slots when you walk in. build_data
writes, for each of the 13 homes: its tiles, its screen (30x20 entries), its colors (up to ROOM_BANKS BG banks),
the room's logic map (15x10 tile ids: walls, floor, furniture are solid), the owner's cell, its two decoration
spots and their pictures (empty, or each of the four decorations)."""
from PIL import Image

W, H = 240, 160
ROOM_BANKS = 8                     # BG banks 8-15 (free in every area)

# the shared furniture colors
F = dict(out=(40, 28, 30), wd=(98, 60, 40), wm=(146, 96, 58), wl=(196, 144, 92), wdd=(74, 44, 30),
         red=(196, 64, 52), redl=(236, 120, 96), redd=(150, 44, 40), pink=(255, 190, 180),
         cream=(244, 230, 196), creaml=(255, 248, 230), creamd=(214, 198, 170),
         lfd=(52, 112, 64), lfl=(112, 176, 88), lfll=(156, 208, 116), lfdd=(36, 80, 46),
         dew=(72, 150, 210), dewl=(176, 228, 246), dewd=(48, 106, 160),
         clay=(200, 120, 72), clayl=(232, 160, 104), clayd=(158, 84, 50),
         st=(150, 146, 150), stl=(200, 196, 200), std=(104, 98, 108),
         yel=(248, 208, 80), yell=(255, 240, 160), white=(255, 255, 255),
         sky=(150, 208, 236), skyl=(186, 228, 246), mud=(110, 78, 52), mudl=(146, 108, 72), coal=(52, 46, 52))
LAYOUT_COLORS = [
    dict(floor=[(150, 112, 88), (182, 142, 110), (210, 176, 140)], shadow=(118, 86, 70), sun=(232, 206, 170),
         wall=[(214, 178, 170), (236, 206, 196), (250, 232, 222)], line=(188, 146, 140),
         top=[(150, 52, 50), (196, 72, 62)]),                                                   # mushroom
    dict(floor=[(96, 66, 44), (124, 88, 58), (152, 112, 74)], shadow=(72, 48, 34), sun=(180, 140, 96),
         wall=[(108, 78, 54), (132, 98, 68), (158, 122, 86)], line=(84, 58, 40),
         top=[(52, 36, 26), (78, 56, 38)]),                                                     # burrow
    dict(floor=[(122, 84, 54), (158, 112, 72), (192, 146, 98)], shadow=(96, 64, 42), sun=(222, 180, 128),
         wall=[(176, 150, 112), (204, 180, 136), (226, 206, 162)], line=(150, 124, 90),
         top=[(96, 60, 38), (128, 84, 52)]),                                                    # cottage
]
VOID = (14, 12, 16)

# each home's layout (from its look in the village: main.c's village_home styles) and its Leafen's look
HOME_LAYOUT = [0, 0, 2, 1, 1, 0, 0, 0, 2, 2, 1, 1, 0]
LEAFEN_LOOKS = ["sprout", "leaf", "bud", "acorn", "dew"]   # main.c LOOK_* order
# the rooms (cells: floor x 2-12, y 3-7; the door at (7, 8)). B: a big piece (2x2), M: a tall one (1x2),
# T: the stool, S: the Leafen's own piece (2x2), a/b: decoration spots, O: where the owner stands.
# Column 7 from the door up, and the rug (6-8, 5-6), stay open.
LAYOUTS = [
    dict(B=[(2, 3), (10, 3), (2, 6)], M=[(4, 3), (9, 3)], T=(4, 7), S=(10, 6), a=(5, 3), b=(12, 5), O=(8, 4)),
    dict(B=[(2, 3), (11, 3), (9, 6)], M=[(10, 3), (4, 6)], T=(11, 7), S=(2, 6), a=(4, 3), b=(12, 6), O=(5, 4)),
    dict(B=[(2, 3), (9, 3), (10, 6)], M=[(2, 6), (3, 6)], T=(9, 7), S=(11, 3), a=(4, 3), b=(12, 7), O=(5, 5)),
]
RUG = (6, 5)
BIG = ["leaf_bed", "acorn_table", "pebble_shelf"]          # turned round by the Leafen's number
TALL = ["dew_lamp", "sprout_pot"]
SPECIALS = ["sp_lilypool", "sp_fruitbowl", "sp_twigs", "sp_vase", "sp_rocks", "sp_shiny", "sp_mudbath",
            "sp_reedbasket", "sp_waterbowl", "sp_lantern", "sp_kettle", "sp_mossbed", "sp_buttonjar"]
DECOR = ["dc_flowerpot", "dc_painting", "dc_bunting", "dc_candle"]

img = None
R = None


def put(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img.putpixel((x, y), c)


def get(x, y):
    return img.getpixel((x, y))


def shape_draw(mask, base, light, dark, out=None, shadow=False, hi=()):
    """Outline, a light band along the top edges, the lower part and right edge darker, an optional soft shadow
    on the floor to the lower right."""
    out = out or F['out']
    if shadow:
        for (x, y) in mask:
            for (sx, sy) in ((x + 2, y + 2), (x + 1, y + 2)):
                if (sx, sy) not in mask and get(sx, sy) in R['floor'] + [R['sun']]:
                    put(sx, sy, R['shadow'])
    ys = [y for _, y in mask]
    y0, y1 = min(ys), max(ys)
    for (x, y) in mask:
        if any((x + a, y + b) not in mask for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            put(x, y, out)
            continue
        top = (x, y - 1) not in mask or (x, y - 2) not in mask
        lower = (y - y0) > (y1 - y0) * 0.62 or (x + 1, y) not in mask or (x + 2, y) not in mask
        put(x, y, light if top else dark if lower else base)
    for (x, y) in hi:
        if (x, y) in mask:
            put(x, y, F['creaml'])


def ell(cx, cy, rx, ry):
    return {(x, y) for y in range(int(cy - ry) - 1, int(cy + ry) + 2) for x in range(int(cx - rx) - 1, int(cx + rx) + 2)
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 < 1}


def rect(x0, y0, x1, y1):
    return {(x, y) for y in range(y0, y1) for x in range(x0, x1)}


def rrect(x0, y0, x1, y1):
    return rect(x0, y0, x1, y1) - {(x0, y0), (x1 - 1, y0), (x0, y1 - 1), (x1 - 1, y1 - 1)}


WOOD = (F['wm'], F['wl'], F['wd'])
DARKWOOD = (F['wd'], F['wm'], F['wdd'])
CREAM = (F['cream'], F['creaml'], F['creamd'])
LEAF = (F['lfl'], F['lfll'], F['lfd'])
DEW = (F['dew'], F['dewl'], F['dewd'])
CLAY = (F['clay'], F['clayl'], F['clayd'])
RED = (F['red'], F['redl'], F['redd'])
STONE = (F['st'], F['stl'], F['std'])
GOLD = (F['yel'], F['yell'], F['clay'])


# ---- the shared furniture ----
def leaf_bed(x, y):            # a wooden bed, a cream pillow, a big leaf for a blanket
    shape_draw(rrect(x + 1, y + 4, x + 31, y + 31), *WOOD, shadow=True)
    shape_draw(rrect(x + 1, y + 1, x + 31, y + 9), *WOOD)
    shape_draw(rrect(x + 5, y + 8, x + 27, y + 15), *CREAM)
    shape_draw(rrect(x + 3, y + 15, x + 29, y + 29), *LEAF)
    for k in range(4, 26, 2):
        put(x + 16, y + 15 + k // 2, F['lfd'])
    for k in range(1, 6):
        put(x + 16 - k * 2, y + 18 + k * 2, F['lfd'])
        put(x + 16 + k * 2, y + 18 + k * 2, F['lfd'])


def acorn_table(x, y):         # a round table on a stump leg, a cup and a bowl of berries
    shape_draw(rect(x + 12, y + 18, x + 20, y + 30), F['wd'], F['wm'], F['out'], shadow=True)
    shape_draw(ell(x + 16, y + 14, 15, 9), *WOOD, shadow=True)
    for (a, b) in ((10, 12), (22, 15), (14, 17)):
        put(x + a, y + b, F['wd'])
    shape_draw(rrect(x + 7, y + 8, x + 12, y + 14), *DEW)
    shape_draw(ell(x + 21, y + 12, 4.5, 3), *CLAY)
    put(x + 20, y + 10, F['red']); put(x + 22, y + 10, F['lfl'])


def pebble_shelf(x, y):        # a low shelf: jars, books, pebbles, yarn
    shape_draw(rect(x + 1, y + 2, x + 31, y + 31), *DARKWOOD, shadow=True)
    for sy in (y + 13, y + 23):
        for xx in range(x + 2, x + 30):
            put(xx, sy, F['wl']); put(xx, sy + 1, F['wm'])
    shape_draw(rrect(x + 4, y + 5, x + 9, y + 13), *DEW)
    shape_draw(rrect(x + 10, y + 7, x + 14, y + 13), *CLAY)
    shape_draw(rect(x + 17, y + 4, x + 20, y + 13), *RED)
    shape_draw(rect(x + 20, y + 5, x + 23, y + 13), *LEAF)
    for (cx, cy) in ((7, 20), (12, 21), (18, 20), (24, 21)):
        shape_draw(ell(x + cx, y + cy, 2.6, 2.2), *STONE)
    shape_draw(ell(x + 9, y + 28, 4, 2.5), *CREAM)
    shape_draw(ell(x + 22, y + 28, 4, 2.5), *LEAF)


def dew_lamp(x, y):            # a glowing dewdrop on a stalk (16x32)
    stalk = {(x + 8, yy) for yy in range(y + 12, y + 30)} | {(x + 7, yy) for yy in range(y + 12, y + 30)}
    shape_draw(stalk | rect(x + 4, y + 28, x + 12, y + 31), F['lfd'], F['lfl'], F['lfdd'], shadow=True)
    drop = ell(x + 8, y + 8, 5.5, 6) | {(x + 8, y + 1), (x + 7, y + 2), (x + 8, y + 2)}
    shape_draw(drop, *DEW, hi={(x + 6, y + 5), (x + 6, y + 6), (x + 7, y + 4)})


def sprout_pot(x, y):          # a sprout in a clay pot (16x32)
    shape_draw(rrect(x + 3, y + 20, x + 13, y + 31), *CLAY, shadow=True)
    for (cx, cy, rx, ry) in ((5, 12, 4, 6), (11, 10, 4, 7), (8, 6, 3, 5)):
        shape_draw(ell(x + cx, y + cy, rx, ry), *LEAF)
    for yy in range(y + 10, y + 20):
        put(x + 8, yy, F['lfd'])


def stool(x, y):               # a red mushroom stool (16x16)
    shape_draw(rect(x + 6, y + 9, x + 10, y + 15), *CREAM, shadow=True)
    shape_draw(ell(x + 8, y + 7, 7, 4.5), *RED, hi={(x + 5, y + 5), (x + 9, y + 4), (x + 11, y + 7)})


def rug(x, y):                 # a woven oval rug (48x32), walkable
    shape_draw(ell(x + 24, y + 16, 23, 14), *RED)
    shape_draw(ell(x + 24, y + 16, 17, 9.5), *CLAY)
    shape_draw(ell(x + 24, y + 16, 10, 5), *CREAM)


# ---- each Leafen's own piece (32x32) ----
def sp_lilypool(x, y):         # Pip: a pond in a ring of stones, a lily pad and a flower
    shape_draw(ell(x + 16, y + 17, 15, 12), *STONE, shadow=True)
    shape_draw(ell(x + 16, y + 17, 11, 8), *DEW)
    shape_draw(ell(x + 13, y + 18, 5, 3.4), *LEAF)
    shape_draw(ell(x + 21, y + 14, 2.5, 2.2), F['redl'], F['pink'], F['red'])
    put(x + 18, y + 21, F['dewl']); put(x + 19, y + 21, F['dewl'])


def sp_fruitbowl(x, y):        # Mossy: a crate with a heaped bowl of fruit
    shape_draw(rect(x + 3, y + 16, x + 29, y + 31), *WOOD, shadow=True)
    for yy in (y + 21, y + 26):
        for xx in range(x + 4, x + 28):
            put(xx, yy, F['wd'])
    shape_draw(ell(x + 16, y + 16, 12, 5), *CLAY)
    for (cx, cy, col) in ((10, 11, RED), (16, 9, GOLD), (22, 11, RED), (13, 13, LEAF), (19, 13, RED)):
        shape_draw(ell(x + cx, y + cy, 3.4, 3.2), *col)
    put(x + 16, y + 5, F['lfd']); put(x + 17, y + 5, F['lfl'])


def sp_twigs(x, y):            # Twig: a woven twig dome on a stump, a leaf flag on top (redrawn)
    shape_draw(ell(x + 16, y + 26, 12, 5), *DARKWOOD, shadow=True)
    dome = ell(x + 16, y + 18, 11, 12)
    for (a, b) in dome:
        if b > y + 24:
            continue
        on = (a - b) % 4 == 0 or (a + b) % 4 == 0
        rim = any((a + d, b + e) not in dome for d, e in ((1, 0), (-1, 0), (0, -1)))
        if rim:
            put(a, b, F['wdd'])
        elif on:
            put(a, b, F['wm'] if (a + b) % 8 < 4 else F['wl'])
    for b in range(y, y + 7):
        put(x + 16, b, F['wd'])
    shape_draw(rrect(x + 17, y + 1, x + 23, y + 5), *LEAF)


def sp_vase(x, y):             # Posy: a vase of flowers on a little round table
    shape_draw(ell(x + 16, y + 24, 12, 6), *WOOD, shadow=True)
    shape_draw(rrect(x + 11, y + 10, x + 21, y + 25) - rect(x + 11, y + 10, x + 13, y + 14) - rect(x + 19, y + 10, x + 21, y + 14), *DEW)
    for (cx, cy, c) in ((10, 6, RED), (16, 3, (F['redl'], F['pink'], F['red'])), (22, 6, RED), (13, 9, CREAM), (20, 9, CREAM)):
        shape_draw(ell(x + cx, y + cy, 3, 2.6), *c)
    for (a, b) in ((12, 10), (16, 7), (20, 10)):
        put(x + a, y + b, F['lfd'])


def sp_rocks(x, y):            # Nub: a little stack of favourite rocks, and a few more round it
    shape_draw(ell(x + 16, y + 25, 9, 5), *STONE, shadow=True)
    shape_draw(ell(x + 15, y + 16, 7, 5), *STONE)
    shape_draw(ell(x + 17, y + 8, 5, 4), *STONE)
    for (cx, cy, r) in ((5, 27, 3), (27, 26, 3.4), (26, 18, 2.4), (6, 19, 2.4)):
        shape_draw(ell(x + cx, y + cy, r, r * 0.8), *STONE, shadow=True)
    put(x + 15, y + 8, F['out']); put(x + 19, y + 8, F['out'])      # a face on the top one
    put(x + 17, y + 10, F['std'])


def sp_shiny(x, y):            # Dewdrop: the shiny pebble on a cream stand, sparkling
    shape_draw(rrect(x + 9, y + 18, x + 23, y + 31), *CREAM, shadow=True)
    shape_draw(rrect(x + 7, y + 16, x + 25, y + 20), *CREAM)
    shape_draw(ell(x + 16, y + 11, 6, 5.5), *DEW, hi={(x + 13, y + 8), (x + 14, y + 8), (x + 13, y + 9)})
    for (a, b) in ((5, 6), (26, 4), (27, 13), (4, 14)):
        put(x + a, y + b, F['white'])
        for (d, e) in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            put(x + a + d, y + b + e, F['dewl'])


def sp_mudbath(x, y):          # Bog: a wooden tub of warm mud, bubbling
    shape_draw(ell(x + 16, y + 19, 15, 11), *WOOD, shadow=True)
    for a in range(x + 4, x + 29, 5):
        for b in range(y + 20, y + 29):
            if get(a, b) in (F['wm'], F['wd']):
                put(a, b, F['wd'])
    shape_draw(ell(x + 16, y + 16, 11, 6.5), F['mud'], F['mudl'], F['wdd'])
    for (cx, cy) in ((11, 15), (20, 17), (16, 13)):
        shape_draw(ell(x + cx, y + cy, 2, 1.6), F['mudl'], F['wl'], F['mud'])


def sp_reedbasket(x, y):       # Reedy: a woven basket, reeds standing in it
    for (a, k) in ((8, 0), (12, 1), (16, 0), (20, 1), (24, 0)):
        for b in range(y + 2 + k * 3, y + 18):
            put(x + a, b, F['lfd'] if b > y + 6 else F['clay'])
            put(x + a + 1, b, F['lfl'] if b > y + 6 else F['clayl'])
    body = rrect(x + 4, y + 14, x + 28, y + 31)
    shape_draw(body, *WOOD, shadow=True)
    for (a, b) in body:
        if (a + b) % 4 == 0 and get(a, b) != F['out']:
            put(a, b, F['wd'])
    for a in range(x + 4, x + 28):
        put(a, y + 15, F['wl'])


def sp_waterbowl(x, y):        # Puddle: a big clay bowl of clean water
    shape_draw(ell(x + 16, y + 20, 14, 10), *CLAY, shadow=True)
    shape_draw(ell(x + 16, y + 17, 11, 6), *DEW)
    shape_draw(ell(x + 16, y + 17, 5, 2.4), F['dew'], F['dewl'], F['dew'], out=F['dewl'])
    put(x + 11, y + 15, F['white']); put(x + 12, y + 15, F['white'])


def sp_lantern(x, y):          # Wick: a tall lantern, lit, its glow on the floor
    for (a, b) in ell(x + 16, y + 28, 13, 4):
        if get(a, b) in R['floor'] and (a + b) % 2 == 0:
            put(a, b, R['sun'])
    shape_draw(rect(x + 15, y + 22, x + 18, y + 30), F['coal'], F['std'], F['out'])
    shape_draw(rect(x + 11, y + 28, x + 22, y + 31), F['coal'], F['std'], F['out'])
    shape_draw(rrect(x + 10, y + 6, x + 23, y + 23), F['coal'], F['std'], F['out'])
    shape_draw(rect(x + 12, y + 8, x + 21, y + 21), F['yel'], F['yell'], F['clay'])
    shape_draw(ell(x + 16.5, y + 15, 2.5, 3.6), F['clayl'], F['white'], F['red'])
    shape_draw(rect(x + 9, y + 3, x + 24, y + 7), F['coal'], F['std'], F['out'])
    for b in range(y + 8, y + 21):
        put(x + 16, b, F['coal'])


def sp_kettle(x, y):           # Soot: a little stone stove, a sooty kettle steaming on it
    shape_draw(rrect(x + 3, y + 16, x + 29, y + 31), *STONE, shadow=True)
    shape_draw(rect(x + 11, y + 23, x + 21, y + 29), F['coal'], F['red'], F['out'])
    put(x + 14, y + 25, F['yel']); put(x + 17, y + 26, F['clayl'])
    shape_draw(ell(x + 15, y + 13, 7, 5), F['coal'], F['std'], F['out'])
    shape_draw(rect(x + 21, y + 10, x + 26, y + 13), F['coal'], F['std'], F['out'])
    for (a, b) in ((24, 7), (25, 5), (24, 3), (25, 1), (14, 6), (15, 4)):
        put(x + a, y + b, F['stl'])


def sp_mossbed(x, y):          # Tuft: a big soft cushion of moss
    shape_draw(ell(x + 16, y + 20, 15, 10), *LEAF, shadow=True)
    for (cx, cy) in ((9, 16), (16, 14), (23, 16), (12, 22), (20, 22)):
        shape_draw(ell(x + cx, y + cy, 4, 3), F['lfl'], F['lfll'], F['lfd'], out=F['lfd'])
    for (a, b) in ((8, 19), (17, 18), (24, 20), (14, 25)):
        put(x + a, y + b, F['yel'])


def sp_buttonjar(x, y):        # Button: a big glass jar of buttons
    jar = rrect(x + 7, y + 6, x + 25, y + 31)
    shape_draw(jar, F['dewl'], F['white'], F['dew'], out=F['dewd'], shadow=True)
    shape_draw(rect(x + 9, y + 3, x + 23, y + 7), *DARKWOOD)
    for (cx, cy, col) in ((11, 26, RED), (16, 27, GOLD), (21, 26, LEAF), (13, 21, DEW), (19, 21, RED), (16, 16, GOLD),
                          (11, 16, LEAF)):
        shape_draw(ell(x + cx, y + cy, 2.6, 2.2), *col)
    for b in range(y + 9, y + 24):
        put(x + 9, b, F['white'])


# ---- decorations (16x16, on a spot) ----
def dc_spot(x, y):             # an empty spot: a faint dashed square
    for k in range(3, 13, 2):
        for (a, b) in ((k, 3), (k, 12), (3, k), (12, k)):
            put(x + a, y + b, R['floor'][0])        # v0.20.5 (Caleb): a little darker, easier to see


def dc_flowerpot(x, y):
    shape_draw(rrect(x + 4, y + 9, x + 12, y + 15), *CLAY, shadow=True)
    for (cx, cy, col) in ((5, 5, RED), (11, 5, GOLD), (8, 3, RED)):
        shape_draw(ell(x + cx, y + cy, 2.4, 2.2), *col)
    put(x + 7, y + 8, F['lfd']); put(x + 9, y + 8, F['lfl']); put(x + 8, y + 7, F['lfd'])


def dc_painting(x, y):         # a little painting on an easel
    for b in range(y + 2, y + 16):
        put(x + 4 + (b - y) // 5, b, F['wd']); put(x + 11 - (b - y) // 5, b, F['wd'])
    shape_draw(rect(x + 2, y + 1, x + 14, y + 11), *WOOD, shadow=True)
    for (a, b) in rect(x + 4, y + 3, x + 12, y + 9):
        put(a, b, F['sky'] if b < y + 6 else F['lfl'])
    put(x + 9, y + 4, F['yel']); put(x + 6, y + 7, F['red'])


def dc_bunting(x, y):          # bunting on two little posts
    for b in range(y + 3, y + 15):
        put(x + 1, b, F['wd']); put(x + 14, b, F['wd'])
    for a in range(x + 1, x + 15):
        put(a, y + 4 + abs(a - x - 7) // 4 * 0, F['wdd'])
    for k, col in enumerate((F['red'], F['yel'], F['dew'], F['lfl'])):
        a = x + 2 + k * 3
        for (d, e) in ((0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1), (1, 2)):
            put(a + d, y + 5 + e, col)
    shape_draw(rect(x, y + 14, x + 3, y + 16), *DARKWOOD)
    shape_draw(rect(x + 13, y + 14, x + 16, y + 16), *DARKWOOD)


def dc_candle(x, y):
    shape_draw(ell(x + 8, y + 13, 5, 2.4), *CLAY, shadow=True)
    shape_draw(rect(x + 6, y + 5, x + 10, y + 13), *CREAM)
    shape_draw(ell(x + 8, y + 2.6, 1.6, 2.2), F['yel'], F['white'], F['clay'], out=F['clay'])


DRAW = {n: globals()[n] for n in BIG + TALL + ["stool"] + SPECIALS + DECOR + ["dc_spot"]}


# ---- the room's shell ----
def shell(lay):
    for y in range(H):
        for x in range(W):
            put(x, y, VOID)
    for y in range(48, 128):                       # planks, three tones, staggered joints
        for x in range(32, 208):
            row = (y - 48) // 8
            c = R['floor'][1]
            if (y - 48) % 8 == 7 or (x + row * 13) % 29 == 0:
                c = R['floor'][0]
            elif (y - 48) % 8 == 0 or (x * 7 + y * 3) % 23 == 0:
                c = R['floor'][2]
            put(x, y, c)
    for y in range(8, 48):                         # the back wall: a top edge, then its face two tiles tall
        for x in range(16, 224):
            if y < 16:
                c = R['top'][0] if y < 10 or y == 15 else R['top'][1]
            else:
                yy = y - 16
                c = R['wall'][1]
                if lay == 0:                       # gills
                    if x % 6 == 0:
                        c = R['line']
                    elif yy < 6:
                        c = R['wall'][2]
                elif lay == 1:                     # packed earth with stones
                    if (x * 5 + y * 11) % 37 == 0 or (x * 3 + y * 7) % 41 == 0:
                        c = R['wall'][0]
                    elif yy < 4:
                        c = R['wall'][2]
                else:                              # planks with a rail
                    if x % 12 == 0:
                        c = R['line']
                    elif yy in (14, 15):
                        c = R['wall'][0] if yy == 15 else R['wall'][2]
                    elif yy < 4:
                        c = R['wall'][2]
                if yy >= 29:
                    c = R['wall'][0]
                if yy == 31:
                    c = F['out']
            put(x, y, c)
    for y in range(48, 52):                        # the wall's shadow on the floor
        for x in range(32, 208):
            if y < 50 or (x + y) % 2 == 0:
                put(x, y, R['shadow'])
    for y in range(16, 144):                       # the side walls and the front wall, from above
        for x in list(range(16, 32)) + list(range(208, 224)):
            put(x, y, F['out'] if x in (31, 208) and y >= 48 else R['top'][0] if x in (16, 223) else R['top'][1])
    for y in range(128, 144):
        for x in range(16, 224):
            if 112 <= x < 128:
                continue
            put(x, y, F['out'] if y == 128 else R['top'][0] if y >= 142 else R['top'][1])
    for y in range(128, 144):                      # the doorway: floor, and the mat
        for x in range(112, 128):
            put(x, y, R['floor'][1] if (y % 8) != 7 else R['floor'][0])
    shape_draw(rrect(113, 130, 127, 142), *CLAY)
    for x in range(115, 126, 3):
        put(x, 136, F['cream'])
    # the window (back wall, cols 6-8) and its sunlight on the floor
    wx, wy = 96, 16
    shape_draw(rrect(wx + 6, wy + 3, wx + 42, wy + 28), *WOOD)
    for (a, b) in rect(wx + 9, wy + 6, wx + 39, wy + 25):
        put(a, b, F['sky'] if b < wy + 14 else F['skyl'])
    for (a, b) in ((14, 10), (15, 10), (16, 9), (17, 10), (18, 10), (30, 16), (31, 15), (32, 16)):
        put(wx + a, wy + b, F['white'])
    for yy in range(wy + 6, wy + 25):
        put(wx + 24, yy, F['wm'])
    for xx in range(wx + 9, wx + 39):
        put(xx, wy + 15, F['wm'])
    shape_draw(rect(wx + 3, wy + 26, wx + 45, wy + 31), *WOOD)
    for y in range(52, 82):
        for x in range(100 + (y - 52) // 3, 136 + (y - 52) // 3):
            if get(x, y) in R['floor'] and (x + y) % 2 == 0:
                put(x, y, R['sun'])


def home_plan(k):
    """The pieces of home k: (name, x cell, y cell), back to front."""
    L = LAYOUTS[HOME_LAYOUT[k]]
    plan = [(BIG[(i + k) % 3], x, y) for i, (x, y) in enumerate(L['B'])]
    plan += [(TALL[(i + k) % 2], x, y) for i, (x, y) in enumerate(L['M'])]
    plan += [("stool", *L['T']), (SPECIALS[k], *L['S'])]
    return sorted(plan, key=lambda p: (p[2], p[1]))


SIZE = {n: (2, 2) for n in BIG + SPECIALS}
SIZE.update({n: (1, 2) for n in TALL})
SIZE["stool"] = (1, 1)


def render(k):
    """Home k's picture, and each decoration spot's 16x16 pictures (empty, then the four decorations)."""
    global img, R
    lay = HOME_LAYOUT[k]
    R = LAYOUT_COLORS[lay]
    img = Image.new('RGB', (W, H))
    shell(lay)
    rug(RUG[0] * 16, RUG[1] * 16)
    for (name, cx, cy) in home_plan(k):
        DRAW[name](cx * 16, cy * 16)
    L = LAYOUTS[lay]
    spots = []
    base = img.copy()
    for s in ('a', 'b'):
        sx, sy = L[s][0] * 16, L[s][1] * 16
        pics = []
        for name in ["dc_spot"] + DECOR:
            img = base.copy()
            DRAW[name](sx, sy)
            pics.append(img.crop((sx, sy, sx + 16, sy + 16)))
        spots.append(pics)
    img = base
    DRAW["dc_spot"](L['a'][0] * 16, L['a'][1] * 16)
    DRAW["dc_spot"](L['b'][0] * 16, L['b'][1] * 16)
    return img, spots


def logic(k, ids):
    """Home k's 15x10 map of tile ids (ids: name -> main.c's tile number)."""
    lay = HOME_LAYOUT[k]
    L = LAYOUTS[lay]
    wall = ids[["wall_gills", "wall_roots", "wall_planks"][lay]]
    m = [[ids["room_void"]] * 15 for _ in range(10)]
    for y in range(10):
        for x in range(15):
            if 1 <= y <= 8 and 1 <= x <= 13:
                m[y][x] = wall
            if 3 <= y <= 7 and 2 <= x <= 12:
                m[y][x] = ids["room_floor"]
    m[8][7] = ids["door_mat"]
    for yy in (RUG[1], RUG[1] + 1):
        for xx in range(RUG[0], RUG[0] + 3):
            m[yy][xx] = ids["rug"]
    for (name, cx, cy) in home_plan(k):
        w, h = SIZE[name]
        tid = ids["sp_first"] + k if name in SPECIALS else ids[name]
        for yy in range(cy, cy + h):
            for xx in range(cx, cx + w):
                m[yy][xx] = tid
    m[L['a'][1]][L['a'][0]] = ids["dc_spot"]
    m[L['b'][1]][L['b'][0]] = ids["dc_spot"]
    return [t for row in m for t in row]


def to15(c):
    return (c[0] >> 3) | ((c[1] >> 3) << 5) | ((c[2] >> 3) << 10)


def merge_close(pic, spots, limit=30):
    """Fewer colors: the least used of the two closest colors takes the other's, until `limit` are left."""
    ims = [pic] + [p for s in spots for p in s]
    count = {}
    for im in ims:
        for c in im.getdata():
            c = tuple(v >> 3 << 3 for v in c)
            count[c] = count.get(c, 0) + 1
    to = {c: c for c in count}
    live = set(count)
    while len(live) > limit:
        cs = sorted(live)
        best = None
        for i in range(len(cs)):
            for j in range(i + 1, len(cs)):
                d = sum((a - b) ** 2 for a, b in zip(cs[i], cs[j]))
                if best is None or d < best[0]:
                    best = (d, cs[i], cs[j])
        _, a, b = best
        keep, drop = (a, b) if count[a] >= count[b] else (b, a)
        count[keep] += count[drop]
        live.discard(drop)
        for c in to:
            if to[c] == drop:
                to[c] = keep
    def remap(im):
        out = im.copy()
        out.putdata([to[tuple(v >> 3 << 3 for v in c)] for c in im.getdata()])
        return out
    return remap(pic), [[remap(p) for p in s] for s in spots]


def pack_banks(k, uniq):
    """Give each tile a bank of at most 15 colors. The colors most tiles use (the floor, the outline) go in every
    bank; the tiles' other color sets are packed best-fit, largest first, with a few shuffled retries."""
    import random as _r
    freq = {}
    for t in uniq:
        for c in set(t):
            freq[c] = freq.get(c, 0) + 1
    for n_common in (6, 7, 5, 8, 4):
        common = set(sorted(freq, key=lambda c: -freq[c])[:n_common])
        sets = list({frozenset(set(t) - common) for t in uniq})
        sets = [a for a in sets if not any(a < b for b in sets)]      # a set inside another goes with it
        cl = [set(a) for a in sets]                # merge the two groups that share most, until few enough
        while len(cl) > ROOM_BANKS:
            best = None
            for i in range(len(cl)):
                for j in range(i + 1, len(cl)):
                    u = len(cl[i] | cl[j])
                    if u + len(common) <= 15:
                        cost = u - max(len(cl[i]), len(cl[j]))
                        if best is None or (cost, u) < best[:2]:
                            best = (cost, u, i, j)
            if best is None:
                break
            _, _, i, j = best
            cl[i] |= cl[j]
            del cl[j]
        if len(cl) <= ROOM_BANKS:
            banks = cl + [set() for _ in range(ROOM_BANKS - len(cl))]
            which = {}
            for t in uniq:
                rest = set(t) - common
                which[t] = next(b for b in range(ROOM_BANKS) if rest <= banks[b])
            return [b | common for b in banks], which
    raise AssertionError(f"home {k}: its colors do not fit {ROOM_BANKS} banks")


def pack(k, tile_base, bank0):
    """Home k cut into 8x8 tiles (shared where equal), each tile given one of ROOM_BANKS 15-color banks.
    Returns (tile words, 600 screen entries, decoration entries [2][5][4], palette [ROOM_BANKS*16])."""
    pic, spots = render(k)
    pic, spots = merge_close(pic, spots)
    cells = []                                     # each tile as a tuple of 15-bit colors
    for ty in range(20):
        for tx in range(30):
            cells.append(tuple(to15(pic.getpixel((tx * 8 + x, ty * 8 + y))) for y in range(8) for x in range(8)))
    dec = []
    for s in spots:
        for p in s:
            for (ox, oy) in ((0, 0), (8, 0), (0, 8), (8, 8)):
                dec.append(tuple(to15(p.getpixel((ox + x, oy + y))) for y in range(8) for x in range(8)))
    uniq = list(dict.fromkeys(cells + dec))
    banks, which = pack_banks(k, uniq)
    order = [sorted(b) for b in banks]
    pal = []
    for b in order:
        pal += [0] + b + [0] * (15 - len(b))
    words, index = [], {}
    for t in uniq:
        idx = {c: i + 1 for i, c in enumerate(order[which[t]])}
        for y in range(8):
            w = 0
            for x in range(8):
                w |= idx[t[y * 8 + x]] << (4 * x)
            words.append(w)
        index[t] = len(index)
    entry = lambda t: (tile_base + index[t]) | ((bank0 + which[t]) << 12)
    assert len(uniq) <= 1024 - tile_base, f"home {k}: {len(uniq)} tiles, room for {1024 - tile_base}"
    return words, [entry(t) for t in cells], [entry(t) for t in dec], pal, len(uniq)


if __name__ == "__main__":                         # a look at all 13 (scratch)
    import sys
    sheet = Image.new('RGB', (W * 4, H * 4))
    for k in range(13):
        sheet.paste(render(k)[0], ((k % 4) * W, (k // 4) * H))
    sheet.save(sys.argv[1])
