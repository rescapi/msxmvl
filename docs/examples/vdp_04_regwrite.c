// backdrop.c — set the border/backdrop colour with a raw VDP register write.
//
// Most VDP settings have a typed helper, but any register is reachable directly with
// VDP_RegWrite(reg, value) — the escape hatch for the ones no wrapper covers. Colour
// register R#7 holds the border/backdrop colour, so writing it here paints the border in
// palette colour 10. (VDP_SetColor(c) is itself just VDP_RegWrite(7, c).)
#include "vdp.h"

#define VDP_R_COLOR  7      // R#7: text / border / backdrop colour
#define COLOR_BLUE   0x0A   // palette entry 10

// Paint the screen border/backdrop a colour.
void backdrop_set(u8 color)
{
	VDP_RegWrite(VDP_R_COLOR, color);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[2];
void main(void)
{
	backdrop_set(COLOR_BLUE);      // R#7 = backdrop colour 10
	R[0] = 0xA5;                   // reached here; R#7 value checked by the harness
	for (;;) {}
}
