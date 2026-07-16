// vertex.c — scale a model vertex by a Q1.7 factor using the fast 8x8 multiply.
//
// There's no FPU: factors live in Q1.7 (an i8 where 127 == 1.0). G3D_MulS8 does the
// signed 8x8->16 multiply; shifting the product right by 7 returns it to integer
// units. e.g. cos(45 deg) is angle 32 of 256 -> G3D_Cos(32) = 90 (0.707 in Q1.7),
// so 100 * 0.707 becomes 100*90>>7 = 70.
#include "g3d.h"

#define ANGLE_45  32     // 45 degrees, as 1/8 of the 256-step turn

// Scale one coordinate by cos(angle) -- e.g. foreshortening a vertex as it turns.
i8 scale_by_cos(i8 coord, u8 angle)
{
	i16 p = G3D_MulS8(coord, G3D_Cos(angle));   // Q1.7 product, kept 16-bit
	return (i8)(p >> 7);                         // Q1.7 -> integer units
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	i8 scaled = scale_by_cos(100, ANGLE_45);   // 100 * cos(45) ~ 70

	R[1] = (u8)scaled;                   // 70 (0x46)
	R[0] = (scaled == 70) ? 0xA5 : 0x00;
	for (;;) {}
}
