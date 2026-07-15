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

## Offset bookkeeping (tested)

Motion is expressed in pixels that accumulate in `g_Scroll_OffsetX` / `g_Scroll_OffsetY`. This
example verifies the accumulation and the vertical clamp (with the default config, the vertical
margin is `(24 − 20) × 8 = 32` pixels). `Scroll_Update` — which programs the VDP and redraws exposed
tiles — needs the full map, so it's shown in the per-frame snippet below rather than tested inline.

```c
#include "scroll.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	Scroll_SetOffsetH(100);
	Scroll_SetOffsetH(-40);                 // accumulates: 100 - 40 = 60
	R[1] = (u8)(g_Scroll_OffsetX & 0xFF);   // 60

	Scroll_SetOffsetV(100);                 // clamps to (24-20)*8 = 32
	R[2] = (u8)(g_Scroll_OffsetY & 0xFF);   // 32

	R[0] = (R[1] == 60 && R[2] == 32) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 3c 20` (`0x3c=60`, `0x20=32`). *(tested: `scroll_01_offset.c`)*

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
