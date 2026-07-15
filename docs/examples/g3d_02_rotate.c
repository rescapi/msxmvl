// G3D_Sin / G3D_Cos — 256-step angle, i8 amplitude 127 (=1.0 in Q1.7).
// Rotate the 2D point (64, 0) by 90 degrees (angle 64 of 256). Expected: (0, 64).
//   x' = x*cos - y*sin      y' = x*sin + y*cos
#include "g3d.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8 x = 64, y = 0;
	i8 s = G3D_Sin(64);          // sin(90 deg) = 127
	i8 c = G3D_Cos(64);          // cos(90 deg) = 0
	i8 nx = (i8)((G3D_MulS8(x, c) - G3D_MulS8(y, s)) >> 7);   // 0
	i8 ny = (i8)((G3D_MulS8(x, s) + G3D_MulS8(y, c)) >> 7);   // 63 (rounding)

	R[1] = (u8)nx;               // 0
	R[2] = (u8)ny;               // 63
	R[0] = (nx == 0 && ny == 63) ? 0xA5 : 0x00;
	for (;;) {}
}
