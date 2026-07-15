// Sprite FX transform 8x8 (or 16x16) sprite patterns in RAM — no VDP involved.
// An 8x8 pattern is 8 bytes, one per row (each bit = one pixel).
//   FlipHorizontal8: mirror left<->right  (bit-reverse every row byte)
//   FlipVertical8:   mirror top<->bottom  (reverse the row order)
// Precompute both facings of a sprite once, then just upload the one you need.
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
