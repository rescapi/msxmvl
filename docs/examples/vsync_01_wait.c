// gameloop.c — pace the game to the display's refresh, tear-free.
//
// VDP_WaitVBlank blocks until the next vertical blank, then returns — one call per frame paces
// the whole game to 50/60 Hz. It masks interrupts internally while it pokes the VDP, but
// restores the caller's interrupt-enable state on return, so it drops safely into a loop that
// leaves interrupts on for music or timers.
#include "vsync.h"

// Wait for the start of the next frame's vertical blank.
void wait_for_frame(void)
{
	VDP_WaitVBlank();
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[2];
void main(void)
{
	__asm ei __endasm;
	wait_for_frame();             // waits one field, then returns (no hang)

	__asm
		ld   a, i                 ; P/V = IFF2 (interrupt-enable state)
		jp   pe, 1$               ; P/V=1 -> interrupts still enabled
		xor  a
		jr   2$
	1$:
		ld   a, #0xA5
	2$:
		ld   (_R + 0), a          ; R[0] = 0xA5 iff interrupts preserved
	__endasm;
	for (;;) {}
}
