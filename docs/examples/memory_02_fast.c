// level.c — move a block of level data with the fast (unrolled-LDI) copies.
//
// Mem_FastSet / Mem_FastCopy are unrolled-LDI variants: same result as Mem_Set /
// Mem_Copy, but faster on larger blocks. Reach for them on hot copy/clear paths —
// scrolling a chunk of the map into place, clearing a work area every frame.
#include "memory.h"

#define TILE_FLOOR  0x5A    // the floor tile we fill a fresh strip with

// Fill a strip of the level with the floor tile (hot path -> fast fill).
void fill_floor_strip(u8* strip, u16 bytes)
{
	Mem_FastSet(TILE_FLOOR, strip, bytes);
}

// Move a block of level data (e.g. scrolling a chunk into view) at full speed.
void move_level_block(const u8* src, u8* dst, u16 bytes)
{
	Mem_FastCopy(src, dst, bytes);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 a[8], b[8];

	fill_floor_strip(a, 8);        // a = eight 0x5A bytes
	move_level_block(a, b, 8);     // b <- a

	R[1] = b[0]; R[2] = b[7];
	R[0] = (b[0]==0x5A && b[7]==0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
