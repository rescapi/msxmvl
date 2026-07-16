// camera.c — accumulate a scroll offset as the player walks the level.
//
// Motion is expressed in pixels that pile up. Scroll_SetOffsetH adds to the horizontal
// offset (wrapping around the map width, so a level can loop); Scroll_SetOffsetV adds to
// the vertical offset but CLAMPS it to the map's margin, (SRC_H-DST_H)*8 pixels, so the
// camera can't scroll past the top or bottom edge. The running totals live in
// g_Scroll_OffsetX / g_Scroll_OffsetY; Scroll_Update (below) later programs the VDP.
#include "scroll.h"

// Walk the camera horizontally by dx pixels this frame (negative = left).
void camera_walk_h(i8 dx)
{
	Scroll_SetOffsetH(dx);
}

// Walk the camera vertically by dy pixels, clamped at the level's top/bottom edge.
void camera_walk_v(i8 dy)
{
	Scroll_SetOffsetV(dy);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	camera_walk_h(100);
	camera_walk_h(-40);                     // accumulates: 100 - 40 = 60
	R[1] = (u8)(g_Scroll_OffsetX & 0xFF);   // 60

	camera_walk_v(100);                     // clamps to (24-20)*8 = 32
	R[2] = (u8)(g_Scroll_OffsetY & 0xFF);   // 32

	R[0] = (R[1] == 60 && R[2] == 32) ? 0xA5 : 0x00;
	for (;;) {}
}
