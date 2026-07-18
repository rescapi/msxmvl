// Resident main — acceptance test for the bankpack C-runtime DATA MODEL.
// Every kind of static a real program has, in the resident AND in two banks:
// initialized data (must be copied ROM->RAM per bank, with the right bank
// mapped), BSS (must be zeroed), and an __at reserved region (must be left
// alone by the runtime and defended by the build's collision check).
#include "types.h"
#include "far.h"

static u16 res_magic = 0x1234;          // resident initialized static
static u8  res_bss[8];                  // resident BSS
volatile u8 __at(0xE000) R[8];          // harness result block
volatile u8 __at(0xC800) dynmem[128];   // reserved region (bank.manifest RESERVE)

void main(void)
{
	u8 i, ok;

	// resident initialized data really got its value
	R[1] = (res_magic == 0x1234) ? 1 : 0;

	// resident BSS really got zeroed
	ok = 1;
	for (i = 0; i < 8; ++i) if (res_bss[i] != 0) ok = 0;
	R[2] = ok;

	// each bank sees its own initial values and zeroed BSS
	R[3] = (far_a_probe(0) && far_b_probe(0)) ? 1 : 0;

	// writes in one bank must not alias the other bank's statics
	far_a_mutate(0x30);
	far_b_mutate(0x40);
	R[4] = (far_a_readback(0) == 0x60 && far_b_readback(0) == 0x80) ? 1 : 0;

	// the reserved region is real RAM the program owns
	dynmem[0] = 0x77;
	R[5] = (dynmem[0] == 0x77) ? 1 : 0;

	// resident statics stay independent of bank writes
	res_bss[0] = 0xEE;
	far_a_mutate(0x31);
	R[6] = (res_bss[0] == 0xEE && res_magic == 0x1234) ? 1 : 0;

	R[0] = (R[1] & R[2] & R[3] & R[4] & R[5] & R[6]) ? 0xA5 : 0x00;
}
