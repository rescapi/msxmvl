# Bitmap Drawing (`draw`)

Draw pixels, lines, boxes, and circles into a SCREEN 5 / GRAPHIC4 bitmap, hardware-accelerated by
the V9938 command engine. Link `vdp.c draw.c`, include `draw.h`. Set a bitmap mode first with
`VDP_SetMode(VDP_MODE_GRAPHIC4)`.

## Coordinates, color, and op

Every primitive takes screen pixel coordinates, a 4-bit `color` (0–15, the GRAPHIC4 palette), and
a logical **op** — how the new pixels combine with what's there: `VDP_OP_IMP` (replace),
`VDP_OP_AND`, `VDP_OP_OR`, `VDP_OP_XOR`, and their transparent (`T*`) variants. `XOR` is handy for
rubber-band cursors (draw, then draw again to erase).

In GRAPHIC4 a VRAM byte holds **two 4-bit pixels**: even x = high nibble, odd x = low nibble, and
the byte address is `y*128 + x/2`. The examples verify pixels by reading VRAM back at that address.

---

## `Draw_Point`

```c
void Draw_Point(UX x, UY y, u8 color, u8 op);
```

Plot a single pixel. Plotting points one at a time is how you paint a radar or minimap —
each contact is one colored blip, and because GRAPHIC4 packs two pixels per byte, two blips
side by side share a VRAM byte:

```c
// radar.c — plot enemy blips on a bitmap minimap (SCREEN 5 / GRAPHIC4).
#include "vdp.h"
#include "draw.h"

#define BLIP_ENEMY  0x0A     // color of a hostile blip
#define BLIP_ALLY   0x0B     // ...and a friendly one

// Light one blip on the radar at pixel (x, y).
void radar_blip(u8 x, u8 y, u8 color)
{
	Draw_Point(x, y, color, VDP_OP_IMP);
}
```

Blipping an enemy at even `x=4` and an ally at odd `x=5` on row 2 packs color `A` into the
high nibble and `B` into the low nibble of the same byte — reading it back gives `0xAB`.
*(tested: `draw_01_point.c`)*

---

## `Draw_LineH` / `Draw_LineV` / `Draw_Box` / `Draw_FillBox` / `Draw_Line` / `Draw_Circle`

```c
void Draw_LineH (UX x1, UX x2, UY y, u8 color, u8 op);
void Draw_LineV (UX x, UY y1, UY y2, u8 color, u8 op);
void Draw_Box   (UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);   // outline
void Draw_FillBox(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);  // solid
void Draw_Line  (UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);   // arbitrary
void Draw_Circle(UX x, UY y, u8 radius, u8 color, u8 op);
```

The box functions take the **inclusive** top-left and bottom-right corners (so
`Draw_FillBox(0,3,7,4,...)` fills an 8×2 block).

> **Wait for the engine.** The command engine runs asynchronously. If you read VRAM (or issue a
> CPU-VRAM access) right after a *large* draw, call `VDP_CommandWait()` first so the operation has
> finished. `draw`/`vdp` functions already wait for the *previous* command before starting a new
> one, so back-to-back draws are safe — it's only CPU reads mid-flight that need the explicit wait.

A **health bar** is a natural use of `Draw_FillBox`: a solid box whose width tracks how much
health is left, redrawn whenever the player takes a hit.

```c
// hud.c — draw the player's health bar with the VDP command engine (GRAPHIC4).
#include "vdp.h"
#include "draw.h"

#define HP_COLOR   0x0F      // health-bar color (palette entry 15)
#define HP_TOP     3         // the bar's top row...
#define HP_BOTTOM  4         // ...and bottom row

// Draw a health bar filling x = 0 .. width-1, two rows tall.
void draw_health_bar(u8 width)
{
	Draw_FillBox(0, HP_TOP, width - 1, HP_BOTTOM, HP_COLOR, VDP_OP_IMP);
}
```

A full bar of width 8 fills the inclusive block `(0,3)–(7,4)` in color 15; sampling any covered
byte reads `0xFF` (two color-15 pixels), confirming the fill reached the whole 8×2 region.
*(tested: `draw_02_shapes.c`)*

## See also

- [Double Buffering](Double-Buffering.md) — draw into the hidden page (`+ Display_GetBackPageY()`
  on the Y coordinate), then flip.
- [VDP Access](VDP-Access.md) — the underlying VRAM/register layer.
