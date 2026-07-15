// Tile draws a screen from a map of tile indices, copying each tile's bitmap from
// a tileset in VRAM (command-engine VRAM->VRAM). Here we fill the tile bank with a
// known pattern (0x99), fill the screen with background (0x11), then draw a 2x2 map
// whose index 0 is the transparent TILE_SKIP_INDEX (left as background) and the
// others are drawn. Verifying VRAM shows exactly which cells were painted.
#include "vdp.h"
#include "tile.h"
volatile u8 __at(0xE000) R[8];

static const u8 g_Map[4] = { 0, 2, 3, 4 };   // index 0 = skip (transparent)

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
	Tile_DrawMapChunk(0, 0, g_Map, 2, 2);  // draw the 2x2 map at cell (0,0)
	VDP_CommandWait();

	R[1] = VDP_Peek_16K(0);    // cell (0,0), tile 0 = SKIP -> background 0x11
	R[2] = VDP_Peek_16K(4);    // cell (1,0), tile 2 = drawn -> 0x99
	R[0] = (R[1] == 0x11 && R[2] == 0x99) ? 0xA5 : 0x00;
	for (;;) {}
}
