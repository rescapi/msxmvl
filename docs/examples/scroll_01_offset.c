// Scrolling is expressed in pixels that accumulate. Scroll_SetOffsetH adds to the
// horizontal offset (wrapping around the map width); Scroll_SetOffsetV adds to the
// vertical offset (CLAMPED to the map's vertical margin, (SRC_H-DST_H)*8 pixels).
// Scroll_Update (not shown here — it needs the full map) then programs the VDP and
// redraws exposed edge tiles. This example verifies the offset bookkeeping.
#include "scroll.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	Scroll_SetOffsetH(100);
	Scroll_SetOffsetH(-40);                 // accumulates: 100 - 40 = 60
	R[1] = (u8)(g_Scroll_OffsetX & 0xFF);   // 60

	Scroll_SetOffsetV(100);                 // clamps to (24-20)*8 = 32
	R[2] = (u8)(g_Scroll_OffsetY & 0xFF);   // 32

	R[0] = (R[1] == 60 && R[2] == 32) ? 0xA5 : 0x00;
	for (;;) {}
}
