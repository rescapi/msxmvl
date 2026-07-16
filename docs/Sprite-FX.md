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

A character that walks both ways needs only **one** hand-drawn facing: build the mirror in RAM
and upload whichever way it's moving. A tumbling enemy is the vertical flip.

```c
// hero_sprite.c — make the hero face left or right from a single sprite pattern.
#include "sprite_fx.h"

// Build the left-facing pattern from the right-facing one.
void face_left(const u8* facing_right, u8* facing_left)
{
	SpriteFX_FlipHorizontal8(facing_right, facing_left);
}

// Build an upside-down pattern (e.g. a defeated enemy tumbling off the top).
void flip_upside_down(const u8* upright, u8* tumbling)
{
	SpriteFX_FlipVertical8(upright, tumbling);
}
```

Feed in a diagonal source pattern and the horizontal flip mirrors it left↔right (row `0x01`
becomes `0x80`), while the vertical flip reverses the row order (top row `0x01` becomes the
bottom) — the source diagonal mirrored both ways. *(tested: `sprfx_01_flip.c`)*

## Beyond flips

`sprite_fx.h` also provides **crop** (`SpriteFX_CropLeft8` / `CropRight8` / `CropTop8` /
`CropBottom8` and 16-bit variants) — shift a pattern by `offset` pixels, filling the vacated edge
with zeros, for smooth edge-of-screen clipping — and **mask** (`SpriteFX_Mask8` / `Mask16`) — AND a
pattern with a mask. All operate the same way: `src` → `dest` in RAM.

## See also

- [VDP Access](VDP-Access.md) — upload the transformed pattern with `VDP_WriteVRAM_16K`, or use the
  sprite helpers (`VDP_LoadSpritePattern`, `VDP_SetSprite`) in `vdp.h`.
