# Hardware Scroll (`scroll`)

Scroll a tile map larger than the screen, using the V9938's hardware scroll registers plus a
tile-refresh strategy at the edges. Link `vdp.c scroll.c`, include `scroll.h`.

## The model

The screen shows a **window** into a larger **source map**. Scrolling has two parts the module
combines for you:

1. **Sub-tile (pixel) offset** — the VDP shifts the whole display by 0–7 pixels via its horizontal
   (R#18 / R#26) and vertical (R#23) scroll registers. Cheap, every frame.
2. **Tile (8-pixel) step** — when the pixel offset wraps past 8, a new column/row of tiles must be
   drawn into the just-revealed edge. The module tracks this and refreshes only the edge tiles.

You express motion in pixels; the module decides when a tile refresh is due.

## API

```c
u8   Scroll_Initialize(u16 map);      // bind the source map, reset offsets; returns status
void Scroll_SetOffsetH(i8 offset);    // add horizontal pixels (wraps around the map)
void Scroll_SetOffsetV(i8 offset);    // add vertical pixels (clamped to the map margin)
void Scroll_Update(void);             // apply: program the VDP + refresh edge tiles
void Scroll_HBlankAdjust(u8 adjust);  // fine per-line horizontal adjust (raster effects)
```

Horizontal scrolling **wraps** (the map is treated as a cylinder); vertical scrolling **clamps** to
the non-visible margin `[0, (SCROLL_SRC_H − SCROLL_DST_H) × 8]`. The map dimensions come from the
`SCROLL_SRC_W/H` and `SCROLL_DST_W/H` build-time defines.

## A camera that follows the player (tested)

Think of the scroll offset as a **camera** that the player pushes around the level. Each frame you
add a few pixels of motion; the totals accumulate in `g_Scroll_OffsetX` / `g_Scroll_OffsetY`.
Horizontal motion wraps (the level can loop); vertical motion **clamps** at the top and bottom
edge, so the camera can't reveal past the map. With the default config the vertical margin is
`(24 − 20) × 8 = 32` pixels.

```c
// camera.c — accumulate a scroll offset as the player walks the level.
#include "scroll.h"

// Walk the camera horizontally by dx pixels this frame (negative = left).
void camera_walk_h(i8 dx)
{
	Scroll_SetOffsetH(dx);
}

// Walk the camera vertically by dy pixels, clamped at the level's top/bottom edge.
void camera_walk_v(i8 dy)
{
	Scroll_SetOffsetV(dy);
}
```

Walking `+100` then `−40` horizontally leaves the camera at `60` (the steps just add up); walking
`+100` vertically saturates at the `32`-pixel margin instead of overshooting the edge. *(tested:
`scroll_01_offset.c`)*

`Scroll_Update` — which programs the VDP and redraws exposed tiles — needs the full map, so it's
shown in the per-frame snippet below rather than tested inline.

## Per-frame usage

```c
#include "scroll.h"

Scroll_Initialize((u16)g_Map);      // once, with your map

for (;;) {
	Scroll_SetOffsetH(scroll_speed);  // move the window this frame
	VDP_WaitVBlank();                 // apply during blanking...
	Scroll_Update();                  // ...program registers + draw the new edge column
}
```

Call `Scroll_Update` once per frame after setting the offsets — it writes the scroll registers and
draws any newly exposed tiles. Pace it with [`VDP_WaitVBlank`](VBlank-Sync.md) so register changes
land off-screen.

## See also

- [Tilemaps](Tilemaps.md) — the tile map `scroll` moves across.
- [VBlank Sync](VBlank-Sync.md) — apply scroll updates during vertical blank.
