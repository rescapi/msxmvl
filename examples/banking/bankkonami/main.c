// Resident main — the turnkey far-call program on a plain KONAMI cartridge.
// The resident runs from 0x4000-0x5FFF, which this mapper hard-wires to
// segment 0; play_level(3) hops through banks 5 and 6 via the 0x8000 window,
// selected through the mapper's own 0x8000 register.
#include "types.h"
#include "far.h"

volatile u8 __at(0xE000) R[8];

void main(void)
{
	u16 score = play_level(3);       // resident -> bank 5 -> bank 6, automatic
	R[1] = (u8)score;
	R[2] = (u8)(score >> 8);
	// play_level(3) = level_score(3) + 50 = (3 * 100) + 50 = 350
	R[0] = (score == 350) ? 0xA5 : 0x00;
}
