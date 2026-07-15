// msxmvl clean-room reimplementation of MSXgl "memory_mapper" module.
// Toolchain: SDCC --sdcccall 1.
//
// Each MemMap_SetPageN routine writes the requested 16 KB segment number to the
// memory-mapper I/O port that governs that Z80 page:
//   page 0 -> 0xFC, page 1 -> 0xFD, page 2 -> 0xFE, page 3 -> 0xFF.
// MemMap_Initialize restores the canonical MSX boot mapping (segments 3,2,1,0
// in pages 0,1,2,3), matching the documented MSXgl inline body.
//
// The header declares NO __PRESERVES() contract for any of these (they are
// `inline` in MSXgl), so they use the standard SDCC convention. Written in
// plain C so SDCC emits the single `out (port),a` per store; the register
// footprint of an inline expansion is not part of the contract.

#include "memory_mapper.h"

//=============================================================================
// PAGE SEGMENT SELECTION
//=============================================================================

void MemMap_SetPage0(u8 seg)
{
	g_PortMemPage0 = seg;
}

void MemMap_SetPage1(u8 seg)
{
	g_PortMemPage1 = seg;
}

void MemMap_SetPage2(u8 seg)
{
	g_PortMemPage2 = seg;
}

void MemMap_SetPage3(u8 seg)
{
	g_PortMemPage3 = seg;
}

//=============================================================================
// INITIALIZE
//=============================================================================

void MemMap_Initialize(void)
{
	MemMap_SetPage3(0);
	MemMap_SetPage2(1);
	MemMap_SetPage1(2);
	MemMap_SetPage0(3);
}
