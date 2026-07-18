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
