// Resident main — acceptance test for ASCII-16 banking (MAPPER ASCII16).
// Same data-model checks as test/bankdata, plus the one thing ASCII-8 cannot
// do: a bank whose CONTENT is bigger than 8 KB. bankA carries a ~9 KB const
// table; its checksum only comes out right if the full 16 KB segment is mapped.
#include "types.h"
#include "far.h"
#include "bank_big.h"   // generated: BIG_EXPECTED (checksum of bankA's table)

static u16 res_magic = 0x1616;          // resident initialized static
static u8  res_bss[8];                  // resident BSS
volatile u8 __at(0xE000) R[8];
volatile u8 __at(0xC800) dynmem[64];    // reserved region (bank.manifest RESERVE)

void main(void)
{
	u8 i, ok;
	u16 sum;

	R[1] = (res_magic == 0x1616) ? 1 : 0;

	ok = 1;
	for (i = 0; i < 8; ++i) if (res_bss[i] != 0) ok = 0;
	R[2] = ok;

	// per-bank statics: initial values + zeroed BSS, in a 16 KB segment
	R[3] = (far_a_probe(0) && far_b_probe(0)) ? 1 : 0;

	// the >8 KB payload: only correct if the WHOLE 16 KB segment is visible
	sum = far_a_sum(0);
	R[4] = (sum == BIG_EXPECTED) ? 1 : 0;
	R[5] = (u8)sum;
	R[6] = (u8)(sum >> 8);

	dynmem[0] = 0x77;
	far_b_mutate(0x40);
	R[7] = (dynmem[0] == 0x77 && far_b_readback(0) == 0x80
	        && res_magic == 0x1616) ? 1 : 0;

	R[0] = (R[1] & R[2] & R[3] & R[4] & R[7]) ? 0xA5 : 0x00;
}
