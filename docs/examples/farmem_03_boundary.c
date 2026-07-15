// Objects may straddle a 16 KB segment boundary. Far_Read/Far_Write auto-split at
// the edge: a block starting near the end of segment 19 continues into segment 20,
// transparently. FAR_ADD / FAR arithmetic carries across segments too.
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 src[4] = { 0xC0, 0xC1, 0xC2, 0xC3 };
	u8 back[4];
	MemSeg_Init(16);

	// start two bytes before the end of segment 19 -> spills into segment 20
	Far_Write(FAR(19, 0x3FFE), src, 4);
	Far_Read (FAR(19, 0x3FFE), back, 4);

	R[1] = Far_Peek(FAR(19, 0x3FFF));       // 0xC1 — last byte of seg 19
	R[2] = Far_Peek(FAR(20, 0x0000));       // 0xC2 — first byte of seg 20
	R[0] = (back[0]==0xC0 && back[1]==0xC1 && back[2]==0xC2 && back[3]==0xC3 &&
	        R[1]==0xC1 && R[2]==0xC2) ? 0xA5 : 0x00;
	for (;;) {}
}
