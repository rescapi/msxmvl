// A FarPtr is a linear address into mapper RAM: FAR(segment, offset). Far_Poke /
// Far_Peek do single-byte access by handle — you never touch a raw window address
// or a segment register yourself.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);

	Far_Poke(FAR(17, 0x0100), 0x7E);        // write one byte in segment 17
	Far_Poke(FAR(18, 0x0100), 0x3C);        // ...and one in segment 18

	R[1] = Far_Peek(FAR(17, 0x0100));       // 0x7E — segments are independent
	R[2] = Far_Peek(FAR(18, 0x0100));       // 0x3C
	R[0] = (R[1] == 0x7E && R[2] == 0x3C) ? 0xA5 : 0x00;
	for (;;) {}
}
