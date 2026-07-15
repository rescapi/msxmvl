// Draw_Point plots one pixel via the VDP command engine (SCREEN 5 / GRAPHIC4).
// In GRAPHIC4 a VRAM byte holds two 4-bit pixels: even x = high nibble, odd x =
// low nibble, and the byte address is y*128 + x/2. The last arg is the logical
// operation (VDP_OP_IMP = plain replace).
#include "vdp.h"
#include "draw.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	Draw_Point(4, 2, 0x0A, VDP_OP_IMP);    // even x -> high nibble = A
	Draw_Point(5, 2, 0x0B, VDP_OP_IMP);    // odd x  -> low nibble  = B

	R[1] = VDP_Peek_16K(2 * 128 + 2);      // byte at (x=4..5, y=2) = 0xAB
	R[0] = (R[1] == 0xAB) ? 0xA5 : 0x00;
	for (;;) {}
}
