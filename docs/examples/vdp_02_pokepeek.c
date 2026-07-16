// tilemap.c — poke individual tiles into the name table, one byte at a time.
//
// Most VRAM writes come in runs, but sometimes you only need to change one tile — drop a coin
// here, flip a switch tile there. VDP_Poke_16K writes a single byte to a VRAM address and
// VDP_Peek_16K reads one back. Each call re-latches the VRAM address, so for a whole row use
// VDP_WriteVRAM_16K instead; for the odd tile, a poke is just right.
#include "vdp.h"

#define NAME_TABLE  0x0200   // base of the tile name table in VRAM

// Put a tile id into a cell of the name table.
void tile_put(u16 cell, u8 tile)
{
	VDP_Poke_16K(tile, NAME_TABLE + cell);
}

// Read the tile id back from a cell.
u8 tile_get(u16 cell)
{
	return VDP_Peek_16K(NAME_TABLE + cell);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	tile_put(0, 0x3C);             // VRAM[0x0200] = 0x3C
	tile_put(1, 0xC3);             // VRAM[0x0201] = 0xC3

	R[1] = tile_get(0);            // -> 0x3C
	R[2] = tile_get(1);            // -> 0xC3
	R[0] = (R[1] == 0x3C && R[2] == 0xC3) ? 0xA5 : 0x00;
	for (;;) {}
}
