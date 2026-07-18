// Resident main — the turnkey far-call program on a KONAMI_SCC cartridge.
// play_level(3) runs in ROM segment 5 and reaches level_score in segment 6:
// both hops select the 0x8000 window through the SCC mapper's 0x9000 register,
// so the right answer proves the mapper table end-to-end.
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
