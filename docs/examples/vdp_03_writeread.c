// Block transfer: VDP_WriteVRAM_16K copies a RAM buffer into VRAM, VDP_ReadVRAM_16K
// copies VRAM back into RAM. This is how you upload tile/sprite/bitmap data and read
// it back — one VRAM address setup, then a streamed run.
#include "vdp.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	const u8 tile[4] = { 0x12, 0x34, 0x56, 0x78 };
	u8 back[4];

	VDP_WriteVRAM_16K(tile, 0x0800, 4);    // upload 4 bytes to VRAM 0x0800
	VDP_ReadVRAM_16K(0x0800, back, 4);     // read them back into RAM

	R[1] = back[0]; R[2] = back[1]; R[3] = back[2]; R[4] = back[3];
	R[0] = (back[0]==0x12 && back[1]==0x34 && back[2]==0x56 && back[3]==0x78)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
