// tileset_to_vram.c — unpack a ZX0 tileset straight into VDP pattern memory.
//
// The common case for a game: a packed tileset in ROM expanded directly into
// VRAM with no RAM staging buffer. UnZX0_VRAM streams literals to the VDP and
// resolves LZ back-references by reading VRAM back, so a repeated tile (a copy
// from an earlier VRAM address) unpacks straight into pattern memory.
#include "unzx0.h"

#define TILE_VRAM  0x3C00      // where the tile lands in VRAM (pattern memory)

// Two identical 8×8 tile rows, ZX0-packed (16 → 13 bytes); row 2 is a back-reference.
static const u8 g_PackedTile[] = {
	0x0A, 0x00, 0x18, 0x3C, 0x7E, 0x7D, 0x3C, 0x18, 0x00, 0xF0, 0xC0, 0x00, 0x20,
};

void unpack_to_vram(void)
{
	UnZX0_VRAM(g_PackedTile, TILE_VRAM, 16);
}

// ---- test harness (not shown in the docs) --------------------------------
// A second vector stresses an *overlapping* run: 32×0xE5 packs to a 1-byte
// literal + a copy at offset 1 (classic RLE), which the byte-at-a-time VRAM
// read-back must reproduce exactly.
#define RUN_VRAM 0x3D00
static const u8 g_PackedRun[] = { 0x95, 0xE5, 0x70, 0x00, 0x08 };

__sfr __at(0x98) g_P98v;
__sfr __at(0x99) g_P99v;
static u8 vpeek(u16 a)
{
	u8 v;
	__asm di __endasm;
	g_P99v = (u8)a;
	g_P99v = (u8)((a >> 8) & 0x3F);
	// VDP read-settle before the data byte is valid. In C-BIOS's boot (text) mode
	// even a V9938 needs it, so settle unconditionally -- this is a test read, not
	// a hot loop.
	__asm
		nop
		nop
		nop
		nop
		nop
		nop
	__endasm;
	v = g_P98v;
	__asm ei __endasm;
	return v;
}

volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, ok = 1;

	unpack_to_vram();
	UnZX0_VRAM(g_PackedRun, RUN_VRAM, 32);

	if (vpeek(TILE_VRAM + 0) != 0x00) ok = 0;         // first tile byte
	if (vpeek(TILE_VRAM + 2) != 0x3C) ok = 0;         // a literal in the run
	if (vpeek(TILE_VRAM + 8) != vpeek(TILE_VRAM + 0)) ok = 0;  // row 2 == row 1
	for (i = 0; i < 32; i++)                           // overlapping run
		if (vpeek(RUN_VRAM + i) != 0xE5) ok = 0;

	R[1] = vpeek(TILE_VRAM + 2);    // 0x3C
	R[2] = vpeek(RUN_VRAM + 31);    // 0xE5 (last byte of the overlapping run)
	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
