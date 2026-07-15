// msxmvl — clean-room reimplementation of MSXgl "sprite_fx" module public API.
// Interface (names, signatures, documented behaviour, __PRESERVES contracts)
// taken from MSXgl's engine/src/sprite_fx.h header ONLY. No MSXgl
// implementation source (.c/.asm/.lst) was read.
//
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack.
//
// Sprite data layout (MSX VDP):
//   8x8   : 8 bytes, byte[row] = one row, bit7 = leftmost pixel (col 0).
//   16x16 : 32 bytes, four 8x8 quadrants in order upper-left, lower-left,
//           upper-right, lower-right. Equivalently:
//             left  column (cols 0-7)  = bytes  0..15 indexed by row (0..15)
//             right column (cols 8-15) = bytes 16..31 indexed by row (0..15)
#ifndef MSXMVL_SPRITE_FX_H
#define MSXMVL_SPRITE_FX_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// CONFIG (defaults mirror MSXgl: all TRUE)
//=============================================================================
#ifndef SPRITEFX_USE_8x8
#define SPRITEFX_USE_8x8		TRUE
#endif
#ifndef SPRITEFX_USE_16x16
#define SPRITEFX_USE_16x16		TRUE
#endif
#ifndef SPRITEFX_USE_CROP
#define SPRITEFX_USE_CROP		TRUE
#endif
#ifndef SPRITEFX_USE_FLIP
#define SPRITEFX_USE_FLIP		TRUE
#endif
#ifndef SPRITEFX_USE_MASK
#define SPRITEFX_USE_MASK		TRUE
#endif
#ifndef SPRITEFX_USE_ROTATE
#define SPRITEFX_USE_ROTATE		TRUE
#endif

//=============================================================================
// Group: Cropping
//=============================================================================
#if (SPRITEFX_USE_CROP)

#if (SPRITEFX_USE_8x8)
// Crop 8x8 sprite left border (clear leftmost 'offset' columns). offset 0-7.
void SpriteFX_CropLeft8(const u8* src, u8* dest, u8 offset);
// Crop 8x8 sprite right border (clear rightmost 'offset' columns). offset 0-7.
void SpriteFX_CropRight8(const u8* src, u8* dest, u8 offset);
// Crop 8x8 sprite top border (clear topmost 'offset' rows). offset 0-7.
void SpriteFX_CropTop8(const u8* src, u8* dest, u8 offset);
// Crop 8x8 sprite bottom border (clear bottommost 'offset' rows). offset 0-7.
void SpriteFX_CropBottom8(const u8* src, u8* dest, u8 offset);
#endif // (SPRITEFX_USE_8x8)

#if (SPRITEFX_USE_16x16)
// Crop 16x16 sprite left border (clear leftmost 'offset' columns). offset 0-15.
void SpriteFX_CropLeft16(const u8* src, u8* dest, u8 offset);
// Crop 16x16 sprite right border (clear rightmost 'offset' columns). offset 0-15.
void SpriteFX_CropRight16(const u8* src, u8* dest, u8 offset);
// Crop 16x16 sprite top border (clear topmost 'offset' rows). offset 0-15.
void SpriteFX_CropTop16(const u8* src, u8* dest, u8 offset);
// Crop 16x16 sprite bottom border (clear bottommost 'offset' rows). offset 0-15.
void SpriteFX_CropBottom16(const u8* src, u8* dest, u8 offset);
#endif // (SPRITEFX_USE_16x16)

#endif // (SPRITEFX_USE_CROP)

//=============================================================================
// Group: Flipping
//=============================================================================
#if (SPRITEFX_USE_FLIP)

#if (SPRITEFX_USE_8x8)
// Vertical flip 8x8 sprite (mirror top<->bottom).
void SpriteFX_FlipVertical8(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
// Horizontal flip 8x8 sprite (mirror left<->right).
void SpriteFX_FlipHorizontal8(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
#define SpriteFX_FlipV8 SpriteFX_FlipVertical8
#define SpriteFX_FlipH8 SpriteFX_FlipHorizontal8
#endif // (SPRITEFX_USE_8x8)

#if (SPRITEFX_USE_16x16)
// Vertical flip 16x16 sprite (mirror top<->bottom).
void SpriteFX_FlipVertical16(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
// Horizontal flip 16x16 sprite (mirror left<->right).
void SpriteFX_FlipHorizontal16(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
#define SpriteFX_FlipV16 SpriteFX_FlipVertical16
#define SpriteFX_FlipH16 SpriteFX_FlipHorizontal16
#endif // (SPRITEFX_USE_16x16)

#endif // (SPRITEFX_USE_FLIP)

//=============================================================================
// Group: Masking
//=============================================================================
#if (SPRITEFX_USE_MASK)

#if (SPRITEFX_USE_8x8)
// Mask 8x8 sprite: dest[i] = src[i] & mask[i].
void SpriteFX_Mask8(const u8* src, u8* dest, const u8* mask);
#endif
#if (SPRITEFX_USE_16x16)
// Mask 16x16 sprite: dest[i] = src[i] & mask[i].
void SpriteFX_Mask16(const u8* src, u8* dest, const u8* mask);
#endif

#endif // (SPRITEFX_USE_MASK)

//=============================================================================
// Group: Rotating
//=============================================================================
#if (SPRITEFX_USE_ROTATE)

#if (SPRITEFX_USE_8x8)
// Rotate 8x8 sprite 90 degrees clockwise.
void SpriteFX_RotateRight8(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
// Rotate 8x8 sprite 90 degrees counter-clockwise.
void SpriteFX_RotateLeft8(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
// Rotate 8x8 sprite 180 degrees.
void SpriteFX_RotateHalfTurn8(const u8* src, u8* dest) __PRESERVES(iyl, iyh);
#define SpriteFX_Rotate90_8 SpriteFX_RotateRight8
#define SpriteFX_Rotate180_8 SpriteFX_RotateHalfTurn8
#define SpriteFX_Rotate270_8 SpriteFX_RotateLeft8
#endif // (SPRITEFX_USE_8x8)

#if (SPRITEFX_USE_16x16)
// Rotate 16x16 sprite 90 degrees clockwise.
void SpriteFX_RotateRight16(const u8* src, u8* dest);
// Rotate 16x16 sprite 90 degrees counter-clockwise.
void SpriteFX_RotateLeft16(const u8* src, u8* dest);
// Rotate 16x16 sprite 180 degrees.
void SpriteFX_RotateHalfTurn16(const u8* src, u8* dest);
#define SpriteFX_Rotate90_16 SpriteFX_RotateRight16
#define SpriteFX_Rotate180_16 SpriteFX_RotateHalfTurn16
#define SpriteFX_Rotate270_16 SpriteFX_RotateLeft16
#endif // (SPRITEFX_USE_16x16)

#endif // (SPRITEFX_USE_ROTATE)

#endif // MSXMVL_SPRITE_FX_H
