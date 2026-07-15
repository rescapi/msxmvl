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

## Unpacking a stream

This stream exercises all four chunk types. Keep the packed data `const` so it stays in ROM.

```c
#include "compress.h"
volatile u8 __at(0xE000) R[8];

// RLEp stream: [default] then chunks, terminated by 0x00.
static const u8 g_Packed[] = {
	0xAA,                    // default value for Type-0 runs
	0x02,                    // T0 x3  -> AA AA AA
	0x43, 0xBB,              // T1 x4  -> BB BB BB BB
	0x81, 0xCC, 0xDD,        // T2 x2  -> CC DD CC DD
	0xC2, 0x11, 0x22, 0x33,  // T3 x3  -> 11 22 33 (raw)
	0x00,                    // terminator
};
static u8 g_Out[16];

void main(void)
{
	u16 len = RLEp_UnpackToRAM(g_Packed, g_Out);

	R[1] = (u8)len;      // 14 unpacked bytes
	R[2] = g_Out[0];     // 0xAA  (default run)
	R[3] = g_Out[3];     // 0xBB  (1-byte run)
	R[4] = g_Out[7];     // 0xCC  (2-byte run)
	R[5] = g_Out[11];    // 0x11  (raw copy)

	R[0] = (len == 14 && g_Out[0] == 0xAA && g_Out[3] == 0xBB
	     && g_Out[7] == 0xCC && g_Out[11] == 0x11) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 0e aa bb cc 11` — 12 packed bytes became `0x0e` = 14. *(tested:
`compress_01_rlep.c`)*

## Notes

- **Size your destination buffer yourself.** The unpacker has no bounds check: it writes until the
  stream terminates. Record the unpacked size when you pack the data.
- `RLEp_UnpackToRAM` writes to **RAM**. To get the result into VRAM, unpack to a RAM scratch buffer
  and then [`VDP_WriteVRAM`](VDP-Access.md) it across.
- The module calls `Mem_Copy`/`Mem_Set`, so link [`memory.c`](Memory-Operations.md) with it.

## See also

- [Memory Operations](Memory-Operations.md) — `Mem_Copy`/`Mem_Set`, which this module builds on.
- [VDP Access](VDP-Access.md) — pushing the unpacked data into VRAM.
- [Tilemaps](Tilemaps.md) — the usual consumer of a compressed map.
