#!/usr/bin/env python3
# zx0pack.py -- shared ZX0 author-time packing for the mvcz80 asset converters
# (img2tiles / img2bitmap / img2sprites, with --zx0).
#
# IMPORTANT: compression runs on the HOST, at build time, once -- using Einar Saukas's
# reference `zx0` encoder (the exact ZX0 format lib/ext/unzx0's UnZX0 decodes). The MSX
# only ever DEcompresses at load time (UnZX0 / UnZX0_fast / UnZX0_VRAM). Nothing here
# runs on the MSX, and none of it runs unless --zx0 is passed.
#
# The encoder is `zx0`/`z88dk-zx0` on PATH, else built from the reference source at
# $ZX0_SRC (default z88dk's src/zx0). If neither is available, `available()` is False
# and callers should fall back to raw output (CI has no encoder, by design).
import os, sys, subprocess, tempfile, shutil

ZX0_SRC = os.environ.get("ZX0_SRC", "/home/remco/tools/z88dk/src/zx0")
_ENC_CACHE = os.path.join(tempfile.gettempdir(), "mvcz80_zx0ref_enc")

def find_encoder():
    exe = shutil.which("zx0") or shutil.which("z88dk-zx0")
    if exe:
        return exe
    if os.path.exists(_ENC_CACHE):
        return _ENC_CACHE
    srcs = [os.path.join(ZX0_SRC, f) for f in ("zx0.c", "compress.c", "optimize.c", "memory.c")]
    if all(os.path.exists(s) for s in srcs):
        subprocess.run(["gcc", "-O2", "-o", _ENC_CACHE, *srcs], check=True)
        return _ENC_CACHE
    return None

def available():
    return find_encoder() is not None

def pack(raw):
    """Compress bytes with the reference ZX0 encoder; return the packed bytes."""
    enc = find_encoder()
    if not enc:
        raise RuntimeError("no ZX0 encoder (install `zx0`, or set ZX0_SRC to the reference source)")
    with tempfile.TemporaryDirectory() as d:
        i = os.path.join(d, "i.bin"); o = os.path.join(d, "o.zx0")
        with open(i, "wb") as f:
            f.write(bytes(raw))
        subprocess.run([enc, "-f", i, o], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        with open(o, "rb") as f:
            return f.read()

def emit(name, suffix, data, use_zx0):
    """C text for one u8 array. If use_zx0: emit `<name>_<suffix>_zx0[]` (packed) plus
    `#define <NAME>_<SUFFIX>_LEN <unpacked>`. Else: raw `<name>_<suffix>[]` -- byte-for-
    byte identical to the plain emitter. Returns (text, stored_len)."""
    if use_zx0:
        p = pack(data)
        body = ",".join(str(b) for b in p)
        text = (f"const u8 {name}_{suffix}_zx0[{len(p)}]={{{body}}};\n"
                f"#define {name.upper()}_{suffix.upper()}_LEN {len(data)}\n")
        return text, len(p)
    body = ",".join(str(b) for b in data)
    return f"const u8 {name}_{suffix}[{len(data)}]={{{body}}};\n", len(data)
