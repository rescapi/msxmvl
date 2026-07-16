// sprite.c — mirror an 8-pixel sprite row so the hero can face the other way.
//
// Each byte of a tile/sprite row is 8 pixels. Math_Flip reverses the bit order
// (bit 0 <-> bit 7, 1 <-> 6, ...), which flips those 8 pixels left-to-right — the cheap
// way to draw a left-facing frame from a right-facing one, no extra artwork needed.
#include "math.h"

// Return one sprite/tile row mirrored horizontally.
u8 mirror_row(u8 row)
{
	return Math_Flip(row);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	R[1] = mirror_row(0x01);       // 0000 0001 -> 1000 0000 = 0x80
	R[2] = mirror_row(0x0F);       // 0000 1111 -> 1111 0000 = 0xF0
	R[0] = (R[1] == 0x80 && R[2] == 0xF0) ? 0xA5 : 0x00;
	for (;;) {}
}
