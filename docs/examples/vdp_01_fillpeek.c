// VDP_FillVRAM_16K writes the same byte to a run of VRAM; VDP_Peek_16K reads one
// byte back. These use 14-bit (16 KB) VRAM addressing — the everyday range. Here we
// fill 16 bytes at VRAM 0x1000 with 0x5A and read the first and last back.
#include "vdp.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_FillVRAM_16K(0x5A, 0x1000, 16);     // VRAM[0x1000..0x100F] = 0x5A

	R[1] = VDP_Peek_16K(0x1000);            // 0x5A (first)
	R[2] = VDP_Peek_16K(0x100F);            // 0x5A (last)
	R[0] = (R[1] == 0x5A && R[2] == 0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
