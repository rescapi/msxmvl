// FAR POINTERS — fill a far region with one value.
//
// Before you draw a new level you usually clear its map to a single "empty" tile;
// before you reuse a buffer you zero it. Far_Set does that out in mapper RAM: it writes
// one repeated byte across a run, and — like Far_Write — it crosses the 16 KB segment
// boundary for you, so clearing a big buffer that spans segments is still one call.
//
// Here: clear a stretch of level map to the "grass" tile (id 7). The stretch sits at the
// end of one segment and runs into the next, and Far_Set fills both halves.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_MAP    22       // level-map segment (the run continues into segment 23)
#define TILE_GRASS 7        // the tile id we clear the map to

void main(void)
{
	MemSeg_Init(16);

	Far_Set(FAR(SEG_MAP, 0x3FFE), TILE_GRASS, 4);   // clear 4 tiles, spanning seg 22 -> 23

	R[1] = Far_Peek(FAR(SEG_MAP,     0x3FFF));       // grass, in segment 22
	R[2] = Far_Peek(FAR(SEG_MAP + 1, 0x0000));       // grass, in segment 23 (past the edge)
	R[0] = (R[1] == TILE_GRASS && R[2] == TILE_GRASS) ? 0xA5 : 0x00;
	for (;;) {}
}
