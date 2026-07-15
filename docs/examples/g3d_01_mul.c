// G3D_MulS8 — fast signed 8x8->16 multiply, ideal for Q1.7 fixed-point.
// Scale x=100 by cos(45 deg): angle 32 of 256 -> G3D_Cos(32)=90 (0.707 in Q1.7).
// A Q1.7 product is >>7 to return to integer units: 100*90>>7 = 70.
#include "g3d.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8  x = 100;
	i16 p = G3D_MulS8(x, G3D_Cos(32));   // = 100 * 90 = 9000
	i8  scaled = (i8)(p >> 7);           // Q1.7 -> integer: 9000>>7 = 70

	R[1] = (u8)scaled;                   // 70 (0x46)
	R[0] = (scaled == 70) ? 0xA5 : 0x00;
	for (;;) {}
}
