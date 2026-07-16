// tilemap.c — the everyday RAM block ops: clear a row of the tilemap, blit a sprite.
//
// Mem_Set fills a buffer with one byte (like memset); Mem_Copy moves a buffer to
// another (like memcpy). Note the argument order: Mem_Set takes the value FIRST, and
// both take dest before size.
#include "memory.h"

#define TILE_BLANK  0x77    // the "empty" tile id we clear a row to

// Clear one row of the tilemap to the blank tile.
void clear_tile_row(u8* row, u16 width)
{
	Mem_Set(TILE_BLANK, row, width);
}

// Blit a sprite's pattern buffer into another buffer (e.g. the work buffer).
void blit_sprite(const u8* src, u8* dst, u16 bytes)
{
	Mem_Copy(src, dst, bytes);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 a[4], b[4];

	clear_tile_row(a, 4);          // a = { 77, 77, 77, 77 }
	blit_sprite(a, b, 4);          // b <- a

	R[1] = a[0]; R[2] = a[3];
	R[3] = b[0]; R[4] = b[3];
	R[0] = (a[0]==0x77 && a[3]==0x77 && b[0]==0x77 && b[3]==0x77) ? 0xA5 : 0x00;
	for (;;) {}
}
