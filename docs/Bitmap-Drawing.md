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

Plot a single pixel.

```c
#include "vdp.h"
#include "draw.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	Draw_Point(4, 2, 0x0A, VDP_OP_IMP);    // even x -> high nibble = A
	Draw_Point(5, 2, 0x0B, VDP_OP_IMP);    // odd x  -> low nibble  = B

	R[1] = VDP_Peek_16K(2 * 128 + 2);      // byte at (x=4..5, y=2) = 0xAB
	R[0] = (R[1] == 0xAB) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 ab` — two adjacent pixels packed into one byte as `0xAB`. *(tested:
`draw_01_point.c`)*

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

```c
#include "vdp.h"
#include "draw.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	Draw_FillBox(0, 3, 7, 4, 0x0F, VDP_OP_IMP);   // solid 8x2 block, color 15
	VDP_CommandWait();                     // let the command engine finish first

	R[1] = VDP_Peek_16K(3 * 128 + 0);      // row 3, first byte  = 0xFF
	R[2] = VDP_Peek_16K(4 * 128 + 3);      // row 4, x=6..7 byte = 0xFF
	R[0] = (R[1] == 0xFF && R[2] == 0xFF) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 ff ff` — both sampled bytes are two color-15 pixels, confirming the fill covered
the full 8×2 region. *(tested: `draw_02_shapes.c`)*

## See also

- [Double Buffering](Double-Buffering.md) — draw into the hidden page (`+ Display_GetBackPageY()`
  on the Y coordinate), then flip.
- [VDP Access](VDP-Access.md) — the underlying VRAM/register layer.
