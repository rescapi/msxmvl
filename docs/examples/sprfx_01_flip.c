// hero_sprite.c — make the hero face left or right from a single sprite pattern.
//
// The V9938 can't mirror a sprite for you, so you precompute both facings in RAM once and
// upload whichever way the hero is walking — no second hand-drawn frame to turn around.
// An 8x8 pattern is 8 bytes, one per row (each bit a pixel). FlipHorizontal8 bit-reverses
// every row (left<->right); FlipVertical8 reverses the row order (top<->bottom).
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

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

static const u8 pat[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

void main(void)
{
	u8 h[8], v[8];
	face_left(pat, h);                     // each row bit-reversed
	flip_upside_down(pat, v);              // rows reversed

	R[1] = h[0];                           // flip(0x01) = 0x80
	R[2] = h[7];                           // flip(0x80) = 0x01
	R[3] = v[0];                           // pat[7] = 0x80
	R[4] = v[7];                           // pat[0] = 0x01
	R[0] = (h[0]==0x80 && h[7]==0x01 && v[0]==0x80 && v[7]==0x01) ? 0xA5 : 0x00;
	for (;;) {}
}
