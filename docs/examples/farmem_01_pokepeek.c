// hiscore.c — a per-player high-score table that lives out in mapper RAM.
//
// Far_Poke writes one byte to a spot in the big mapper memory, addressed by (bank, offset);
// Far_Peek reads one back. The library maps the bank in, does the access, and maps your own
// memory back — so it feels like ordinary memory that just happens to be huge. Give each
// player their own 16 KB bank and their scores can never tread on each other.
#include "farmem.h"

#define BANK_P1  17     // player 1's save data has its own 16 KB bank
#define BANK_P2  18     // ...and player 2's

// Store a player's high score.
void hiscore_save(u8 player_bank, u8 score)
{
	Far_Poke(FAR(player_bank, 0), score);
}

// Read a player's high score back.
u8 hiscore_load(u8 player_bank)
{
	return Far_Peek(FAR(player_bank, 0));
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	MemSeg_Init(16);                    // once at startup: bring up the mapper (home = 16)
	hiscore_save(BANK_P1, 95);          // player 1 scored 95
	hiscore_save(BANK_P2, 72);          // player 2 scored 72
	R[1] = hiscore_load(BANK_P1);       // -> 95
	R[2] = hiscore_load(BANK_P2);       // -> 72
	R[0] = (R[1] == 95 && R[2] == 72) ? 0xA5 : 0x00;
	for (;;) {}
}
