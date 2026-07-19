#!/usr/bin/env python3
# img2sprites_test.py -- golden/drift test for the sprite converter (tools/img2sprites.py).
# Self-contained: builds deterministic sprite sheets, runs the converter, and asserts
# EXACT pattern bytes (including the MSX 16x16 quadrant order TL/BL/TR/BR), colour
# extraction, the black/alpha = transparent rule, and byte-stable golden hashes.
import os, subprocess, sys, tempfile, hashlib, re
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "img2sprites.py")

# A few exact TMS9918 palette colours so colour extraction is deterministic.
WHITE = (255, 255, 255)
GREEN = (33, 200, 66)     # palette index 2
BLACK = (0, 0, 0)         # transparent (off)

def run(d, png, name, size, *extra):
    out = os.path.join(d, name + ".h")
    r = subprocess.run([sys.executable, TOOL, png, name, str(size), out, *extra],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f"FAIL: tool errored (size {size})\n", r.stderr); sys.exit(1)
    return open(out).read()

def arr(text, name):
    m = re.search(r"\b" + re.escape(name) + r"\[\d+\]=\{([^}]*)\}", text)
    return [int(x) for x in m.group(1).split(",")]

def golden(text):
    return hashlib.sha256("\n".join(l for l in text.splitlines()
                                    if l.startswith("const u8")).encode()).hexdigest()[:16]

def make(path, w, h, mode, paint):
    im = Image.new(mode, (w, h), (0, 0, 0, 0) if mode == "RGBA" else (0, 0, 0))
    px = im.load()
    paint(px)
    im.save(path)

def test_8x8(d):
    # 16x8 = two 8x8 sprites. Sprite 0: top row white. Sprite 1: left column green.
    def paint(px):
        for x in range(8):
            px[x, 0] = WHITE                 # sprite 0, row 0 fully lit
        for y in range(8):
            px[8, y] = GREEN                 # sprite 1, column 0 lit
    p = os.path.join(d, "s8.png"); make(p, 16, 8, "RGB", paint)
    t = run(d, p, "s8", 8)
    pat, col = arr(t, "s8_pattern"), arr(t, "s8_colour")
    if len(pat) != 16:
        print("FAIL: 8x8 pattern length", len(pat)); sys.exit(1)
    if pat[0:8] != [0xFF, 0, 0, 0, 0, 0, 0, 0]:
        print("FAIL: 8x8 sprite0 top-row pattern", pat[0:8]); sys.exit(1)
    if pat[8:16] != [0x80] * 8:
        print("FAIL: 8x8 sprite1 left-col pattern", pat[8:16]); sys.exit(1)
    if col != [15, 2]:
        print("FAIL: 8x8 colours (want white=15, green=2)", col); sys.exit(1)
    if "S8_COUNT 2" not in t or "S8_SIZE 8" not in t:
        print("FAIL: 8x8 defines\n", t); sys.exit(1)
    return t

def test_16x16(d):
    # One 16x16 sprite with a single lit pixel in each quadrant at a DIFFERENT row, so
    # the emitted bytes pin down the exact MSX quadrant order (TL, BL, TR, BR) and the
    # row order within each quadrant.
    def paint(px):
        px[0, 0]  = WHITE      # TL quadrant, local (col0,row0) -> TL byte[0] bit7
        px[0, 9]  = WHITE      # BL quadrant, local (col0,row1) -> BL byte[1] bit7
        px[8, 2]  = WHITE      # TR quadrant, local (col0,row2) -> TR byte[2] bit7
        px[8, 11] = WHITE      # BR quadrant, local (col0,row3) -> BR byte[3] bit7
    p = os.path.join(d, "s16.png"); make(p, 16, 16, "RGB", paint)
    t = run(d, p, "s16", 16)
    pat = arr(t, "s16_pattern")
    if len(pat) != 32:
        print("FAIL: 16x16 pattern length", len(pat)); sys.exit(1)
    want = [0] * 32
    want[0], want[9], want[18], want[27] = 0x80, 0x80, 0x80, 0x80   # TL,BL,TR,BR
    if pat != want:
        print("FAIL: 16x16 quadrant order/pattern\n got ", pat, "\n want", want); sys.exit(1)
    if "S16_COUNT 1" not in t or "S16_SIZE 16" not in t:
        print("FAIL: 16x16 defines\n", t); sys.exit(1)
    return t

def test_transparent(d):
    # Fully-white 8x8 but alpha 0 -> all pixels transparent -> empty pattern, colour 1.
    def paint(px):
        for y in range(8):
            for x in range(8):
                px[x, y] = (255, 255, 255, 0)
    p = os.path.join(d, "tr.png"); make(p, 8, 8, "RGBA", paint)
    t = run(d, p, "tr", 8)
    if arr(t, "tr_pattern") != [0] * 8:
        print("FAIL: alpha-transparent should be all-off", arr(t, "tr_pattern")); sys.exit(1)
    if arr(t, "tr_colour") != [1]:
        print("FAIL: empty sprite colour should default to 1", arr(t, "tr_colour")); sys.exit(1)

def main():
    with tempfile.TemporaryDirectory() as d:
        t8 = test_8x8(d)
        t16 = test_16x16(d)
        test_transparent(d)
        for label, text, want in (("8x8", t8, "bc453d1acfbc7564"), ("16x16", t16, "9aa7cbda911ad0e0")):
            g = golden(text)
            if g != want:
                print(f"FAIL: {label} golden drift (got {g}, want {want})"); sys.exit(1)
        print("img2sprites_test PASS: 8x8 + 16x16 (quadrant order) patterns, colour "
              "extraction, alpha/black transparency, arrays byte-stable")

if __name__ == "__main__":
    main()
