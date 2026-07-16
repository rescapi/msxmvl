// rotate.c — spin a 2D point around the origin with Q1.7 sin/cos.
//
// G3D_Sin / G3D_Cos take a 256-step angle (0..255 == a full turn) and return an i8
// amplitude in Q1.7 (127 == 1.0). The standard 2D rotation, each product shifted
// back to integer units:   x' = x*cos - y*sin      y' = x*sin + y*cos
#include "g3d.h"

#define ANGLE_90  64     // 90 degrees, as a quarter of the 256-step turn

// Rotate point (x,y) about the origin by `angle`; writes the result back in place.
void rotate_point(i8* x, i8* y, u8 angle)
{
	i8 s = G3D_Sin(angle);
	i8 c = G3D_Cos(angle);
	i8 nx = (i8)((G3D_MulS8(*x, c) - G3D_MulS8(*y, s)) >> 7);
	i8 ny = (i8)((G3D_MulS8(*x, s) + G3D_MulS8(*y, c)) >> 7);
	*x = nx;
	*y = ny;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	i8 x = 64, y = 0;

	rotate_point(&x, &y, ANGLE_90);      // rotate (64,0) by 90 deg -> (0, 63)

	R[1] = (u8)x;                // 0
	R[2] = (u8)y;                // 63 (Q1.7 rounding of 64)
	R[0] = (x == 0 && y == 63) ? 0xA5 : 0x00;
	for (;;) {}
}
