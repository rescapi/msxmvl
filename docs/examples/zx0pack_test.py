#!/usr/bin/env python3
# zx0pack_test.py -- round-trip test for the asset converters' --zx0 packing.
#
# LOCAL-ONLY: ZX0 compression is an author-time step needing the reference encoder (and,
# to verify, the reference decoder). CI has neither, so this SKIPS there (exit 0) -- the
# packed output's correctness rests on two oracles that DO run in CI: the raw arrays are
# byte-exact (the per-format golden tests) and the ZX0 codec is byte-exact (unzx0's
# on-target corpus test). Here we additionally prove, on the host, that each tool's --zx0
# output decompresses (reference DZX0) back to exactly the raw array it would have emitted.
import os, subprocess, sys, tempfile, re, shutil
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "..", "tools")
sys.path.insert(0, TOOLS)
import zx0pack

def find_decoder():
    exe = shutil.which("dzx0") or shutil.which("z88dk-dzx0")
    if exe:
        return exe
    src = os.environ.get("ZX0_SRC", "/home/remco/tools/z88dk/src/zx0")
    out = os.path.join(tempfile.gettempdir(), "mvcz80_zx0ref_dec")
    if os.path.exists(out):
        return out
    srcs = [os.path.join(src, f) for f in ("dzx0.c", "memory.c")]
    if all(os.path.exists(s) for s in srcs):
        subprocess.run(["gcc", "-O2", "-o", out, *srcs], check=True)
        return out
    return None

def unpack(dec, packed):
    with tempfile.TemporaryDirectory() as d:
        z = os.path.join(d, "a.zx0"); o = os.path.join(d, "a.raw")
        open(z, "wb").write(bytes(packed))
        subprocess.run([dec, "-f", z, o], check=True, capture_output=True)
        return list(open(o, "rb").read())

def tool(name, *args):
    r = subprocess.run([sys.executable, os.path.join(TOOLS, name), *args],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f"FAIL: {name} {' '.join(args)}\n", r.stderr); sys.exit(1)
    outp = next(a for a in args if a.endswith(".h"))   # the output header path
    return open(outp).read()

def arr(text, name):
    m = re.search(r"\b" + re.escape(name) + r"\[\d+\]=\{([^}]*)\}", text)
    return [int(x) for x in m.group(1).split(",")] if m else None

def define(text, name):
    m = re.search(r"#define\s+" + re.escape(name) + r"\s+(\d+)", text)
    return int(m.group(1)) if m else None

def check(dec, raw_text, zx0_text, base, suffix):
    """The <base>_<suffix> array in the raw output must equal the decompressed
    <base>_<suffix>_zx0 array in the --zx0 output; and the packed form must be smaller."""
    rawarr = arr(raw_text, f"{base}_{suffix}")
    packed = arr(zx0_text, f"{base}_{suffix}_zx0")
    ln = define(zx0_text, f"{base.upper()}_{suffix.upper()}_LEN")
    if rawarr is None or packed is None or ln is None:
        print(f"FAIL: {base}_{suffix} arrays/define missing (packed={packed is not None}, len={ln})"); sys.exit(1)
    if ln != len(rawarr):
        print(f"FAIL: {base}_{suffix} LEN {ln} != raw {len(rawarr)}"); sys.exit(1)
    if len(packed) >= len(rawarr):
        print(f"FAIL: {base}_{suffix} not smaller ({len(packed)} >= {len(rawarr)})"); sys.exit(1)
    if unpack(dec, packed) != rawarr:
        print(f"FAIL: {base}_{suffix} does not round-trip through DZX0"); sys.exit(1)

def main():
    dec = find_decoder()
    if not zx0pack.available() or dec is None:
        print("zx0pack_test SKIP: no ZX0 encoder/decoder available (author-time only)")
        sys.exit(0)

    with tempfile.TemporaryDirectory() as d:
        # a compressible image: mostly one colour with a little structure.
        def make(w, h, rgb=(33, 200, 66)):
            im = Image.new("RGB", (w, h), rgb); px = im.load()
            for i in range(0, w, 8):
                px[i, 0] = (255, 255, 255)
            p = os.path.join(d, f"i{w}x{h}.png"); im.save(p); return p

        # img2bitmap SCREEN 5: bitmap packed, palette stays raw.
        p = make(64, 16)
        rawb = tool("img2bitmap.py", p, "b5", "5", os.path.join(d, "b5.h"))
        zxb  = tool("img2bitmap.py", p, "b5", "5", os.path.join(d, "b5z.h"), "--zx0")
        check(dec, rawb, zxb, "b5", "bitmap")
        if arr(zxb, "b5_palette") is None or arr(zxb, "b5_bitmap") is not None:
            print("FAIL: --zx0 should pack the bitmap but keep the palette raw\n", zxb); sys.exit(1)

        # img2sprites: pattern packed, colour stays raw.
        raws = tool("img2sprites.py", p, "sp", "16", os.path.join(d, "sp.h"))
        zxs  = tool("img2sprites.py", p, "sp", "16", os.path.join(d, "spz.h"), "--zx0")
        check(dec, raws, zxs, "sp", "pattern")
        if arr(zxs, "sp_colour") is None or arr(zxs, "sp_pattern") is not None:
            print("FAIL: --zx0 should pack the pattern but keep the colour raw\n", zxs); sys.exit(1)

        # img2tiles: pattern, colour AND nametable all packed.
        rawt = tool("img2tiles.py", p, "tl", os.path.join(d, "tl.h"))
        zxt  = tool("img2tiles.py", p, "tl", os.path.join(d, "tlz.h"), "--zx0")
        for suffix in ("pattern", "colour", "nametable"):
            check(dec, rawt, zxt, "tl", suffix)

        print("zx0pack_test PASS: img2bitmap/img2sprites/img2tiles --zx0 packs the bulk "
              "arrays, keeps metadata raw, and each round-trips through the reference DZX0")

if __name__ == "__main__":
    main()
