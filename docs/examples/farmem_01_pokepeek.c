// FAR POINTERS — reach one byte of data that lives beyond the Z80's 64 KB.
//
// A "far pointer" is a handle to a byte somewhere in the big mapper RAM, written
// FAR(segment, offset). Far_Poke writes one byte through it; Far_Peek reads one back.
// You never touch the mapper hardware or the 0x8000 window yourself — the library maps
// the right segment in, does the access, and maps your old one back.
//
// Here: two players' high scores, kept in two different segments. Because the segments
// are separate physical memory, player 1's score can never clobber player 2's — no
// bookkeeping, no overlap.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_PLAYER1  17     // segment holding player 1's save data
#define SEG_PLAYER2  18     // ...and player 2's
#define OFF_HISCORE  0      // where in the segment the high score lives

void main(void)
{
	MemSeg_Init(16);                                    // bring up the mapper (home = 16)

	Far_Poke(FAR(SEG_PLAYER1, OFF_HISCORE), 95);        // player 1 scored 95
	Far_Poke(FAR(SEG_PLAYER2, OFF_HISCORE), 72);        // player 2 scored 72

	R[1] = Far_Peek(FAR(SEG_PLAYER1, OFF_HISCORE));     // read them back -> 95
	R[2] = Far_Peek(FAR(SEG_PLAYER2, OFF_HISCORE));     // -> 72

	R[0] = (R[1] == 95 && R[2] == 72) ? 0xA5 : 0x00;    // both intact, side by side
	for (;;) {}
}
