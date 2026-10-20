"""Shorten the player's walking legs (run after tools/import_sheet.py --grid).

The generated stride frames cut the coat short and draw long legs with wide boots.
This rebuilds walk frames 1 and 3 of each direction from that direction's standing frame:
the coat keeps its full length and only short boots show below it, in stride positions.
The passing frame (2) is kept as drawn.
Run: python3 tools/player_feet.py  (edits art/player_sprite.png in place)
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATH = os.path.join(ROOT, "art", "player_sprite.png")

# Bottom two rows (cell rows 30-31) for each walk frame. '.' keeps the standing pixel, ' ' clears it.
# Boots: 1 outline, A boot, F leather sole.
FEET = {
    # front and back: feet spread, the forward foot planted on the bottom row, the back heel lifted
    ("front", 1): ["   1AA1  1A1    ", "   1FF1   11    "],
    ("front", 3): ["    1A1  1AA1   ", "    11   1FF1   "],
    ("back", 1):  ["   1AA1  1A1    ", "   1FF1   11    "],
    ("back", 3):  ["    1A1  1AA1   ", "    11   1FF1   "],
    # side (faces right): feet apart in a small step
    ("side", 1):  ["  1AA1 1AA1     ", " 1FF1   1FF1    "],
    ("side", 3):  ["   1AA11AA1     ", "  1FF1  1FF1    "],
}
KEEP_COLS = {"front": (12, 13, 14, 15), "back": (), "side": (11, 12, 13, 14, 15)}   # staff stays


def main():
    im = Image.open(PATH)
    px = im.load()
    for r, name in enumerate(("front", "back", "side")):
        stand = (r * 4) * 16
        for k in (1, 3):
            f = (r * 4 + k) * 16
            # coat and body: copy the standing frame's rows 22-29
            for y in range(22, 30):
                for x in range(16):
                    px[f + x, y] = px[stand + x, y]
            for i, row in enumerate(FEET[(name, k)]):
                y = 30 + i
                for x, ch in enumerate(row):
                    if x in KEEP_COLS[name]:
                        px[f + x, y] = px[stand + x, y]
                    elif ch == ".":
                        px[f + x, y] = px[stand + x, y]
                    elif ch == " ":
                        px[f + x, y] = 0
                    else:
                        px[f + x, y] = int(ch, 16)
    im.save(PATH)
    print("shortened walk legs in", PATH)


if __name__ == "__main__":
    main()
