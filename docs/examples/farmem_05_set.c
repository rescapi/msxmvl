// level.c — clear a level's map to a single tile before drawing it.
//
// Before you build a new level you usually wipe its map to one "empty" tile; before you reuse
// a buffer you zero it. Far_Set does that out in mapper RAM — it writes one repeated byte across
// a run and, like the block copies, crosses the 16 KB bank boundary for you, so clearing a big
// map that spans banks is still a single call.
#include "farmem.h"

#define BANK_MAP    22      // the level-map bank (a big map continues into bank 23)
#define TILE_GRASS  7       // the tile id we clear the map to

// Fill a run of the map with one tile — used to reset the map between levels.
void map_fill(u16 tile_index, u8 tile, u16 count)
{
	Far_Set(FAR(BANK_MAP, tile_index), tile, count);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	MemSeg_Init(16);
	map_fill(0x3FFE, TILE_GRASS, 4);            // clear 4 tiles, spanning bank 22 -> 23
	R[1] = Far_Peek(FAR(BANK_MAP,     0x3FFF));  // grass, in bank 22
	R[2] = Far_Peek(FAR(BANK_MAP + 1, 0x0000));  // grass, in bank 23 (past the edge)
	R[0] = (R[1] == TILE_GRASS && R[2] == TILE_GRASS) ? 0xA5 : 0x00;
	for (;;) {}
}
