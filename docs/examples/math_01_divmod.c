// Math_Div10 / Math_Mod10 are fast divide/modulo by 10 — the building blocks for
// turning a number into decimal digits without a general (slow) division routine.
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	R[1] = (u8)Math_Div10(95);     // 95 / 10 = 9
	R[2] = Math_Mod10(95);         // 95 % 10 = 5
	R[0] = (R[1] == 9 && R[2] == 5) ? 0xA5 : 0x00;
	for (;;) {}
}
