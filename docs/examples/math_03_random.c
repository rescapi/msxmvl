// spawn.c — pick enemy spawns from a seed you can reproduce for replays.
//
// Math_GetRandom16 is a 16-bit xorshift PRNG: fully deterministic from its seed, so the
// same seed replays the exact same wave of enemies (great for demos and replays). Seed
// once at the start of a level, then draw a value per spawn. (The 8-bit Math_GetRandom8
// mixes in the Z80 R register for extra entropy and is therefore NOT reproducible — use
// the 16-bit one when you need determinism.)
#include "math.h"

// Start a level's spawn sequence from a known seed.
void spawn_seed(u16 seed)
{
	Math_SetRandomSeed16(seed);
}

// Draw the next spawn value (e.g. which enemy to place, or where).
u16 spawn_next(void)
{
	return Math_GetRandom16();
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	spawn_seed(12345);
	u16 first  = spawn_next();
	u16 second = spawn_next();

	spawn_seed(12345);             // reseed identically...
	u16 again  = spawn_next();     // ...reproduces `first`

	R[1] = (u8)first;
	R[2] = (u8)second;
	R[0] = (again == first && second != first) ? 0xA5 : 0x00;
	for (;;) {}
}
