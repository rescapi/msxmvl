#!/usr/bin/env python3
# img2bitmap.py -- mvcz80 asset pipeline (Category D2): convert a PNG into MSX2
# bitmap-mode VRAM data -- SCREEN 5/7 (GRAPHIC 4/5, 16 colours, 4 bits/pixel) or
# SCREEN 8 (GRAPHIC 6, 256 colours, 8 bits/pixel). Emits a linear C array you copy
# straight to VRAM (types.h compatible), plus the palette for SCREEN 5/7.
#
# SCREEN 5/7: two pixels per byte (high nibble = left pixel, low nibble = right);
#   each pixel is a 4-bit index into the 16-colour palette. The palette is
#   programmable -- the MSX2 default palette is used and emitted in VDP register
#   format (2 bytes/colour: byte0 = 0RRR0BBB, byte1 = 00000GGG) so you can load it.
# SCREEN 8: one byte per pixel in the fixed GRB332 form (bits GGGRRRBB); no palette.
#
# The companion to img2tiles (D1, SCREEN 2 tiles); same minimal + deterministic +
# dependency-light approach (Python + Pillow), no vendored SDK.
#
# D4 palette helper: pass --optimal (SCREEN 5/7 only) to derive a per-image optimal
# 16-colour palette by median-cut in the MSX2 9-bit (512-colour) space instead of
# forcing the fixed default palette -- far better for photographic/gradient art.
# Usage: img2bitmap.py <in.png> <name> <screen 5|7|8> [out.h] [--optimal]
import sys
from PIL import Image

# MSX2 default 16-colour palette, each channel 0..7 (used for SCREEN 5/7).
MSX2_PAL3 = [
    (0, 0, 0), (0, 0, 0), (1, 6, 1), (3, 7, 3), (1, 1, 7), (2, 3, 7), (5, 1, 1), (2, 6, 7),
    (7, 1, 1), (7, 3, 3), (6, 6, 1), (6, 6, 4), (1, 4, 1), (6, 2, 5), (5, 5, 5), (7, 7, 7),
]
# Expanded to 8-bit RGB for nearest-colour matching.
PAL8 = [tuple(round(c * 255 / 7) for c in rgb) for rgb in MSX2_PAL3]

def nearest(rgb, pal8):
    r, g, b = rgb[:3]
    return min(range(len(pal8)),
               key=lambda i: (r-pal8[i][0])**2 + (g-pal8[i][1])**2 + (b-pal8[i][2])**2)

def to_msx9(rgb):
    """Quantise 8-bit RGB to the MSX2 9-bit (3 bits/channel, 0..7) colour space."""
    return tuple(round(c * 7 / 255) for c in rgb[:3])

def median_cut(hist, k):
    """Deterministic median-cut: split the colour histogram (MSX2 9-bit space, weighted
    by pixel count) into k boxes, return each box's count-weighted average colour."""
    boxes = [list(hist.items())]                 # each box: [((r,g,b), count), ...]
    def rng(box, ch):
        vs = [c[0][ch] for c in box]
        return max(vs) - min(vs)
    def widest(box):
        return max(range(3), key=lambda ch: rng(box, ch))
    def priority(box):
        return sum(c for _, c in box) * max(rng(box, ch) for ch in range(3))
    while len(boxes) < k:
        cand = [b for b in boxes if len(b) > 1 and max(rng(b, ch) for ch in range(3)) > 0]
        if not cand:
            break
        box = max(cand, key=priority)
        ch = widest(box)
        box.sort(key=lambda item: (item[0][ch], item[0]))   # stable, tie-broken by colour
        total = sum(c for _, c in box)
        acc, cut = 0, 0
        for cut, (_, c) in enumerate(box):
            acc += c
            if acc * 2 >= total:
                break
        cut = max(0, min(cut, len(box) - 2))                # keep both halves non-empty
        boxes.remove(box)
        boxes.append(box[:cut+1])
        boxes.append(box[cut+1:])
    pal = []
    for b in boxes:
        tot = sum(c for _, c in b)
        pal.append(tuple(round(sum(col[ch] * c for col, c in b) / tot) for ch in range(3)))
    pal.sort()                                              # stable palette ordering
    while len(pal) < k:
        pal.append((0, 0, 0))
    return pal[:k]

