// FAR POINTERS — data bigger than one segment "just works".
//
// Segments are 16 KB. A big world map, a long song, a wide tile row — these can be
// larger than a single segment, so a block you read or write may START in one segment
// and CONTINUE in the next. You do NOT have to split it yourself: Far_Write, Far_Read
// and Far_Set notice the 16 KB edge and carry on into the following segment for you.
//
// Here: a strip of map tiles is stored right at the end of the level segment, so it
// spills over into the next one. We write it as a single block and read it back whole —
// the crossing is invisible. FAR(seg, 0x3FFE) is 2 bytes before the end of a segment.
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

#define SEG_MAP    19       // first segment of a level map that is bigger than 16 KB

void main(void)
{
	u8 tiles[4] = { 10, 11, 12, 13 };   // four map tiles to store
	u8 loaded[4];                       // read them back here
	MemSeg_Init(16);

	// Start 2 bytes before the segment's end, so tiles 12 and 13 land in the NEXT segment.
	Far_Write(FAR(SEG_MAP, 0x3FFE), tiles, 4);      // one call — the split is automatic
	Far_Read (FAR(SEG_MAP, 0x3FFE), loaded, 4);     // read the whole strip back

	R[1] = Far_Peek(FAR(SEG_MAP,     0x3FFF));       // tile 11 — last byte of segment 19
	R[2] = Far_Peek(FAR(SEG_MAP + 1, 0x0000));       // tile 12 — first byte of segment 20
	R[0] = (loaded[0]==10 && loaded[1]==11 && loaded[2]==12 && loaded[3]==13 &&
	        R[1]==11 && R[2]==12) ? 0xA5 : 0x00;
	for (;;) {}
}
