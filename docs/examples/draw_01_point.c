// radar.c — plot enemy blips on a bitmap minimap (SCREEN 5 / GRAPHIC4).
//
// Draw_Point lights one pixel at (x, y) in a 4-bit color. In GRAPHIC4 a VRAM byte packs
// two pixels — even x in the high nibble, odd x in the low nibble — so two blips side by
// side share a byte, at address y*128 + x/2. VDP_OP_IMP just replaces the pixel (no
// blending). Plotting single points like this is how you paint a radar or a minimap.
#include "vdp.h"
#include "draw.h"

#define BLIP_ENEMY  0x0A     // color of a hostile blip
#define BLIP_ALLY   0x0B     // ...and a friendly one

// Light one blip on the radar at pixel (x, y).
void radar_blip(u8 x, u8 y, u8 color)
{
	Draw_Point(x, y, color, VDP_OP_IMP);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	radar_blip(4, 2, BLIP_ENEMY);          // even x -> high nibble = A
	radar_blip(5, 2, BLIP_ALLY);           // odd x  -> low nibble  = B

	R[1] = VDP_Peek_16K(2 * 128 + 2);      // byte at (x=4..5, y=2) = 0xAB
	R[0] = (R[1] == 0xAB) ? 0xA5 : 0x00;
	for (;;) {}
}
