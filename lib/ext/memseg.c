// msxmvl EXTENSION module: memseg -- shadow-backed segment windowing.
// See memseg.h for the design rationale (write-only ports => mandatory shadow).

#include "memseg.h"

#ifdef MEMSEG_BACKEND_RAM

// Page-2 RAM-mapper select port. Writing a segment number here pages that 16 KB
// segment into 0x8000-0xBFFF. Write-only in hardware -> the shadow below is the
// only readable record of the current mapping.
__sfr __at(0xFE) g_MemSeg_PortP2;

// The shadow: current segment in the window page. Lives in resident RAM.
static MemSeg s_window;

static void hal_set(MemSeg seg)
{
	g_MemSeg_PortP2 = seg;   // SDCC emits `out (0xFE),a`
}

#else // MEMSEG_BACKEND_ROM8 (reserved)

// ASCII-8 bank-2 select is a WRITE to ROM address 0x7000 (not an I/O port).
static MemSeg s_window;

static void hal_set(MemSeg seg)
{
	*(volatile u8*)0x7000 = seg;
}

#endif

void MemSeg_Init(MemSeg home)
{
	s_window = home;
	hal_set(home);
}

MemSeg MemSeg_GetWindow(void)
{
	return s_window;
}

void MemSeg_SetWindow(MemSeg seg)
{
	s_window = seg;
	hal_set(seg);
}

MemSeg MemSeg_Window(MemSeg seg)
{
	MemSeg prev = s_window;   // readback via shadow -- hardware cannot tell us
	s_window = seg;
	hal_set(seg);
	return prev;
}

void MemSeg_Restore(MemSeg prev)
{
	s_window = prev;
	hal_set(prev);
}
