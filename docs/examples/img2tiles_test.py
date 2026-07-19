#!/usr/bin/env python3
# img2tiles_test.py -- golden/drift test for the asset pipeline (tools/img2tiles.py).
# Self-contained: generates a deterministic MSX-SCREEN-2-legal image, runs the
# converter, and asserts (1) the built-in round-trip holds, (2) de-duplication works,
# (3) the emitted arrays are byte-stable (golden). No committed binary asset needed.
import os, subprocess, sys, tempfile, hashlib
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "img2tiles.py")

# Deterministic 32x16 image (4x2 tiles). Each 8-pixel row uses <=2 palette colours
# (the SCREEN 2 rule) so conversion is exact; adjacent tiles repeat to exercise dedup.
PAL = [(0, 0, 0), (33, 200, 66), (84, 85, 237), (255, 255, 255)]

def make_image(path):
    im = Image.new("RGB", (32, 16))
    px = im.load()
    for y in range(16):
        for x in range(32):
            tile = x // 8
            a = PAL[0 if (y % 8) < 4 else 1]
            b = PAL[2 if tile % 2 else 3]
            px[x, y] = a if ((x + y) & 1) else b
    im.save(path)

def main():
    with tempfile.TemporaryDirectory() as d:
        png = os.path.join(d, "t.png")
        out = os.path.join(d, "t.h")
        make_image(png)
        r = subprocess.run([sys.executable, TOOL, png, "asset_test", out],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("FAIL: tool errored\n", r.stderr); sys.exit(1)
        if "round-trip OK" not in r.stdout:
            print("FAIL: no round-trip confirmation\n", r.stdout); sys.exit(1)
        text = open(out).read()
        # dedup: 4x2 = 8 tiles must collapse to 2 unique
        if "ASSET_TEST_NTILES 2" not in text:
            print("FAIL: expected 2 unique tiles\n", text); sys.exit(1)
        # golden: emitted arrays are byte-stable
        h = hashlib.sha256("\n".join(l for l in text.splitlines()
                                     if l.startswith("const u8")).encode()).hexdigest()[:16]
        GOLDEN = "9805c1e2f5587e27"   # bytes of pattern/colour/nametable arrays
        if h != GOLDEN:
            print(f"FAIL: golden drift (got {h}, want {GOLDEN})"); sys.exit(1)
        print("img2tiles_test PASS: round-trip OK, dedup 8->2, arrays byte-stable")

if __name__ == "__main__":
    main()
