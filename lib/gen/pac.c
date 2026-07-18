// msxmvl "pac" module — ORIGINAL implementation (see pac.h for the interface
// provenance). A thin layer over lib/ext/sram.c's shared PAC core: the slot
// scan, the lock protocol and the per-byte interslot loops live THERE, once —
// this file only adds MSXgl's page carve, multi-device bookkeeping and the
// optional 4-byte signature. Link pac.c WITH sram.c (default PAC backend).
#include "pac.h"
#include "sram.h"

#ifndef SRAM_BACKEND_PAC
#error "pac.c needs sram.c's PAC backend (do not define SRAM_BACKEND_MAPPER for this pair)"
#endif

u8 g_PAC_Num = 0;
u8 g_PAC_Slot[PAC_DEVICE_MAX];
u8 g_PAC_Current = SLOT_NOTFOUND;

#if (PAC_USE_SIGNATURE)
static const c8 g_PAC_Sign[5] = APPSIGN;   // 4-char app signature (+ the literal's NUL)
#define PAC_SIGN_BYTES  4u                 // only the 4 characters go to the SRAM
#else
#define PAC_SIGN_BYTES  0u
#endif

// Page geometry: 1 KB pages at 0x4000 + page*1024. The LAST page loses its
// final 2 bytes to the lock registers (0x5FFE/0x5FFF) — writing those would
// relock (or worse, unlock) the cartridge, so they are never part of a page.
static u16 pac_base(u8 page) { return 0x4000u + ((u16)page << 10); }
static u16 pac_room(u8 page) { return (page == PAC_PAGE_MAX - 1) ? 1022u : 1024u; }

#if (PAC_USE_VALIDATOR)
static u8 pac_bad(u8 page) { return (page >= PAC_PAGE_MAX) || (g_PAC_Current == SLOT_NOTFOUND); }
#else
#define pac_bad(page) 0
#endif

bool PAC_Initialize()
{
	g_PAC_Num = Sram_PacScan(g_PAC_Slot, PAC_DEVICE_MAX);
	g_PAC_Current = g_PAC_Num ? g_PAC_Slot[0] : SLOT_NOTFOUND;
	return g_PAC_Num != 0;
}

void PAC_Activate(bool bEnable)
{
	Sram_PacActivate(g_PAC_Current, bEnable);
}

void PAC_Write(u8 page, const u8* data, u16 size)
{
	u16 room;
	if (pac_bad(page))
		return;
	room = pac_room(page) - PAC_SIGN_BYTES;
	if (size > room)
		size = room;
#if (PAC_USE_SIGNATURE)
	Sram_PacWrite(g_PAC_Current, pac_base(page), (const u8*)g_PAC_Sign, PAC_SIGN_BYTES);
#endif
	Sram_PacWrite(g_PAC_Current, pac_base(page) + PAC_SIGN_BYTES, data, size);
}

void PAC_Read(u8 page, u8* data, u16 size)
{
	u16 room;
	if (pac_bad(page))
		return;
	room = pac_room(page) - PAC_SIGN_BYTES;
	if (size > room)
		size = room;
	Sram_PacRead(g_PAC_Current, pac_base(page) + PAC_SIGN_BYTES, data, size);
}

void PAC_Format(u8 page)
{
	if (pac_bad(page))
		return;
	Sram_PacFill(g_PAC_Current, pac_base(page), PAC_EMPTY_CHAR, pac_room(page));
}

u8 PAC_Check(u8 page)
{
	if (pac_bad(page))
		return PAC_CHECK_ERROR;
#if (PAC_USE_SIGNATURE)
	{
		u8 sig[PAC_SIGN_BYTES];
		Sram_PacRead(g_PAC_Current, pac_base(page), sig, PAC_SIGN_BYTES);
		if (sig[0] == g_PAC_Sign[0] && sig[1] == g_PAC_Sign[1] &&
		    sig[2] == g_PAC_Sign[2] && sig[3] == g_PAC_Sign[3])
			return PAC_CHECK_APP;
	}
#endif
	if (Sram_PacIsAll(g_PAC_Current, pac_base(page), PAC_EMPTY_CHAR, pac_room(page)))
		return PAC_CHECK_EMPTY;
	return PAC_CHECK_UNDEF;
}
