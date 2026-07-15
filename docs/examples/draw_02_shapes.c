// Higher-level primitives, all hardware-accelerated by the VDP command engine:
// Draw_LineH/LineV (straight lines), Draw_Box (outline), Draw_FillBox (solid).
// Here a filled box of color 0x0F covers x=0..7, y=3..4; every covered pixel byte
// reads 0xFF (two color-15 pixels).
#include "vdp.h"
#include "draw.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	Draw_FillBox(0, 3, 7, 4, 0x0F, VDP_OP_IMP);   // solid 8x2 block, color 15
	VDP_CommandWait();                     // let the command engine finish first

	R[1] = VDP_Peek_16K(3 * 128 + 0);      // row 3, first byte  = 0xFF
	R[2] = VDP_Peek_16K(4 * 128 + 3);      // row 4, x=6..7 byte = 0xFF
	R[0] = (R[1] == 0xFF && R[2] == 0xFF) ? 0xA5 : 0x00;
	for (;;) {}
}
