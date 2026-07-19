#!/usr/bin/env python3
# img2sprites.py -- mvcz80 asset pipeline: convert a PNG sprite sheet into an MSX
# hardware-sprite pattern table (8x8 or 16x16), plus a per-sprite colour byte.
#
# MSX sprites are monochrome per pattern: each pixel is on (draw in the sprite's
# colour) or off (transparent). A pixel is OFF if it is transparent (alpha < 128) or,
# for images without alpha, pure black -- everything else is ON. The sprite's colour
# is the nearest TMS9918 palette index of its most-common lit pixel (for SCREEN 2
# sprite-mode-1 attribute bytes).
#
#   8x8  (size 0): 8 bytes/sprite, one per row, bit7 = leftmost pixel.
#   16x16 (size 1): 32 bytes/sprite in the MSX quadrant order TL, BL, TR, BR
#                   (each quadrant an 8x8 block of 8 bytes).
#
# --mode2 (MSX2 sprite mode 2, "OR colour"): a real multi-colour sprite is built from
# several monochrome planes stacked at the same position. Each cell is decomposed into
# one plane per distinct colour; the base plane's colour byte has CC=0 and the stacked
# planes have CC=0x40, so the VDP OR-combines overlapping dots. Plane count is uniform
# across the sheet (= max distinct colours), for simple indexing. The colour byte is
# EC(7)|CC(6)|IC(5)|0|colour(3-0); the caller writes it to all 16 lines of that plane's
# mode-2 colour table. (MSX2 Technical Handbook ch.4; msx.org "The OR Color".)
#
# Companion to img2tiles/img2bitmap; deterministic + dependency-light (Python+Pillow),
# golden-tested. Usage: img2sprites.py <in.png> <name> <size 8|16> [out.h] [--mode2]
import sys
from PIL import Image

SPR_CC = 0x40   # colour-combination bit: this plane OR-combines onto the base plane

# Standard TMS9918 16-colour palette (RGB); index 0 = transparent.
PALETTE = [
    (0, 0, 0), (0, 0, 0), (33, 200, 66), (94, 220, 120),
    (84, 85, 237), (125, 118, 252), (212, 82, 77), (66, 235, 245),
    (252, 85, 84), (255, 121, 120), (212, 193, 84), (230, 206, 128),
    (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]

def nearest(rgb):
    r, g, b = rgb[:3]
    return min(range(1, 16), key=lambda i: (r-PALETTE[i][0])**2 + (g-PALETTE[i][1])**2 + (b-PALETTE[i][2])**2)

def lit(px, x, y):
    r, g, b, a = px[x, y]
    if a < 128:
        return False
    return not (r == 0 and g == 0 and b == 0)   # black == transparent for alpha-less art

def block8(px, ox, oy, on):
    """8 bytes for the 8x8 block at (ox,oy): one per row, bit7 = leftmost. `on(x,y)`
    decides whether a pixel is set."""
    out = []
    for row in range(8):
        bits = 0
        for col in range(8):
            bits = (bits << 1) | (1 if on(ox + col, oy + row) else 0)
        out.append(bits)
    return out

def pattern_of(px, ox, oy, size, on):
    """Pattern bytes for one sprite: 8 bytes (8x8) or 32 in MSX quadrant order (16x16)."""
    if size == 8:
        return block8(px, ox, oy, on)
    return (block8(px, ox,     oy,     on) + block8(px, ox,     oy + 8, on) +
            block8(px, ox + 8, oy,     on) + block8(px, ox + 8, oy + 8, on))   # TL,BL,TR,BR

def cell_colours(px, ox, oy, size):
    """Distinct TMS9918 palette indices of the lit pixels in a cell, ascending."""
    idxs = set()
    for y in range(size):
        for x in range(size):
            if lit(px, ox + x, oy + y):
                idxs.add(nearest(px[ox + x, oy + y]))
    return sorted(idxs)

def sprite_colour(px, ox, oy, size):
    counts = {}
    for y in range(size):
        for x in range(size):
            if lit(px, ox + x, oy + y):
                c = nearest(px[ox + x, oy + y])
                counts[c] = counts.get(c, 0) + 1
    if not counts:
        return 1   # empty sprite -> black
    return sorted(counts, key=lambda c: (-counts[c], c))[0]

def carr(name, data):
    return f"const u8 {name}[{len(data)}]={{{','.join(str(b) for b in data)}}};\n"

def convert(path, size, mode2=False):
    if size not in (8, 16):
        sys.exit("size must be 8 or 16")
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    if w % size or h % size:
        sys.exit(f"image {w}x{h} must be a multiple of {size} in both dimensions")
    px = im.load()
    sw, sh = w // size, h // size
    cells = [(sx * size, sy * size) for sy in range(sh) for sx in range(sw)]

    if not mode2:
        pattern, colour = [], []
        for ox, oy in cells:
            pattern.extend(pattern_of(px, ox, oy, size, lambda x, y: lit(px, x, y)))
            colour.append(sprite_colour(px, ox, oy, size))
        return dict(size=size, count=len(cells), planes=1,
                    pattern=pattern, colour=colour)

    # --- mode 2: one monochrome plane per distinct colour, uniform plane count ---
    per_cell = [cell_colours(px, ox, oy, size) for ox, oy in cells]
    planes = max((len(c) for c in per_cell), default=1) or 1
    if planes > 4:
        sys.stderr.write(f"warning: {planes} colours in a sprite -> {planes} planes; "
                         f"a combined group is limited (8 sprites/line in mode 2)\n")
    pattern, colour = [], []
    for (ox, oy), cols in zip(cells, per_cell):
        for p in range(planes):
            if p < len(cols):
                idx = cols[p]
                pattern.extend(pattern_of(px, ox, oy, size,
                                          lambda x, y, i=idx: lit(px, x, y) and nearest(px[x, y]) == i))
                colour.append(idx | (0 if p == 0 else SPR_CC))   # base CC=0, stacked CC=1
            else:
                pattern.extend([0] * (8 if size == 8 else 32))    # empty plane
                colour.append(0)
    return dict(size=size, count=len(cells), planes=planes,
                pattern=pattern, colour=colour)

def main():
    argv = [a for a in sys.argv[1:] if a != "--mode2"]
    mode2 = "--mode2" in sys.argv[1:]
    if len(argv) < 3:
        sys.exit("usage: img2sprites.py <in.png> <name> <size 8|16> [out.h] [--mode2]")
    path, name, size = argv[0], argv[1], int(argv[2])
    out = argv[3] if len(argv) > 3 else name + "_sprites.h"
    r = convert(path, size, mode2)
    U = name.upper()
    mnote = f", {r['planes']} planes/sprite (mode 2 OR-colour)" if mode2 else ""
    lines = [
        f"// {out} -- GENERATED by img2sprites.py from {path} (do not edit).",
        f"// MSX {r['size']}x{r['size']} sprite bank: {r['count']} sprites{mnote} "
        f"({len(r['pattern'])} pattern bytes).",
        '#include "types.h"', "",
        carr(name + "_pattern", r["pattern"]),
        carr(name + "_colour", r["colour"]),
        f"#define {U}_COUNT {r['count']}",
        f"#define {U}_SIZE {r['size']}",
        f"#define {U}_PLANES {r['planes']}",
    ]
    open(out, "w").write("\n".join(lines) + "\n")
    print(f"wrote {out}: {r['count']} {r['size']}x{r['size']} sprites"
          f"{' x ' + str(r['planes']) + ' planes' if mode2 else ''}, "
          f"{len(r['pattern'])}B pattern + {len(r['colour'])}B colour")

if __name__ == "__main__":
    main()
