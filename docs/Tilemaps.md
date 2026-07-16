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

## Drawing a level chunk (tested)

A level is just an array of tile indices. `Tile_DrawMapChunk` paints a rectangle of that map onto
the screen — the everyday way to draw a room, or to refresh only the part of the level that
scrolled into view. Index `0` is the transparent `TILE_SKIP_INDEX`: cells holding it are left
untouched, so a chunk can be overlaid without erasing what's behind it.

```c
// level.c — draw a chunk of the level's tile map onto the screen.
#include "vdp.h"
#include "tile.h"

// A 2x2 corner of the level map. Tile 0 is transparent (skipped); 2,3,4 are drawn.
static const u8 g_Map[4] = { 0, 2, 3, 4 };   // index 0 = skip (transparent)

// Draw the 2x2 level chunk at screen cell (cx, cy).
void draw_level_chunk(u8 cx, u8 cy)
{
	Tile_DrawMapChunk(cx, cy, g_Map, 2, 2);
}
```

In the test the tileset bank is filled with a known pattern (`0x99`) and the screen with a
background (`0x11`), then this chunk is drawn at cell `(0,0)`: the transparent tile-`0` cell keeps
the `0x11` background while the tile-`2` cell next to it becomes `0x99` — reading VRAM back shows
exactly which cells were painted. *(tested: `tile_01_map.c`)*

In a real project you upload actual tile bitmaps with `Tile_LoadBankEx(0, g_TileSet, 0, count)` and
paint a full screen with `Tile_DrawScreen(g_Map)`, using `Tile_DrawMapChunk` to refresh only a
changed region rather than repainting everything.

## See also

- [Hardware Scroll](Hardware-Scroll.md) — scroll a map larger than the screen.
- [Bitmap Drawing](Bitmap-Drawing.md) — the command engine that copies the tiles.
