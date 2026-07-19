# UnLZSA2 — runtime LZSA2 decompressor

Runtime decompressor for the **LZSA2** format by Emmanuel Marty
([github.com/emmanuel-marty/lzsa](https://github.com/emmanuel-marty/lzsa)) — a
fast-decompression LZ format designed to replace ZX7 on 8-bit machines.

> **Provenance — NOT clean-room.** Unlike the other decoders in this library, this
> module is **not** an independent implementation. It is the size-optimized LZSA2
> Z80 decompressor by **spke & uniabis** (from Marty's LZSA), transliterated to
> `sdasz80` and wrapped as an SDCC `--sdcccall 1` naked function. It is an *altered
> version* used under the **zlib licence** (full notice + attribution in
> `lib/ext/unlzsa2.c`; recorded in `NOTICE.md`). We ship the original rather than
> our own because spke & uniabis's decoder is already an optimal ROM-safe primitive
> — reimplementing it clean-room only produced something larger and slower (see
> "History" below).

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

**State / safety.** Runs correctly **from ROM** — no self-modifying code. The match
offset is held in one RAM global (`g_LZSA2_Off`), so the decoder is **not
re-entrant** — and so that it leaves `IX` (SDCC's callee-saved frame pointer)
intact, the one adaptation we made to the reference's ROM-safe variant, which parks
the offset in `IX`. It keeps its nibble reservoir in **`AF'`**, so it is **NOT
`AF'`-safe**: if an interrupt handler on your decode path uses the shadow
accumulator, mask/save `AF'` around the call.

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

## Size & speed

| Decoder | Code size | Decode T-states/byte¹ | ROM-safe | `AF'`-safe |
|---|---:|---:|:--:|:--:|
| **`UnLZSA2`** (spke & uniabis, ROM-safe, this module) | **134 B** | **42.0** | yes | no |
| Previous msxmvl clean-room implementation | 247 B | 49.5 | yes | yes |

¹ Same block, same toolchain (SDCC `sdasz80`), same method: pure decode between two
PC markers on **C-BIOS_MSX2** (Z80 @ 3.579545 MHz, openMSX fast mode) over one
realistic 4096-byte block packed with `lzsa -f2 -r`. T/byte is over *output* bytes,
so its absolute value depends on the block's compressibility — only the same-block
comparison is meaningful. Byte-exact output confirmed for both.

Shipping the reference replaced our clean-room decoder with one that is **−46%
smaller and −15% faster** on the same block, and is the recognised optimal ROM-safe
LZSA2 decoder for Z80.

## Correctness

Byte-exact against the reference `lzsa -f2 -r` encoder over the committed corpus
(corner cases — 1 byte, all-same, incompressible, tile data, a 4 KB block — plus
deterministic fuzz vectors) on both **MSX2 and MSX1**. Every decode path is
exercised: 5/9/13/16-bit and repeat match offsets; direct, nibble, byte and 16-bit
extended literal and match lengths; overlapping matches; and the raw-block EOD
marker. *(tested: `unlzsa2_01_roundtrip.c`)*

## History — why we ship the reference

This module was first written clean-room from the LZSA2 block-format spec (247 B,
ROM- and `AF'`-safe). It worked and was honest, but it was ~86% larger and slower
than spke & uniabis's decoder. Attempts to close the gap — register-backing the
state (probe: 232 B), then a shared-code rewrite — either gave only small gains at
the cost of the `AF'`-safety, or required reproducing the reference's exact
structure (which is no longer "clean-room"). The zlib licence explicitly permits
reuse with attribution, so the right call for a *format-compatibility* module is to
ship the proven original, credited, rather than a worse independent copy. For the
fast-and-small niche where you control the format, prefer **`unzx0`** (69 B, better
ratio); use `UnLZSA2` when you specifically need LZSA2 compatibility.
