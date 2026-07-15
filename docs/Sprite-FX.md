# Sprite Transforms (`sprite_fx`)

Transform sprite (or tile) patterns **in RAM** — flip, crop, and mask — before uploading them to
VRAM. Pure computation: no VDP, no dependencies. Link `sprite_fx.c`, include `sprite_fx.h`.

## Why transform in RAM

The V9938 can't mirror or shift a sprite for you. The idiom is to **precompute** the variants you
need once (e.g. a character facing left and right) from a single source pattern, store them, and
just select which to upload. These functions do that transform on the raw pattern bytes.

A pattern is a column of row bytes: **8 bytes** for an 8×8 sprite, **32 bytes** for a 16×16 sprite
(the `8`/`16` in each function name). Each bit is one pixel.

---

## `SpriteFX_FlipHorizontal8` / `SpriteFX_FlipVertical8`

```c
void SpriteFX_FlipHorizontal8(const u8* src, u8* dest);   // mirror left<->right
void SpriteFX_FlipVertical8  (const u8* src, u8* dest);    // mirror top<->bottom
```

Horizontal flip **bit-reverses every row byte** (left edge becomes right edge); vertical flip
**reverses the row order** (top row becomes bottom). 16×16 variants (`*16`) work on the 32-byte
pattern, handling its four 8×8 quadrants.

```c
#include "sprite_fx.h"
volatile u8 __at(0xE000) R[8];

static const u8 pat[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

void main(void)
{
	u8 h[8], v[8];
	SpriteFX_FlipHorizontal8(pat, h);      // each row bit-reversed
	SpriteFX_FlipVertical8(pat, v);        // rows reversed

	R[1] = h[0];                           // flip(0x01) = 0x80
	R[2] = h[7];                           // flip(0x80) = 0x01
	R[3] = v[0];                           // pat[7] = 0x80
	R[4] = v[7];                           // pat[0] = 0x01
	R[0] = (h[0]==0x80 && h[7]==0x01 && v[0]==0x80 && v[7]==0x01) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 80 01 80 01` — the source diagonal is mirrored both ways. *(tested:
`sprfx_01_flip.c`)*

## Beyond flips

`sprite_fx.h` also provides **crop** (`SpriteFX_CropLeft8` / `CropRight8` / `CropTop8` /
`CropBottom8` and 16-bit variants) — shift a pattern by `offset` pixels, filling the vacated edge
with zeros, for smooth edge-of-screen clipping — and **mask** (`SpriteFX_Mask8` / `Mask16`) — AND a
pattern with a mask. All operate the same way: `src` → `dest` in RAM.

## See also

- [VDP Access](VDP-Access.md) — upload the transformed pattern with `VDP_WriteVRAM_16K`, or use the
  sprite helpers (`VDP_LoadSpritePattern`, `VDP_SetSprite`) in `vdp.h`.
