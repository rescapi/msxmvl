// Far_Write / Far_Read copy a whole block to/from mapper RAM (map once, LDIR the
// block) — far faster than looping Far_Poke, and the natural way to stash or fetch
// an asset or a scratch buffer that lives in high RAM.
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
	u8 back[4];
	MemSeg_Init(16);

	Far_Write(FAR(19, 0x0000), src, 4);     // stash 4 bytes into segment 19
	Far_Read(FAR(19, 0x0000), back, 4);     // read them back

	R[1] = back[0]; R[2] = back[1]; R[3] = back[2]; R[4] = back[3];
	R[0] = (back[0]==0xDE && back[1]==0xAD && back[2]==0xBE && back[3]==0xEF)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
