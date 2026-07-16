// tileset.c — upload a tile's pattern into VRAM and read it back.
//
// Graphics data lives in ROM/RAM as byte arrays; to draw it, the VDP needs it in VRAM.
// VDP_WriteVRAM_16K streams a whole buffer into VRAM in one shot — the address is latched
// once and the run streams, the fast way to upload tiles, sprites, or a bitmap.
// VDP_ReadVRAM_16K copies a VRAM run back into RAM, e.g. to checksum an upload.
#include "vdp.h"

#define PATTERN_VRAM  0x0800    // where this tile's pattern goes in VRAM
#define TILE_BYTES    4         // bytes in the pattern

// Upload a tile's pattern bytes to VRAM.
void tile_upload(const u8* pattern)
{
	VDP_WriteVRAM_16K(pattern, PATTERN_VRAM, TILE_BYTES);
}

// Read a tile's pattern back out of VRAM.
void tile_download(u8* out)
{
	VDP_ReadVRAM_16K(PATTERN_VRAM, out, TILE_BYTES);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	const u8 tile[4] = { 0x12, 0x34, 0x56, 0x78 };
	u8 back[4];

	tile_upload(tile);             // upload 4 bytes to VRAM 0x0800
	tile_download(back);           // read them back into RAM

	R[1] = back[0]; R[2] = back[1]; R[3] = back[2]; R[4] = back[3];
	R[0] = (back[0]==0x12 && back[1]==0x34 && back[2]==0x56 && back[3]==0x78)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
