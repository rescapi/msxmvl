// bigrom.c — a 32 KB cartridge: the ROM's second half (0x8000-0xBFFF) holds data.
//
// A 16 KB cart tops out at 0x7FFF. Building with crt0_rom32 (EX_ROM=32) gives the
// program the whole 0x4000-0xBFFF range — the crt0 enables the cartridge's own
// slot in page 2 at boot, so the second 16 KB is simply *there*: const tables,
// graphics, music data, all addressable with no banking and no copying.
#include "types.h"

// A table pinned into the ROM's second half — impossible on a 16 KB cart.
__at(0x9000) const u8 g_HighTable[64] = {
	  0,   3,   6,   9,  12,  15,  18,  21,  24,  27,  30,  33,  36,  39,  42,  45,
	 48,  51,  54,  57,  60,  63,  66,  69,  72,  75,  78,  81,  84,  87,  90,  93,
	 96,  99, 102, 105, 108, 111, 114, 117, 120, 123, 126, 129, 132, 135, 138, 141,
	144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 189
};

// Sum the table straight out of page 2.
u16 bigrom_checksum(void)
{
	u8 i; u16 sum = 0;
	for (i = 0; i < 64; ++i) sum += g_HighTable[i];
	return sum;                      // 64 entries of i*3: 3 * (63*64/2) = 6048
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u16 sum = bigrom_checksum();

	R[1] = (u8)sum;
	R[2] = (u8)(sum >> 8);
	R[3] = g_HighTable[1];           // 3 — read again, directly

	R[0] = (sum == 6048 && g_HighTable[1] == 3) ? 0xA5 : 0x00;
	for (;;) {}
}
