// worldmap.c — a world map too big for 64 KB, stored across mapper banks.
//
// A big map can be larger than one 16 KB bank, so a run of tiles you read might START in one
// bank and CONTINUE in the next. You never split it yourself: Far_Read, Far_Write and Far_Set
// notice the 16 KB edge and carry on across it — so you treat the whole map as one long array.
#include "farmem.h"

#define BANK_MAP  19    // first bank of a world map that spans several banks

// Read a run of tiles from the map, starting at any tile index — even one that
// makes the run cross from one bank into the next. It "just works".
void map_read(u16 tile_index, u8* out, u16 count)
{
	Far_Read(FAR(BANK_MAP, tile_index), out, count);
}

// Write a run of tiles back.
void map_write(u16 tile_index, const u8* tiles, u16 count)
{
	Far_Write(FAR(BANK_MAP, tile_index), tiles, count);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 strip[4] = { 10, 11, 12, 13 };   // four adjacent map tiles
	u8 loaded[4];
	MemSeg_Init(16);
	// index 0x3FFE is 2 tiles before the bank's end, so tiles 12 and 13 land in the NEXT bank.
	map_write(0x3FFE, strip, 4);
	map_read (0x3FFE, loaded, 4);
	R[1] = Far_Peek(FAR(BANK_MAP,     0x3FFF));   // tile 11 — last byte of bank 19
	R[2] = Far_Peek(FAR(BANK_MAP + 1, 0x0000));   // tile 12 — first byte of bank 20
	R[0] = (loaded[0]==10 && loaded[1]==11 && loaded[2]==12 && loaded[3]==13 &&
	        R[1]==11 && R[2]==12) ? 0xA5 : 0x00;
	for (;;) {}
}
