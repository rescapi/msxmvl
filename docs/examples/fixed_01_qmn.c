// physics.c — sub-pixel positions using Q7.1 fixed-point.
//
// Fixed-point numbers are named Qm.n: m integer bits, n fraction bits. In Q7.1 (7 integer,
// 1 fraction bit) a value is stored x2, so one step is half a pixel — enough to move
// smoothly without floats. QMN_Set8 packs a whole-pixel coordinate into Q7.1; QMN_Get8
// unpacks the whole-pixel part back out for drawing.
#include "fixed_point.h"

// Convert a whole-pixel coordinate into a Q7.1 sub-pixel position.
i8 to_subpixel(i8 pixels)
{
	return QMN_Set8(Q7_1, pixels);
}

// Recover the whole-pixel coordinate to draw the sprite at.
i8 to_pixel(i8 subpixel)
{
	return QMN_Get8(Q7_1, subpixel);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	i8 packed = to_subpixel(10);   // 10 in Q7.1 -> 20
	i8 back    = to_pixel(packed); // 20 -> 10

	R[1] = (u8)packed;             // 20
	R[2] = (u8)back;               // 10
	R[0] = (packed == 20 && back == 10) ? 0xA5 : 0x00;
	for (;;) {}
}
