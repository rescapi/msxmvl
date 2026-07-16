// level.c — draw a chunk of the level's tile map onto the screen.
//
// A level is a byte array of tile indices; g_Map[row*width + col] says which tile goes in
// each cell. Tile_DrawMapChunk blits that rectangle of tiles from the tileset in VRAM to
// the screen with the command engine (VRAM->VRAM). Index TILE_SKIP_INDEX (0) counts as
// transparent and is left untouched, so you can overlay a chunk without erasing behind it.
#include "vdp.h"
#include "tile.h"

// A 2x2 corner of the level map. Tile 0 is transparent (skipped); 2,3,4 are drawn.
static const u8 g_Map[4] = { 0, 2, 3, 4 };

// Draw the 2x2 level chunk at screen cell (cx, cy).
void draw_level_chunk(u8 cx, u8 cy)
{
	Tile_DrawMapChunk(cx, cy, g_Map, 2, 2);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);
	Tile_SetDrawPage(0);
	Tile_SetBankPage(2);
	Tile_SelectBank(0);
	Tile_FillScreen(1);                    // screen background -> 0x11
	VDP_CommandWait();
	Tile_FillBank(0, 0x99);                // every tile's bitmap -> 0x99
	VDP_CommandWait();
	draw_level_chunk(0, 0);                // draw the 2x2 map at cell (0,0)
	VDP_CommandWait();

	R[1] = VDP_Peek_16K(0);    // cell (0,0), tile 0 = SKIP -> background 0x11
	R[2] = VDP_Peek_16K(4);    // cell (1,0), tile 2 = drawn -> 0x99
	R[0] = (R[1] == 0x11 && R[2] == 0x99) ? 0xA5 : 0x00;
	for (;;) {}
}
