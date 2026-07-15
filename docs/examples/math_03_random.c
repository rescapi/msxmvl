// Math_GetRandom16 is a 16-bit xorshift PRNG — fully deterministic from its seed,
// so the same seed reproduces the same sequence (good for replays / procedural
// content). (Note: the 8-bit Math_GetRandom8 mixes in the Z80 R register for extra
// entropy and is therefore NOT reproducible — use the 16-bit one when you need
// determinism.)
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Math_SetRandomSeed16(12345);
	u16 first  = Math_GetRandom16();
	u16 second = Math_GetRandom16();

	Math_SetRandomSeed16(12345);   // reseed identically...
	u16 again  = Math_GetRandom16(); // ...reproduces `first`

	R[1] = (u8)first;
	R[2] = (u8)second;
	R[0] = (again == first && second != first) ? 0xA5 : 0x00;
	for (;;) {}
}
