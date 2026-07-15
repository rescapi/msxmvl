// VDP_WaitVBlank blocks until the next vertical blank, then returns — and it
// preserves the caller's interrupt-enable state (it masks interrupts internally
// but restores them). Here we enter with interrupts enabled and confirm they are
// still enabled after the call (IFF2 read via `ld a,i` -> P/V flag).
#include "vsync.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	__asm ei __endasm;
	VDP_WaitVBlank();              // waits one field, then returns (no hang)

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
