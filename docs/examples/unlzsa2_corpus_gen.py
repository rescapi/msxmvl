#!/usr/bin/env python3
# Generate unlzsa2_corpus.h: a set of LZSA2 round-trip test vectors packed by the
# REFERENCE encoder (Emmanuel Marty's `lzsa`, raw LZSA2 mode `-f2 -r` -- the format
# our UnLZSA2 decodes). This makes the encoder the ground truth: the example
# decompresses each vector on-target and compares byte-exact to the original.
#
# LOCAL-ONLY regeneration tool. The generated header is committed; CI never runs
# this. It builds the `lzsa` encoder from source (github.com/emmanuel-marty/lzsa)
# if no `lzsa` is on PATH, cloning it if a local checkout is not already present.
import os, subprocess, sys, tempfile, shutil, random

HERE = os.path.dirname(os.path.abspath(__file__))
LZSA_SRC = os.environ.get("LZSA_SRC", "/home/remco/tools/lzsa")   # local checkout
LZSA_URL = "https://github.com/emmanuel-marty/lzsa"

def find_encoder():
    exe = shutil.which("lzsa")
    if exe:
        return exe
    # build from a local checkout of the reference source, cloning it if needed.
    if not os.path.isdir(LZSA_SRC):
        subprocess.run(["git", "clone", "--depth", "1", LZSA_URL, LZSA_SRC], check=True)
    built = os.path.join(LZSA_SRC, "lzsa")
    if not os.path.exists(built):
        # clang is the Makefile default; gcc works too and is what we have.
        subprocess.run(["make", "-C", LZSA_SRC, "CC=gcc"], check=True)
    return built

def pack(enc, raw):
    with tempfile.TemporaryDirectory() as d:
        i = os.path.join(d, "i.bin"); o = os.path.join(d, "o.lz2")
        open(i, "wb").write(raw)
        # -f2 = LZSA2 format, -r = raw (frame-less) block with an embedded EOD marker.
        subprocess.run([enc, "-f2", "-r", i, o], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        packed = open(o, "rb").read()
        # sanity: the reference decoder must round-trip its own output.
        back = os.path.join(d, "b.bin")
        subprocess.run([enc, "-d", "-f2", "-r", o, back], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert open(back, "rb").read() == raw, "reference self-roundtrip failed"
        return packed

def carr(name, data):
    body = ",".join(str(b) for b in data)
    return f"static const u8 {name}[{len(data)}] = {{{body}}};\n"

def main():
    enc = find_encoder()

    # Realistic ~2 KB block: reuse the committed benchmark corpus if present, else a
    # deterministic structured fill. Both the packed AND the raw copy of every vector
    # live in the example's 16 KB cartridge ROM, so total footprint is kept modest.
    big_path = os.path.join(HERE, "..", "..", "benchmarks",
                            "zx0-decompress", "data", "raw.bin")
    if os.path.exists(big_path):
        big = open(big_path, "rb").read()[:2048]
    else:
        rnd = random.Random(1234)
        big = bytes((rnd.randrange(256) if i % 7 else 0x20) for i in range(2048))

    # Deterministic random fills (shared RNG so a run is genuinely varied).
    def gen_pure(seed, n):
        r = random.Random(seed)
        return bytes(r.randrange(256) for _ in range(n))

    def gen_struct(seed, n):
        r = random.Random(seed)
        out = bytearray()
        while len(out) < n:
            k = r.randrange(4)
            if k == 0:                                  # run of one byte (RLE / overlap)
                out += bytes([r.randrange(256)]) * (1 + r.randrange(60))
            elif k == 1 and len(out) > 2:               # back-reference (copy earlier)
                off = 1 + r.randrange(min(len(out), 4000))
                ln = 1 + r.randrange(80)
                base = len(out) - off
                for i in range(ln):
                    out.append(out[base + (i % off)])
            elif k == 2:                                # literal burst
                out += bytes(r.randrange(256) for _ in range(1 + r.randrange(30)))
            else:                                       # repeated small motif
                motif = bytes(r.randrange(256) for _ in range(1 + r.randrange(4)))
                out += motif * (1 + r.randrange(20))
        return bytes(out[:n])

    tiles = bytes(([0x00,0x18,0x3C,0x7E,0x7E,0x3C,0x18,0x00] * 8) +
                  ([0xFF,0x81,0x81,0x81,0x81,0x81,0x81,0xFF] * 8))

    # Corner cases (kept exactly), then fuzz vectors with deterministic seeds.
    vectors = [
        ("one",   bytes([0x42])),                       # single byte
        ("two",   bytes([0xAA, 0x55])),                 # two bytes
        ("same",  bytes([0xE3]) * 512),                 # all-same run -> 16-bit match length
        ("incmp", gen_pure(1, 400)),                    # incompressible -> 16-bit literal length
        ("tiles", tiles),                               # structured tile data
        ("big",   big),                                 # realistic ~2 KB, many tokens
    ]
    pure_sizes   = [3, 8, 32, 128, 255]                 # small, literal-heavy
    struct_sizes = [64, 256, 512, 1024]                 # matches, overlaps, long runs
    for i, sz in enumerate(pure_sizes):
        vectors.append((f"pr{i:02d}", gen_pure(1000 + i, sz)))
    for i, sz in enumerate(struct_sizes):
        vectors.append((f"st{i:02d}", gen_struct(2000 + i, sz)))

    lines = [
        "// unlzsa2_corpus.h -- GENERATED by unlzsa2_corpus_gen.py (do not edit).",
        "// LZSA2 round-trip vectors packed by the reference encoder (`lzsa -f2 -r`);",
        "// the oracle for unlzsa2_01_roundtrip.c. Each entry: packed stream + original",
        "// + length.",
        "#ifndef MVCZ80_UNLZSA2_CORPUS_H",
        "#define MVCZ80_UNLZSA2_CORPUS_H",
        "#include \"types.h\"",
        "",
    ]
    entries = []
    maxraw = 0
    for name, raw in vectors:
        packed = pack(enc, raw)
        maxraw = max(maxraw, len(raw))
        lines.append(carr(f"g_lz2_pk_{name}", packed))
        lines.append(carr(f"g_lz2_raw_{name}", raw))
        entries.append((name, len(raw)))
    lines.append(f"#define LZSA2_CORPUS_MAXRAW {maxraw}")
    lines.append(f"#define LZSA2_CORPUS_COUNT  {len(entries)}")
    lines.append("typedef struct { const u8* packed; const u8* raw; u16 len; } LZSA2Vec;")
    lines.append("static const LZSA2Vec g_lz2_corpus[LZSA2_CORPUS_COUNT] = {")
    for name, ln in entries:
        lines.append(f"\t{{ g_lz2_pk_{name}, g_lz2_raw_{name}, {ln} }},")
    lines.append("};")
    lines.append("#endif")
    open(os.path.join(HERE, "unlzsa2_corpus.h"), "w").write("\n".join(lines) + "\n")
    print("wrote unlzsa2_corpus.h:", ", ".join(f"{n}={l}B" for n, l in entries))

if __name__ == "__main__":
    main()
