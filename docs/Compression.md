# Compression (`compress`)

Unpack **RLEp** (run-length encoded, packed) data at runtime. Cart ROM space is the scarcest
resource on MSX, so tile maps, screens and pattern tables are usually stored compressed and
expanded into RAM or VRAM on demand. Link `memory.c compress.c`, include `compress.h`.

`compress` is a *decompressor only* — you pack the data on the host side, at build time.

## The RLEp format

The stream is a sequence of chunks. Each chunk starts with one header byte:

```
  7   6   5   4   3   2   1   0
 T1  T0  C5  C4  C3  C2  C1  C0
 └─┬──┘  └──────────┬─────────┘
  type          count - 1        (so 0 means "1", 63 means "64")
```

| Type | Meaning | Bytes that follow |
|------|---------|-------------------|
| 0 | run of the **default value** | none |
| 1 | run of one given byte | 1 |
| 2 | run of a given **byte pair** | 2 |
| 3 | **raw** (uncompressed) data | *count* |

With the default configuration (`COMPRESS_USE_RLEP_DEFAULT`), the very first byte of the stream
is the default value used by Type-0 runs, and a `0x00` header terminates the stream.

Type 0 is what makes this format pay: a screen is mostly one repeated tile, and each run of up to
64 of them costs a **single byte**.

## API

```c
u16 RLEp_UnpackToRAM(const u8* src, u8* dst);   // returns the unpacked size
```

Set `COMPRESS_USE_RLEP_FIXSIZE` to `1` before including the header to get a
`RLEp_UnpackToRAM(src, dst, size)` variant that stops after `size` bytes instead of at the
terminator.

## Unpacking a level's tilemap

A level's tilemap ships packed in ROM and is expanded into a RAM buffer when the level loads.
`load_tilemap` is one call that returns how many tiles it produced. Keep the packed data `const` so
it stays in ROM — this little stream exercises all four chunk types.

```c
// level_load.c — unpack a compressed tilemap into RAM when a level loads.
#include "compress.h"

// A tiny packed tilemap: [default tile] then chunks, terminated by 0x00.
static const u8 g_PackedMap[] = {
	0xAA,                    // default tile for Type-0 runs
	0x02,                    // T0 x3  -> AA AA AA
	0x43, 0xBB,              // T1 x4  -> BB BB BB BB
	0x81, 0xCC, 0xDD,        // T2 x2  -> CC DD CC DD
	0xC2, 0x11, 0x22, 0x33,  // T3 x3  -> 11 22 33 (raw)
	0x00,                    // terminator
};
static u8 g_TileMap[16];     // where the level's tiles land in RAM

// Unpack the level's tilemap into RAM; returns the tile count.
u16 load_tilemap(void)
{
	return RLEp_UnpackToRAM(g_PackedMap, g_TileMap);
}
```

`load_tilemap()` returns `14`: the 12 packed bytes expand to `AA AA AA BB BB BB BB CC DD CC DD 11 22
33`. The default-run tile is `0xAA`, the 1-byte run gives `0xBB`, the pair run `0xCC`, and the raw
chunk copies `0x11 0x22 0x33` through untouched. *(tested: `compress_01_rlep.c`)*

## Notes

- **Size your destination buffer yourself.** The unpacker has no bounds check: it writes until the
  stream terminates. Record the unpacked size when you pack the data.
- `RLEp_UnpackToRAM` writes to **RAM**. To get the result into VRAM, unpack to a RAM scratch buffer
  and then [`VDP_WriteVRAM`](VDP-Access.md) it across.
- The module calls `Mem_Copy`/`Mem_Set`, so link [`memory.c`](Memory-Operations.md) with it.

---

# ZX0 (`unzx0`)

RLEp is a *run* packer: it wins on screens that are mostly one repeated tile, and does almost
nothing on data that lacks long runs — graphics, level layouts, music tables. **ZX0** is a real
LZ packer (literals + back-references, interlaced Elias-gamma lengths, last-offset reuse), so it
packs that data much tighter while keeping a tiny, fast Z80 decoder. Link `unzx0.c`, include
`unzx0.h`. Like `compress`, it is a *decompressor only* — you pack on the host.

