# Tilemaps (`tile`)

Draw a screen from a **tile map** — an array of tile indices — onto a GRAPHIC4+ bitmap, copying each
tile's bitmap from a tileset in VRAM via the command engine. Link `vdp.c tile.c`, include `tile.h`.
(`tile.c` already includes the command-engine functions it needs, so don't also link `vdp_inl.c`.)

## The model

A **tile** is a small fixed-size bitmap (commonly 8×8), and a **tileset** is a strip of them stored
in VRAM. A **map** is a byte array of tile indices — `map[row*width + col]` is which tile goes at
that cell. `tile` reads the map and, for each cell, uses a VRAM-to-VRAM copy (`LMMM`) to blit that
tile's bitmap from the tileset to the screen position. This is how you build backgrounds and levels
from a compact map instead of a full-screen bitmap.

The tile size and layout come from build-time defines (`TILE_WIDTH`, `TILE_HEIGHT`,
`TILE_PER_ROW`). An optional transparent index (`TILE_SKIP_INDEX`) is skipped when drawing, so you
can overlay.

## API

```c
void Tile_LoadBankEx(u8 bank, const u8* data, u16 offset, u16 num);   // upload a tileset to VRAM
void Tile_DrawScreen(const u8* map);                                   // draw a full screen map
void Tile_DrawMapChunk(u8 x, u8 y, const u8* map, u8 width, u8 height);// draw a sub-rectangle
```

## Usage (tested)

Set the draw page / bank, fill a tileset, then draw a map. Here the bank is filled with a known
pattern (`0x99`) and the screen with a background (`0x11`); the 2×2 map's index `0` is the
transparent `TILE_SKIP_INDEX` (left as background) while the others are drawn — so reading VRAM
shows exactly which cells were painted.

```c
#include "vdp.h"
#include "tile.h"
volatile u8 __at(0xE000) R[8];

static const u8 g_Map[4] = { 0, 2, 3, 4 };   // index 0 = skip (transparent)

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);
	Tile_SetDrawPage(0);
	Tile_SetBankPage(2);
	Tile_SelectBank(0);
	Tile_FillScreen(1);                    // screen background -> 0x11
	VDP_CommandWait();
	Tile_FillBank(0, 0x99);                // every tile's bitmap -> 0x99
	VDP_CommandWait();
	Tile_DrawMapChunk(0, 0, g_Map, 2, 2);  // draw the 2x2 map at cell (0,0)
	VDP_CommandWait();

	R[1] = VDP_Peek_16K(0);    // cell (0,0), tile 0 = SKIP -> background 0x11
	R[2] = VDP_Peek_16K(4);    // cell (1,0), tile 2 = drawn -> 0x99
	R[0] = (R[1] == 0x11 && R[2] == 0x99) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 11 99` — the transparent cell kept the background, the next cell got the tile.
*(tested: `tile_01_map.c`)*

In a real project you upload actual tile bitmaps with `Tile_LoadBankEx(0, g_TileSet, 0, count)` and
paint a full screen with `Tile_DrawScreen(g_Map)`, using `Tile_DrawMapChunk` to refresh only a
changed region rather than repainting everything.

## See also

- [Hardware Scroll](Hardware-Scroll.md) — scroll a map larger than the screen.
- [Bitmap Drawing](Bitmap-Drawing.md) — the command engine that copies the tiles.
