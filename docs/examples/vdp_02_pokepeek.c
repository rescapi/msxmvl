// Single-byte VRAM access: VDP_Poke_16K writes one byte, VDP_Peek_16K reads one.
// Use these for occasional touches (a tile here, a status byte there); for runs,
// prefer Fill/Write which set the VRAM address once and stream.
#include "vdp.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_Poke_16K(0x3C, 0x0200);            // VRAM[0x0200] = 0x3C
	VDP_Poke_16K(0xC3, 0x0201);            // VRAM[0x0201] = 0xC3

	R[1] = VDP_Peek_16K(0x0200);           // 0x3C
	R[2] = VDP_Peek_16K(0x0201);           // 0xC3
	R[0] = (R[1] == 0x3C && R[2] == 0xC3) ? 0xA5 : 0x00;
	for (;;) {}
}
