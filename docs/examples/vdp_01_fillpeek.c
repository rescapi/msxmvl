// hud.c — clear the on-screen status bar in VRAM, then sample it back.
//
// The HUD strip lives in its own run of VRAM. Before redrawing it each level we wipe the
// whole strip to one blank tile with a single VDP_FillVRAM_16K — far faster than poking byte
// by byte, because the VRAM address is latched once and the fill then streams. VDP_Peek_16K
// reads a single byte back, handy as a sanity check that the strip really did clear.
#include "vdp.h"

#define HUD_VRAM    0x1000    // where the status-bar strip sits in VRAM
#define HUD_WIDTH   16        // bytes in the strip
#define TILE_BLANK  0x5A      // the "empty" tile we clear it to

// Wipe the whole status bar to the blank tile.
void hud_clear(void)
{
	VDP_FillVRAM_16K(TILE_BLANK, HUD_VRAM, HUD_WIDTH);
}

// Read one tile of the status bar back.
u8 hud_peek(u16 offset)
{
	return VDP_Peek_16K(HUD_VRAM + offset);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	hud_clear();                        // VRAM[0x1000..0x100F] = 0x5A

	R[1] = hud_peek(0);                 // first tile -> 0x5A
	R[2] = hud_peek(HUD_WIDTH - 1);     // last tile  -> 0x5A
	R[0] = (R[1] == 0x5A && R[2] == 0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
