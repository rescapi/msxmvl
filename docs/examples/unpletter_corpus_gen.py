#!/usr/bin/env python3
# Generate unpletter_corpus.h: a set of Pletter round-trip test vectors packed by
# the REFERENCE encoder (XL2S Entertainment's `pletter`, default mode -- WITHOUT
# -s, so no stored length; the format our UnPletter decodes). This makes the
# encoder the ground truth: the example decompresses each vector on-target and
# compares byte-exact to the original.
#
# LOCAL-ONLY regeneration tool. The generated header is committed; CI never runs
# this. It builds the encoder from MSXgl's vendored Pletter source if no `pletter`
# is on PATH (same code as github.com/nanochess/Pletter).
import os, subprocess, sys, tempfile, shutil, random

HERE = os.path.dirname(os.path.abspath(__file__))
# Reference Pletter encoder: github.com/nanochess/Pletter (pletter.cpp).
PLET_REPO = "https://github.com/nanochess/Pletter"

def find_encoder():
    exe = shutil.which("pletter")
    if exe and os.access(exe, os.X_OK):
        return exe
    # build from the public reference source (cloned into a temp dir)
    d = os.path.join(tempfile.gettempdir(), "pletter_ref_src")
    if not os.path.isdir(d):
        subprocess.run(["git", "clone", "--depth", "1", PLET_REPO, d], check=True)
    out = os.path.join(tempfile.gettempdir(), "pletter_ref_gen")
    subprocess.run(["g++", "-O2", "-w", "-o", out, os.path.join(d, "pletter.cpp")], check=True)
    return out

def pack(enc, raw):
    with tempfile.TemporaryDirectory() as d:
        i = os.path.join(d, "i.bin"); o = os.path.join(d, "o.plet5")
        open(i, "wb").write(raw)
        # default mode (no -s): the length is NOT stored; UnPletter stops on the
        # stream's own end marker.
        subprocess.run([enc, i, o], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return open(o, "rb").read()

def carr(name, data):
    body = ",".join(str(b) for b in data)
    return f"static const u8 {name}[{len(data)}] = {{{body}}};\n"

def main():
    enc = find_encoder()
    # Realistic 4 KB block: reuse the committed benchmark corpus if present,
    # else a deterministic pseudo-random-but-structured fill.
    big_path = os.path.join(HERE, "..", "..", "benchmarks",
                            "pletter-decompress", "data", "raw.bin")
    if os.path.exists(big_path):
        big = open(big_path, "rb").read()
    else:
        rnd = random.Random(1234)
        big = bytes((rnd.randrange(256) if i % 7 else 0x20) for i in range(4096))

    # Corner cases (kept exactly).
    rnd = random.Random(99)
    vectors = [
        ("one",   bytes([0x42])),                       # single byte
        ("two",   bytes([0xAA, 0x55])),                 # two bytes
        ("same",  bytes([0xE3]) * 256),                 # all-same (long run)
        ("incmp", bytes(rnd.randrange(256) for _ in range(200))),  # incompressible
        ("tiles", bytes(([0x00,0x18,0x3C,0x7E,0x7E,0x3C,0x18,0x00] * 8) +
                        ([0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF] * 8)),),  # structured
        ("big",   big),                                 # realistic, many tokens
    ]

    # Fuzz-grade random vectors: deterministic seeds so the committed header is
    # stable, but a wide spread of sizes and two flavours -- pure-random (literal-
    # heavy, exercises the incompressible path) and "structured" (runs + repeats +
    # literals, exercises the LZ matcher and long / far back-references). The size
    # spread pushes the encoder through several offset modes (q=1..6).
    def gen_pure(seed, n):
        r = random.Random(seed)
        return bytes(r.randrange(256) for _ in range(n))

    def gen_struct(seed, n):
        r = random.Random(seed)
        out = bytearray()
        while len(out) < n:
            k = r.randrange(4)
            if k == 0:                                  # run of one byte
                out += bytes([r.randrange(256)]) * (1 + r.randrange(40))
            elif k == 1 and len(out) > 2:               # back-reference (copy earlier)
                off = 1 + r.randrange(min(len(out), 300))
                ln = 1 + r.randrange(40)
                base = len(out) - off
                for i in range(ln):
                    out.append(out[base + (i % off)])
            elif k == 2:                                # short literal burst
                out += bytes(r.randrange(256) for _ in range(1 + r.randrange(8)))
            else:                                       # repeated small motif
                motif = bytes(r.randrange(256) for _ in range(1 + r.randrange(4)))
                out += motif * (1 + r.randrange(20))
        return bytes(out[:n])

    # 6 pure-random (small, varied) + 6 structured (larger, packable) = 12 fuzz
    # vectors, so 18 total including the corners above. Sizes are capped so the
    # whole committed corpus (packed + raw arrays) still fits a 16 KB test ROM;
    # the 4 KB "big" vector already drives the wide-offset modes.
    pure_sizes   = [3, 5, 13, 34, 89, 233]
    struct_sizes = [40, 100, 200, 384, 700, 1200]
    for i, sz in enumerate(pure_sizes):
        vectors.append((f"pr{i:02d}", gen_pure(1000 + i, sz)))
    for i, sz in enumerate(struct_sizes):
        vectors.append((f"st{i:02d}", gen_struct(2000 + i, sz)))

    lines = [
        "// unpletter_corpus.h -- GENERATED by unpletter_corpus_gen.py (do not edit).",
        "// Pletter v0.5c round-trip vectors packed by the reference encoder; the",
        "// oracle for unpletter_01_roundtrip.c. Each entry: packed stream + original",
        "// + length.",
        "#ifndef MVCZ80_UNPLETTER_CORPUS_H",
        "#define MVCZ80_UNPLETTER_CORPUS_H",
        "#include \"types.h\"",
        "",
    ]
    entries = []
    maxraw = 0
    for name, raw in vectors:
        packed = pack(enc, raw)
        maxraw = max(maxraw, len(raw))
        lines.append(carr(f"g_plt_pk_{name}", packed))
        lines.append(carr(f"g_plt_raw_{name}", raw))
        entries.append((name, len(raw)))
    lines.append(f"#define PLT_CORPUS_MAXRAW {maxraw}")
    lines.append(f"#define PLT_CORPUS_COUNT  {len(entries)}")
    lines.append("typedef struct { const u8* packed; const u8* raw; u16 len; } PltVec;")
    lines.append("static const PltVec g_plt_corpus[PLT_CORPUS_COUNT] = {")
    for name, ln in entries:
        lines.append(f"\t{{ g_plt_pk_{name}, g_plt_raw_{name}, {ln} }},")
    lines.append("};")
    lines.append("#endif")
    open(os.path.join(HERE, "unpletter_corpus.h"), "w").write("\n".join(lines) + "\n")
    print("wrote unpletter_corpus.h:", ", ".join(f"{n}={l}B" for n, l in entries))

if __name__ == "__main__":
    main()