> Decompressor for the ZX0 format by Einar Saukas (format only; independent implementation).
> The decoder here was written clean-room from the format description, with the reference
> decoder used only as a behavioural oracle.

## Packing your data

Use Einar Saukas's `zx0` encoder ([github.com/einar-saukas/ZX0](https://github.com/einar-saukas/ZX0))
in its default mode, then `#include` (or link) the bytes it emits:

```
zx0 tileset.bin tileset.zx0        # default = forward, v2 — the format UnZX0 decodes
```

## API

```c
void UnZX0(const u8* src, u8* dst);                    // decompress into RAM (69 B)
void UnZX0_fast(const u8* src, u8* dst);               // same output, ~19% faster (128 B)
void UnZX0_VRAM(const u8* src, u16 vramDst, u16 len);  // stream straight into VRAM
```

`UnZX0` is the small ROM-safe default; `UnZX0_fast` produces **byte-identical** output with ~19%
fewer T-states for 59 more bytes (see the table below). `len` is the uncompressed size (the stream
self-terminates on its end marker). All three run on MSX1 and MSX2.

## Unpacking a tileset into RAM

A tileset ships ZX0-packed in ROM and is expanded into a RAM buffer at load time. Because the
tile's second row here is identical to the first, ZX0 stores it as a single back-reference — the
LZ win a run packer cannot get.

```c
// tile_unpack.c — expand a ZX0-compressed tileset into RAM when a level loads.
#include "unzx0.h"

// Two identical 8×8 tile rows, ZX0-packed by the reference encoder: 16 → 13 bytes.
static const u8 g_PackedTiles[] = {
	0x0A, 0x00, 0x18, 0x3C, 0x7E, 0x7D, 0x3C, 0x18, 0x00, 0xF0, 0xC0, 0x00, 0x20,
};
static u8 g_Tiles[16];   // where the unpacked tile lands in RAM

// Unpack the tileset into RAM.
void unpack_tiles(void)
{
	UnZX0(g_PackedTiles, g_Tiles);
}
```

`g_Tiles` fills with `00 18 3C 7E 7E 3C 18 00` twice — the packer emitted the first row as eight
literals and the second as one back-reference.

When decode speed matters more than 59 bytes of ROM, `UnZX0_fast` is a drop-in replacement with
byte-identical output:

```c
// The same, on the fast path: byte-identical output, ~19% fewer cycles, still ROM-safe.
void unpack_tiles_fast(void)
{
	UnZX0_fast(g_PackedTiles, g_Tiles);
}
```

*(tested: `unzx0_01_roundtrip.c` round-trips a reference-packed corpus — corner cases plus 24
deterministic fuzz vectors, 30 in all — through **both** decoders, byte-exact on MSX1 **and**
MSX2.)*

## Streaming straight into VRAM

The common case is unpacking a tileset directly into VDP pattern memory with no RAM staging
buffer. `UnZX0_VRAM` writes literals to the VDP and resolves back-references by reading VRAM back
(so a repeated tile, or an overlapping RLE-style run, both unpack correctly). It masks interrupts
for the duration to keep the VDP address latch coherent.

```c
// tileset_to_vram.c — unpack a ZX0 tileset into VDP pattern memory.
#include "unzx0.h"

#define TILE_VRAM  0x3C00      // where the tile lands in VRAM (pattern memory)

static const u8 g_PackedTile[] = {
	0x0A, 0x00, 0x18, 0x3C, 0x7E, 0x7D, 0x3C, 0x18, 0x00, 0xF0, 0xC0, 0x00, 0x20,
};

void unpack_to_vram(void)
{
	UnZX0_VRAM(g_PackedTile, TILE_VRAM, 16);
}
```

The whole output must lie within one 16 KB VRAM page (the VDP auto-increment does not cross a
page). *(tested: `unzx0_02_vram.c`, which reads the unpacked bytes back from VRAM on MSX1 and
MSX2, including an overlapping run.)*

## Ours vs the reference decoder — measured