def grb332(rgb):
    """RGB -> MSX SCREEN 8 byte: 3 bits green, 3 bits red, 2 bits blue (GGGRRRBB)."""
    r, g, b = rgb[:3]
    return ((g >> 5) << 5) | ((r >> 5) << 2) | (b >> 6)

def carr(name, data):
    return f"const u8 {name}[{len(data)}]={{{','.join(str(b) for b in data)}}};\n"

def convert(path, screen, optimal=False):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    if screen in (5, 7):
        if w % 2:
            sys.exit(f"SCREEN {screen} width {w} must be even (2 pixels per byte)")
        if optimal:
            hist = {}
            for y in range(h):
                for x in range(w):
                    c = to_msx9(px[x, y])
                    hist[c] = hist.get(c, 0) + 1
            pal3 = median_cut(hist, 16)
        else:
            pal3 = MSX2_PAL3
        pal8 = [tuple(round(c * 255 / 7) for c in rgb) for rgb in pal3]
        data = []
        for y in range(h):
            for x in range(0, w, 2):
                data.append((nearest(px[x, y], pal8) << 4) | nearest(px[x+1, y], pal8))
        pal = []
        for r, g, b in pal3:
            pal.append((r << 4) | b)   # byte0: 0RRR0BBB
            pal.append(g)              # byte1: 00000GGG
        return dict(w=w, h=h, bpp=4, screen=screen, data=data, pal=pal, pal8=pal8)
    if screen == 8:
        if optimal:
            sys.exit("--optimal applies to SCREEN 5/7 only (SCREEN 8 has a fixed palette)")
        data = [grb332(px[x, y]) for y in range(h) for x in range(w)]
        return dict(w=w, h=h, bpp=8, screen=screen, data=data, pal=None, pal8=None)
    sys.exit("screen must be 5, 7 or 8")

def roundtrip_ok(path, r):
    """Re-derive every VRAM byte from the source pixels and confirm it matches what
    was emitted -- a stable golden check (conversion is deterministic per pixel)."""
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    if r["bpp"] == 4:
        pal8 = r["pal8"]
        for y in range(h):
            for x in range(0, w, 2):
                want = (nearest(px[x, y], pal8) << 4) | nearest(px[x+1, y], pal8)
                if r["data"][y*(w//2) + x//2] != want:
                    return False
    else:
        for y in range(h):
            for x in range(w):
                if r["data"][y*w + x] != grb332(px[x, y]):
                    return False
    return True

def main():
    argv = [a for a in sys.argv[1:] if a != "--optimal"]
    optimal = "--optimal" in sys.argv[1:]
    if len(argv) < 3:
        sys.exit("usage: img2bitmap.py <in.png> <name> <screen 5|7|8> [out.h] [--optimal]")
    path, name, screen = argv[0], argv[1], int(argv[2])
    out = argv[3] if len(argv) > 3 else name + "_bitmap.h"
    r = convert(path, screen, optimal)
    assert roundtrip_ok(path, r), "round-trip mismatch (tool bug)"
    U = name.upper()
    lines = [
        f"// {out} -- GENERATED by img2bitmap.py from {path} (do not edit).",
        f"// MSX SCREEN {r['screen']} bitmap: {r['w']}x{r['h']}, {r['bpp']} bpp, "
        f"{len(r['data'])} VRAM bytes.",
        '#include "types.h"', "",
        carr(name + "_bitmap", r["data"]),
    ]
    if r["pal"] is not None:
        lines.append(carr(name + "_palette", r["pal"]))  # 16 colours, VDP format (RB,G)
    lines += [
        f"#define {U}_W {r['w']}",
        f"#define {U}_H {r['h']}",
        f"#define {U}_BPP {r['bpp']}",
        f"#define {U}_SCREEN {r['screen']}",
    ]
    open(out, "w").write("\n".join(lines) + "\n")
    extra = f" + {len(r['pal'])}B palette" if r["pal"] else ""
    palkind = " (optimal)" if optimal else (" (MSX2 default)" if r["pal"] else "")
    print(f"wrote {out}: SCREEN {r['screen']} {r['w']}x{r['h']} {r['bpp']}bpp, "
          f"{len(r['data'])}B bitmap{extra}{palkind}, round-trip OK")

if __name__ == "__main__":
    main()
