"""Turn a designer's upscaled sprite image into a true low-res indexed sheet.

Grid mode (preferred; for images made with the 4x3 player prompt, exact 10x10 pixel blocks):
  python3 tools/import_sheet.py --grid SRC OUT
  Finds the frames, reads the center of each block, and snaps every color to the player
  palette in tools/player_palette.py. Rows: front, back, side (faces right); columns: stand,
  walk1, passing, walk3. Output: player_sprite.png layout (12 frames of 16x32, art 16x24).

Strip mode (for blurry or unevenly scaled images, e.g. a WebP of one long row):
  python3 tools/import_sheet.py SRC OUT FRAMES ART_HEIGHT CELL_HEIGHT
  SRC          image with FRAMES characters in one row on a magenta background
               (it may be scaled up and blurred, e.g. an AI image or a WebP)
  OUT          indexed PNG: FRAMES cells of 16 x CELL_HEIGHT, index 0 = transparent magenta
  ART_HEIGHT   the real height of the art in pixels (the source is scaled to this)
  CELL_HEIGHT  16 or 32; the art sits on the bottom of each cell

Each output pixel takes the median color at the center of its block (where blur is lowest),
then all frames share one 15-color palette.
"""
import os
import sys
from PIL import Image


def runs(flags):
    out, i = [], 0
    while i < len(flags):
        if flags[i]:
            s = i
            while i < len(flags) and flags[i]:
                i += 1
            out.append((s, i - 1))
        i += 1
    return out


def grid_main(src, out):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from player_palette import PAL
    im = Image.open(src).convert("RGB")
    w, h = im.size
    px = im.load()

    def is_bg(c):
        r, g, b = c
        return r > 180 and g < 110 and b > 180

    bands = runs([any(not is_bg(px[x, y]) for x in range(w)) for y in range(h)])
    if len(bands) != 3:
        sys.exit(f"expected 3 rows of figures, found {len(bands)}: {bands}")
    # Pixel block size: the most common length of a same-color run.
    from collections import Counter
    lens = Counter()
    for y in range(0, h, 3):
        x = 0
        while x < w:
            c, s0 = px[x, y], x
            while x < w and px[x, y] == c:
                x += 1
            lens[x - s0] += 1
    block = min(k for k, v in lens.items() if v >= max(lens.values()) // 3 and k > 1)
    sheet = Image.new("P", (16 * 12, 32), 0)
    sheet.putpalette([c for rgb in PAL for c in rgb] + [0, 0, 0] * 240)
    snapped = 0
    for r, (y0, y1) in enumerate(bands):
        cols = runs([any(not is_bg(px[x, y]) for y in range(y0, y1 + 1)) for x in range(w)])
        if len(cols) != 4:
            sys.exit(f"row {r}: expected 4 figures, found {len(cols)}: {cols}")
        art_h = round((y1 - y0 + 1) / block)
        if art_h > 24:
            sys.exit(f"row {r}: figures are {art_h} pixels tall, more than 24")
        for c, (x0, x1) in enumerate(cols):
            fw = round((x1 - x0 + 1) / block)
            ox = (16 - fw) // 2
            for ty in range(art_h):
                for tx in range(fw):
                    col = px[x0 + tx * block + block // 2, y0 + ty * block + block // 2]
                    if is_bg(col):
                        continue
                    d, idx = min((sum((col[k] - PAL[i][k]) ** 2 for k in range(3)), i) for i in range(1, 16))
                    snapped += d > 0
                    # feet on the bottom row of the 24-pixel art box (cell rows 8..31)
                    sheet.putpixel(((r * 4 + c) * 16 + ox + tx, 32 - art_h + ty), idx)
    sheet.save(out)
    print(f"wrote {out}: block {block}px, {snapped} pixels needed color snapping")


def main():
    if sys.argv[1] == "--grid":
        return grid_main(sys.argv[2], sys.argv[3])
    src, out, frames, art_h, cell_h = sys.argv[1], sys.argv[2], *map(int, sys.argv[3:6])
    im = Image.open(src).convert("RGB")
    w, h = im.size
    px = im.load()

    def is_bg(c):
        r, g, b = c
        return r > 180 and g < 110 and b > 180

    cols = [any(not is_bg(px[x, y]) for y in range(h)) for x in range(w)]
    runs, x = [], 0
    while x < w:
        if cols[x]:
            s = x
            while x < w and cols[x]:
                x += 1
            runs.append((s, x - 1))
        x += 1
    if len(runs) != frames:
        sys.exit(f"found {len(runs)} figures, expected {frames}: {runs}")
    rows = [y for y in range(h) if any(not is_bg(px[x, y]) for x in range(w))]
    top, bot = rows[0], rows[-1]
    scale = (bot - top + 1) / art_h

    def is_fringe(c):
        # magenta blended into an edge: red and blue well above green
        r, g, b = c
        return r > 60 and b > 60 and g < 0.6 * min(r, b)

    def sample(cx, cy):
        cs = [px[min(w - 1, max(0, int(cx) + dx)), min(h - 1, max(0, int(cy) + dy))]
              for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
        bg_like = sum(is_bg(c) or (is_fringe(c) and sum(c) > 300) for c in cs)
        if bg_like >= 5:
            return None
        cs = sorted((c for c in cs if not is_bg(c)), key=sum)
        c = cs[len(cs) // 2]
        if is_fringe(c):          # a dark fringe is really the outline
            return OUTLINE
        return c

    # The outline color: the darkest common color in the source figures.
    dark = [px[x, y] for y in range(top, bot + 1, 2) for x in range(0, w, 2)
            if not is_bg(px[x, y]) and sum(px[x, y]) < 90 and not is_fringe(px[x, y])]
    OUTLINE = sorted(dark, key=sum)[len(dark) // 2] if dark else (16, 12, 20)

    cells = []
    for x0, x1 in runs:
        nw = min(16, round((x1 - x0 + 1) / scale))
        off = (16 - nw) // 2
        cell = {}
        for ty in range(art_h):
            for tx in range(nw):
                c = sample(x0 + (tx + 0.5) * scale, top + (ty + 0.5) * scale)
                if c:
                    cell[(tx + off, ty)] = c
        cells.append(cell)

    colors = [c for cell in cells for c in cell.values()]
    strip = Image.new("RGB", (len(colors), 1))
    strip.putdata(colors)
    pal = strip.quantize(colors=15, method=Image.Quantize.MAXCOVERAGE, kmeans=8).getpalette()[:45]

    def nearest(c):
        return min(range(15), key=lambda i: sum((c[k] - pal[i * 3 + k]) ** 2 for k in range(3)))

    sheet = Image.new("P", (16 * frames, cell_h), 0)
    sheet.putpalette([255, 0, 255] + pal + [0, 0, 0] * 240)
    for i, cell in enumerate(cells):
        for (tx, ty), c in cell.items():
            sheet.putpixel((i * 16 + tx, cell_h - art_h + ty), nearest(c) + 1)
    sheet.save(out)
    print(f"wrote {out}: {frames} frames, {scale:.2f} source px per pixel")


if __name__ == "__main__":
    main()