The library's thesis is *smaller **and** faster than the reference, measured, not asserted.* So we
assembled Einar Saukas's canonical `dzx0_standard` and `dzx0_turbo`, and both of ours, **with the
same assembler** (sdasz80) and timed them **on the same 4 KB corpus** (2185 bytes packed → 4096,
53%) under openMSX in fast mode. Function bytes and decode T-states, side by side:

| Decoder | Size | Decode | T/byte | Byte-exact | ROM-safe |
|---------|-----:|-------:|-------:|:----------:|:--------:|
| reference `dzx0_standard` | 69 B | 367,704 T | 89.8 | ✓ | ✓ |
| **msxmvl `UnZX0`** | **69 B** | **367,704 T** | **89.8** | ✓ | ✓ |
| **msxmvl `UnZX0_fast`** | **128 B** | **296,849 T** | **72.5** | ✓ | ✓ |
| reference `dzx0_turbo` | 128 B | 292,589 T¹ | 71.4 | ✓¹ | ✗ |

¹ `dzx0_turbo` is self-modifying — it patches its own `ld hl,nn` offset operand — so it **only runs
from RAM**; assembled into ROM (where MSX cart data lives) it decodes garbage. The 292,589 T is its
RAM-only figure.

**Honest verdict, per row:**

- **`UnZX0` ties `dzx0_standard` exactly** — same 69 bytes, same 367,704 T. Written independently
  from the format, an optimal ~70-byte routine converges to the same instruction budget (our byte
  layout differs; the totals do not). That is the size/speed Pareto floor for *ROM-safe* code, so a
  tie is the ceiling and we do not pretend to beat it.
