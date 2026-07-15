// msxmvl extension: vertical-blank sync. See vsync.h. SDCC --sdcccall 1.
#include "vsync.h"

void VDP_WaitVBlank(void) __naked
{
	__asm
		ld   a, i            ; P/V <- IFF2 = caller's interrupt-enable state
		push af              ; (openMSX reads IFF2 exactly; the real-Z80 ld a,i erratum,
		di                   ;  which needs a hardware interrupt race, does not apply here)
		xor  a
		out  (0x99), a
		ld   a, #(0x80 | 15)
		out  (0x99), a       ; select status register S#0 (R#15 = 0)
		in   a, (0x99)       ; read once to clear any already-pending vblank flag
	wvb$:
		in   a, (0x99)       ; poll S#0
		and  #0x80           ; bit 7 = F (vertical-blank) flag
		jr   z, wvb$
		pop  af
		ret  po              ; IFF2 was 0 -> caller had interrupts disabled; leave them so
		ei                   ; else restore the enabled state we masked
		ret
	__endasm;
}
