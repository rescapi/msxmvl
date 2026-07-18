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
