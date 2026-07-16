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

## See also

- [Memory Operations](Memory-Operations.md) — `Mem_Copy`/`Mem_Set`, which this module builds on.
- [VDP Access](VDP-Access.md) — pushing the unpacked data into VRAM.
- [Tilemaps](Tilemaps.md) — the usual consumer of a compressed map.
