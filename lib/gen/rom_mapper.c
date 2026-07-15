// msxmvl clean-room reimplementation of MSXgl "rom_mapper" module.
// Toolchain: SDCC --sdcccall 1.
//
// ASCII-8 ROM mapper: four 8 KB banks span 4000h-BFFFh. A segment is selected
// by writing its number to the bank's write-only switch address:
//   bank 0 -> 6000h, bank 1 -> 6800h, bank 2 -> 7000h, bank 3 -> 7800h.
// Because the switch registers are write-only on hardware, GET_* returns a RAM
// shadow (g_Bank0Segment) that every SET_* keeps in sync. This mirrors the
// documented MSXgl inline bodies (Poke to ADDR_BANK_n + update g_Bank0Segment).
//
// The header declares NO __PRESERVES() contract for any of these (they are
// `inline` in MSXgl), so they use the standard SDCC convention. Written in
// plain C; the register footprint of an inline expansion is not part of the
// contract.

#include "rom_mapper.h"

//=============================================================================
// GLOBALS
//=============================================================================

// RAM shadow of the four write-only bank switch registers.
u8 g_Bank0Segment[MAPPER_BANKS];

//=============================================================================
// HELPERS
//=============================================================================

// Poke: write a byte to an absolute address (matches MSXgl system.h Poke()).
static void Poke(u16 addr, u8 val)
{
	*(volatile u8*)addr = val;
}

//=============================================================================
// SET
//=============================================================================

void SET_BANK_SEGMENT(u8 b, u8 s)
{
	g_Bank0Segment[b] = s;
	if (b == 0)
		Poke(ADDR_BANK_0, s);
	else if (b == 1)
		Poke(ADDR_BANK_1, s);
	else if (b == 2)
		Poke(ADDR_BANK_2, s);
	else if (b == 3)
		Poke(ADDR_BANK_3, s);
}

void SET_BANK_SEGMENT_LOW(u8 b, u8 s)
{
	SET_BANK_SEGMENT(b, s);
}

void SET_BANK_SEGMENT_HIGH(u8 b, u8 s)
{
	// No high byte for an 8-bit mapper.
	(void)b;
	(void)s;
}

//=============================================================================
// GET
//=============================================================================

u8 GET_BANK_SEGMENT(u8 b)
{
	return g_Bank0Segment[b];
}

u8 GET_BANK_SEGMENT_LOW(u8 b)
{
	return GET_BANK_SEGMENT(b);
}

u8 GET_BANK_SEGMENT_HIGH(u8 b)
{
	(void)b;
	return 0;
}
