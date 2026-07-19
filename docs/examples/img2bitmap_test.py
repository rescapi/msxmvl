#!/usr/bin/env python3
# img2bitmap_test.py -- golden/drift test for the asset pipeline (tools/img2bitmap.py).
# Self-contained: generates deterministic images, runs the converter for SCREEN 5
# (4bpp + palette) and SCREEN 8 (8bpp GRB332), and asserts (1) the built-in
# round-trip holds, (2) the packed VRAM byte count is right, (3) the emitted arrays
# are byte-stable (golden). No committed binary asset needed.
import os, subprocess, sys, tempfile, hashlib
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "img2bitmap.py")

# MSX2 default palette expanded to 8-bit RGB (matches img2bitmap.PAL8) so a SCREEN 5
# image built from these colours quantises exactly.
PAL3 = [(0,0,0),(0,0,0),(1,6,1),(3,7,3),(1,1,7),(2,3,7),(5,1,1),(2,6,7),
        (7,1,1),(7,3,3),(6,6,1),(6,6,4),(1,4,1),(6,2,5),(5,5,5),(7,7,7)]
PAL8 = [tuple(round(c*255/7) for c in rgb) for rgb in PAL3]

def make_s5(path):
    # 16x4: each pixel cycles through palette entries 2..15 -> exact 4bpp packing.
    im = Image.new("RGB", (16, 4)); px = im.load()
    for y in range(4):
        for x in range(16):
            px[x, y] = PAL8[2 + ((x + y) % 14)]
    im.save(path)

def make_s8(path):
    # 8x4 gradient exercising all three GRB332 fields.
    im = Image.new("RGB", (8, 4)); px = im.load()
    for y in range(4):
        for x in range(8):
            px[x, y] = (x * 32, y * 64, x * 16)
    im.save(path)

def make_opt(path):
    # 16x8 gradient with many colours -> exercises the median-cut optimal palette.
    im = Image.new("RGB", (16, 8)); px = im.load()
    for y in range(8):
        for x in range(16):
            px[x, y] = ((x*16) % 256, (y*32) % 256, (x*8 + y*8) % 256)
    im.save(path)

def golden_of(text):
    return hashlib.sha256("\n".join(l for l in text.splitlines()
                                    if l.startswith("const u8")).encode()).hexdigest()[:16]

def run(png, name, screen, out, *extra):
    r = subprocess.run([sys.executable, TOOL, png, name, str(screen), out, *extra],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f"FAIL: tool errored (SCREEN {screen})\n", r.stderr); sys.exit(1)
    if "round-trip OK" not in r.stdout:
        print(f"FAIL: no round-trip confirmation (SCREEN {screen})\n", r.stdout); sys.exit(1)
    return open(out).read()

def main():
    with tempfile.TemporaryDirectory() as d:
        # SCREEN 5: 16x4 4bpp -> 32 bitmap bytes + 32 palette bytes
        p5 = os.path.join(d, "a.png"); make_s5(p5)
        t5 = run(p5, "s5", 5, os.path.join(d, "a.h"))
        if "S5_BITMAP" not in t5.upper() or "s5_bitmap[32]" not in t5:
            print("FAIL: SCREEN 5 bitmap size wrong\n", t5); sys.exit(1)
        if "s5_palette[32]" not in t5:
            print("FAIL: SCREEN 5 palette missing\n", t5); sys.exit(1)
        g5 = golden_of(t5); G5 = "d1b196c76741d035"
        if g5 != G5:
            print(f"FAIL: SCREEN 5 golden drift (got {g5}, want {G5})"); sys.exit(1)

        # SCREEN 8: 8x4 8bpp -> 32 bitmap bytes, no palette
        p8 = os.path.join(d, "b.png"); make_s8(p8)
        t8 = run(p8, "s8", 8, os.path.join(d, "b.h"))
        if "s8_bitmap[32]" not in t8:
            print("FAIL: SCREEN 8 bitmap size wrong\n", t8); sys.exit(1)
        if "s8_palette" in t8:
            print("FAIL: SCREEN 8 must not emit a palette\n", t8); sys.exit(1)
        g8 = golden_of(t8); G8 = "950b3eb113f03361"
        if g8 != G8:
            print(f"FAIL: SCREEN 8 golden drift (got {g8}, want {G8})"); sys.exit(1)

        # SCREEN 5 --optimal (D4 median-cut palette): 16x8 gradient. The optimal
        # palette must differ from the fixed MSX2 default and be deterministic.
        po = os.path.join(d, "c.png"); make_opt(po)
        to = run(po, "opt", 5, os.path.join(d, "c.h"), "--optimal")
        if "opt_bitmap[64]" not in to or "opt_palette[32]" not in to:
            print("FAIL: optimal-mode shapes wrong\n", to); sys.exit(1)
        # a non-default palette adapts to the image -> its bytes must not be all-default
        tdef = run(po, "def", 5, os.path.join(d, "c2.h"))
        if golden_of(to) == golden_of(tdef):
            print("FAIL: --optimal produced the default palette output"); sys.exit(1)
        go = golden_of(to); GO = "61519f17ada199e3"
        if go != GO:
            print(f"FAIL: optimal golden drift (got {go}, want {GO})"); sys.exit(1)

        print("img2bitmap_test PASS: SCREEN 5 (4bpp+palette) & SCREEN 8 (GRB332) & "
              "--optimal median-cut palette -- round-trip OK, arrays byte-stable")

if __name__ == "__main__":
    main()
