// hud.c — draw the player's health bar with the VDP command engine (GRAPHIC4).
//
// Draw_FillBox paints a solid rectangle between two inclusive corners in a single
// hardware-accelerated command. A health bar is just a filled box whose width tracks how
// much health is left — redraw it each time the player takes a hit. (Draw_LineH/LineV,
// Draw_Box, and Draw_Line are the sibling primitives for outlines and straight runs.)
#include "vdp.h"
#include "draw.h"

#define HP_COLOR   0x0F      // health-bar color (palette entry 15)
#define HP_TOP     3         // the bar's top row...
#define HP_BOTTOM  4         // ...and bottom row

// Draw a health bar filling x = 0 .. width-1, two rows tall.
void draw_health_bar(u8 width)
{
	Draw_FillBox(0, HP_TOP, width - 1, HP_BOTTOM, HP_COLOR, VDP_OP_IMP);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);

	draw_health_bar(8);                    // full bar: solid 8x2 block, color 15
	VDP_CommandWait();                     // let the command engine finish first

	R[1] = VDP_Peek_16K(3 * 128 + 0);      // row 3, first byte  = 0xFF
	R[2] = VDP_Peek_16K(4 * 128 + 3);      // row 4, x=6..7 byte = 0xFF
	R[0] = (R[1] == 0xFF && R[2] == 0xFF) ? 0xA5 : 0x00;
	for (;;) {}
}
