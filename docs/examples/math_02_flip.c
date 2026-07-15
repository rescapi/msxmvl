// Math_Flip reverses the bit order of a byte (bit 0 <-> bit 7, etc.). Handy for
// mirroring a 8-pixel sprite/tile row horizontally.
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	R[1] = Math_Flip(0x01);        // 0000 0001 -> 1000 0000 = 0x80
	R[2] = Math_Flip(0x0F);        // 0000 1111 -> 1111 0000 = 0xF0
	R[0] = (R[1] == 0x80 && R[2] == 0xF0) ? 0xA5 : 0x00;
	for (;;) {}
}
