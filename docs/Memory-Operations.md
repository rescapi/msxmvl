# Memory Operations (`memory`)

Block copy and fill for main RAM — the Z80 equivalents of `memcpy`/`memset`, plus faster
unrolled variants for hot paths. Link `memory.c`, include `memory.h`.

---

## `Mem_Set` / `Mem_Copy`

```c
void Mem_Set(u8 val, void* dest, u16 size);
void Mem_Copy(const void* src, void* dest, u16 size);
```

Fill `size` bytes with `val`; copy `size` bytes from `src` to `dest`. Note the argument order:
`Mem_Set` takes the **value first**, and both take `dest` before `size`. Two everyday jobs —
clearing a row of the tilemap to a blank tile, and blitting a sprite's pattern buffer:

```c
// tilemap.c — the everyday RAM block ops: clear a row of the tilemap, blit a sprite.
#include "memory.h"

#define TILE_BLANK  0x77    // the "empty" tile id we clear a row to

// Clear one row of the tilemap to the blank tile.
void clear_tile_row(u8* row, u16 width)
{
	Mem_Set(TILE_BLANK, row, width);
}

// Blit a sprite's pattern buffer into another buffer (e.g. the work buffer).
void blit_sprite(const u8* src, u8* dst, u16 bytes)
{
	Mem_Copy(src, dst, bytes);
}
```

`clear_tile_row` fills the row with `TILE_BLANK`; `blit_sprite` copies one buffer to another.
*(tested: `memory_01_copyset.c`)*

---

## `Mem_FastSet` / `Mem_FastCopy`

```c
void Mem_FastSet(u8 val, void* dest, u16 size);
void Mem_FastCopy(const void* src, void* dest, u16 size);
```

Same contract, unrolled-`LDI` implementations that are faster for larger blocks (verified
byte-identical to the plain versions, no small-size penalty). Reach for these on hot copy/clear
paths — scrolling a chunk of the level map into place, or clearing a work area every frame:

```c
// level.c — move a block of level data with the fast (unrolled-LDI) copies.
#include "memory.h"

#define TILE_FLOOR  0x5A    // the floor tile we fill a fresh strip with

// Fill a strip of the level with the floor tile (hot path -> fast fill).
void fill_floor_strip(u8* strip, u16 bytes)
{
	Mem_FastSet(TILE_FLOOR, strip, bytes);
}

// Move a block of level data (e.g. scrolling a chunk into view) at full speed.
void move_level_block(const u8* src, u8* dst, u16 bytes)
{
	Mem_FastCopy(src, dst, bytes);
}
```

`fill_floor_strip` clears a strip to `TILE_FLOOR`; `move_level_block` copies a block at full
speed — same result as the plain versions, just quicker. *(tested: `memory_02_fast.c`)*

> For copying to/from **VRAM**, use the VDP block functions instead
> ([`VDP_WriteVRAM_16K` / `VDP_ReadVRAM_16K`](VDP-Access.md)) — VRAM isn't CPU-addressable.
> For copying to/from **mapper RAM beyond 64 KB**, use [`Far_Read` / `Far_Write`](Far-Pointers.md).

## See also

- [VDP Access](VDP-Access.md) — the VRAM counterpart of these operations.