- **`UnZX0_fast` is 19% faster than `UnZX0`** (72.5 vs 89.8 T/byte) for +59 bytes — the "turbo"
  shape (inlined Elias, offset off the stack) but **ROM-safe**. Where `dzx0_turbo` self-modifies
  its code (forcing RAM-resident execution), `UnZX0_fast` keeps that one slot in RAM *data*, which
  we measured costs just **1.0 T/byte** (72.5 vs turbo's 71.4). So it lands within 1.5% of the
  reference's fastest published decoder **while still running from ROM**, which turbo cannot. It
  does not beat turbo's raw cycle count; it makes turbo-class speed usable on a cartridge.

On top of matching the reference, the library adds what it has no equivalent for: a zero-overhead
SDCC C entry point (arguments arrive in `HL`/`DE` under `--sdcccall 1`, so there is no
calling-convention bridge) and the `UnZX0_VRAM` streaming variant.

## Notes

- **No bounds check** (same as the reference): size `dst` yourself; record the unpacked length at
  pack time.
- `UnZX0` and `UnZX0_fast` run with interrupts as-is and use the **alternate `AF` register** (the
  reference decoder's contract). If an interrupt handler clobbers `AF'`, mask interrupts around the
  call.
- `UnZX0_fast` also keeps its last-offset in one 2-byte RAM global (`g_ZX0_FastOff`); it is
  therefore not re-entrant, but it still runs from ROM (it does **not** self-modify code). Both
  decoders are otherwise self-contained and need no other module.


## More LZ decompressors (clean-room, measured)

Same round-trip oracle and measured discipline as ZX0, for the other MSX-scene formats so
your existing assets decompress natively. Each is byte-exact on MSX1 and MSX2.

# ZX7 decompression (`unzx7`)

Clean-room runtime decompressor for the **ZX7** format by Einar Saukas
([github.com/einar-saukas/ZX7](https://github.com/einar-saukas/ZX7)) — the
predecessor of ZX0. ZX7 is a little weaker than ZX0 but still a real optimal
LZ77/LZSS packer, and its stream stays common in existing MSX assets. The format
was read for facts only (literal-first LZ stream, non-interlaced Elias-gamma
lengths, 7-or-11-bit offsets); the decoder is our own, with the reference Z80
routines used purely as a behavioural oracle.

Pack on the host with the `zx7` encoder (default forward mode); expand on-target
with `UnZX7`. Link `unzx7.c`, include `unzx7.h`.

## API

```c
void UnZX7(const u8* src, u8* dst);   // decompress a ZX7 stream into RAM
```

The uncompressed size is fixed at pack time — size `dst` yourself; there is no
bounds check (same contract as the reference decoder). `UnZX7` uses only the main
registers (no `AF'`), so it needs no interrupt masking.

Unlike the reference Z80 "standard" routine — which spots the end marker via a
16-bit length overflow and so reads a couple of bytes *past* the compressed
block — `UnZX7` detects end-of-stream by counting the 16 leading zeros, so it
never reads beyond the last stream byte.

## Ours vs the reference ZX7 Z80 decoder

Both assembled the same way (`sdasz80`); decode speed measured on openMSX
(throttle off) over the committed 30-vector corpus (9,780 output bytes, ×16).

| Decoder | Size | Speed | Reads past end? |
|---|---:|---:|:--:|
| `UnZX7` (ours) | **78 B** | **97.1 T/byte** | no |
| Reference "standard" (Saukas / Villena / Metalbrain) | 71 B | 100.0 T/byte | yes (≈2 B) |

Honest result: ~3% fewer T-states per byte at a cost of 7 bytes of code. The
reference "standard" routine is famously tight (documented at 69–70 bytes); the
71 B above is our self-contained `sdasz80` port of it, measured on the identical
toolchain and corpus. Call it a tie on size, a hair ahead on speed, and safer at
the stream boundary.

## Usage

```c
// Expand a ZX7-packed tileset into RAM when a level loads.
#include "unzx7.h"

// Two identical 8x8 tile rows, ZX7-packed by the reference encoder: 16 -> 13 bytes.
static const u8 g_PackedTiles[] = {
	0x00, 0x01, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00, 0x3C, 0x07, 0x00, 0x02,
};
static u8 g_Tiles[16];

void unpack_tiles(void)
{
	UnZX7(g_PackedTiles, g_Tiles);   // g_Tiles now holds the 16 unpacked bytes
}
```

The on-target oracle `unzx7_01_roundtrip.c` decompresses a corpus of vectors
packed by the **reference** `zx7` encoder (corner cases — 1 byte, all-same,
incompressible — plus 24 fuzz vectors across a spread of sizes) and checks every
one byte-exact. Regenerate the corpus with `unzx7_corpus_gen.py`.

### Pletter (`UnPletter`)

`void UnPletter(const u8* src, u8* dst)` — decompress a Pletter v0.5c stream into RAM.
Clean-room decoder written from the Pletter format (independent of the reference depacker).
Pletter is the long-standing MSX-scene LZ format, so much existing MSX data is Pletter-packed.

```c
#include "unpletter.h"
u8 g_Tiles[512];
UnPletter(g_PackedTiles, g_Tiles);   // g_Tiles[] holds the decompressed bytes
```

| Decoder | Code size | ROM-safe | Byte-exact MSX1+MSX2 |
|---|---:|:--:|:--:|
| `UnPletter` (ours) | 184 B | yes | yes |

Verified byte-exact against the reference `pletter` encoder over an 18-vector corpus (corners:
1-byte, all-same, incompressible, tile data, a few KB) on C-BIOS MSX2 **and** MSX1.
*(tested: `unpletter_01_roundtrip.c`)*

# UnLZSA2 — runtime LZSA2 decompressor

Clean-room RAM decompressor for the **LZSA2** format by Emmanuel Marty
([github.com/emmanuel-marty/lzsa](https://github.com/emmanuel-marty/lzsa)) — a fast-
decompression LZ format. Written independently from the block-format spec; the
reference `lzsa` compressor and the spke/uniabis Z80 decoders were used only to
learn the format and as a byte-exact oracle.

## API

```c
#include "unlzsa2.h"

void UnLZSA2(const u8* src, u8* dst);   // decompress a raw LZSA2 stream into RAM
```

Pack your data on the host with the reference encoder in **raw LZSA2** mode:

```sh
lzsa -f2 -r  input.bin  output.lz2      # -f2 = LZSA2, -r = raw (frame-less) block
```

A raw block carries its own end-of-data marker, so `UnLZSA2` needs no length
argument. Size `dst` yourself (the uncompressed size is fixed at pack time); there
is **no bounds check**, matching the reference decoder. Overlapping back-references
(RLE-style runs, e.g. offset 1) are handled.

**State / safety.** `UnLZSA2` keeps its 4 bytes of decode state in RAM data
(`g_LZSA2_Off/Nib/Tok`), so it is **not re-entrant**, but it runs correctly **from
ROM** — there is no self-modifying code. It uses **only the main register set** (no
`ex af,af'`), so it is safe to call from an interrupt-driven program without masking
`AF'`.

## Usage example

```c
#include "unlzsa2.h"

// Two identical 8x8 tile rows, LZSA2-packed by `lzsa -f2 -r`: 16 -> 13 bytes.
static const u8 g_PackedTiles[] = {
    0x3E,0x5C,0x00,0x18,0x3C,0x7E,0x7E,0x3C,0x18,0x00,0xE7,0xF0,0xE8,
};
static u8 g_Tiles[16];

void unpack_tiles(void) {
    UnLZSA2(g_PackedTiles, g_Tiles);   // second row is stored as a back-reference
}
```

The round-trip oracle `unlzsa2_01_roundtrip.c` decompresses a committed corpus
(`unlzsa2_corpus.h`, packed by the reference encoder) and checks it byte-exact on
both MSX2 and MSX1.

## Measured size & speed (ours vs the reference LZSA2 Z80 decoder)

Both decoders assembled the **same way** (SDCC's `sdasz80`) and timed the **same
way**: pure decode between two PC markers on **C-BIOS_MSX2** (Z80 @ 3.579545 MHz,
openMSX fast mode), decompressing one realistic 4096-byte block
(`benchmarks/zx0-decompress/data/raw.bin`, packed to 2160 bytes with `lzsa -f2 -r`).
Byte-exact output was confirmed for both (checksum `0x01E1`).

| Decoder | Code size | Decode T-states/byte | ROM-safe | Touches `AF'` |
|---|---:|---:|:--:|:--:|
| **Ours** (`UnLZSA2`) | **247 B** (+4 B RAM state) | **96.8** | yes | no |
| Reference, size-opt, ROM-safe (`AVOID_SELFMODIFYING_CODE`, IX) | 133 B | 79.4 | yes | yes |
| Reference, size-opt, default (self-modifying) | 134 B | ~4% faster than IX¹ | **no** (RAM-only) | yes |
| Reference, speed-opt (self-modifying) | 210 B | fastest¹ | **no** (RAM-only) | yes |

¹ From the reference sources (`asm/z80/unlzsa2_small.asm`, `unlzsa2_fast.asm`); not
re-measured here — the self-modifying variants must execute from RAM, so they are
not an apples-to-apples ROM-safe comparison.

**Honest verdict: we do not beat the reference.** Ours is ~86% larger and ~22%
slower than the reference's ROM-safe size-optimized decoder. spke & uniabis's
decoders are the product of years of expert Z80 golfing (self-modifying code,
shadow registers, hand-tuned bit tricks); this is a straightforward clean-room
implementation. Where it differs on purpose: (1) it runs **from ROM with no self-
modifying code** — the reference's *default* configuration does not, requiring
either RAM execution or the slightly larger IX variant above; and (2) it leaves the
**shadow `AF'` register untouched** — the reference keeps its nibble reservoir in
`AF'`, so an interrupt handler that clobbers `AF'` would corrupt a decode in
progress. If you need the smallest/fastest LZSA2 decoder and can run from RAM, use
spke & uniabis's; if you want a plain C-callable, ROM- and `AF'`-safe one, use this.

## Correctness

Byte-exact against the reference `lzsa -f2 -r` encoder: 848+ host fuzz vectors
(deterministic seeds; truly-random/literal-heavy, structured/match-heavy, all-same,
and large blocks up to 70 KB) plus the 15 on-target corpus vectors, verified on both
MSX2 and MSX1. Every decode path is exercised: 5/9/13/16-bit and repeat match
offsets; direct, nibble, byte and 16-bit extended literal and match lengths;
overlapping matches; and the raw-block EOD marker.

## See also

- [Memory Operations](Memory-Operations.md) — `Mem_Copy`/`Mem_Set`, which this module builds on.
- [VDP Access](VDP-Access.md) — pushing the unpacked data into VRAM.
- [Tilemaps](Tilemaps.md) — the usual consumer of a compressed map.
