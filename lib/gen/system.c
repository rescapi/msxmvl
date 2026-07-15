// msxmvl clean-room reimplementation of MSXgl "system" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/L(8b), arg3+ -> stack.
//   return 8b->A, 16b->HL.
//
// The header declares no __PRESERVES() contract for the slot routines, so they
// follow the standard SDCC convention (AF/BC/DE/HL/IX caller-clobberable, IY
// untouched). The slot switching is DI-protected and, while page 3 is
// temporarily repointed to read/write the 0xFFFF secondary-slot register, the
// code performs no stack access -- so a stack living in page 3 RAM survives the
// switch and the final RET lands correctly.

#include "system.h"

//=============================================================================
// GLOBALS
//=============================================================================

// CRT0 program-extent markers. Real MSXgl gets these from crt0/linker; for a
// standalone msxmvl build we provide storage so &g_FirstAddr etc. resolve.
// In an MSXgl build the crt0 owns these symbols and the compiler defines TARGET,
// so only emit our own storage when building standalone (no MSXgl crt0 present).
#ifndef TARGET
u16 g_FirstAddr;
u16 g_HeaderAddr;
u16 g_LastAddr;
#endif

// BIOS / system-work-area variables at fixed addresses. These MUST be defined
// (not merely declared extern) so SDCC emits the absolute-symbol assignment
// (_g_EXPTBL = 0xFCC1, ...). An `extern __at(addr)` declaration alone does NOT
// force absolute addressing under SDCC: the reference stays an undefined global
// that the linker resolves to 0x0000, so every read would hit address 0 instead
// of the intended BIOS variable.
const u8 __at(0xFCC1) g_EXPTBL[4]; // EXPTBL: per-primary-slot expanded flags
const u8 __at(0x002B) g_BASRVN[2]; // BASRVN: Main-ROM version / frequency bytes
const u8 __at(0x002D) g_MSXVER;    // MSXVER: MSX version number

//=============================================================================
// SLOT
//=============================================================================

// Sys_GetPageSlot(page=A) -> slot ID in A.
// Reads the PPI primary-slot register (0xA8); if that primary slot is flagged
// expanded in EXPTBL, reads its secondary-slot register (complement of 0xFFFF
// while the primary is paged into page 3).
u8 Sys_GetPageSlot(u8 page) __naked
{
	(void)page;
	__asm
		add  a, a               ; A = page * 2
		ld   b, a               ; B = shift (bit offset of this page field)
		in   a, (#0xA8)
		ld   c, b
		inc  c
	00001$:
		dec  c
		jr   z, 00002$
		rrca
		jr   00001$
	00002$:
		and  #0x03              ; primary slot number
		ld   d, a               ; D = prim
		ld   hl, #0xFCC1        ; g_EXPTBL
		add  a, l
		ld   l, a
		jr   nc, 00003$
		inc  h
	00003$:
		bit  7, (hl)            ; expanded flag for this primary slot?
		jr   nz, 00004$
		ld   a, d               ; not expanded -> return primary only
		ret
	00004$:
		di
		in   a, (#0xA8)
		ld   l, a               ; save current PPI value
		and  #0x3F              ; clear page-3 primary field
		ld   e, a
		ld   a, d
		rrca
		rrca                    ; prim << 6 (into page-3 field)
		or   e
		out  (#0xA8), a         ; page 3 -> prim (no stack access follows)
		ld   a, (#0xFFFF)
		cpl                     ; A = secondary-slot config for prim
		ld   e, a
		ld   a, l
		out  (#0xA8), a         ; restore original paging
		ei
		ld   a, e
		ld   c, b
		inc  c
	00005$:
		dec  c
		jr   z, 00006$
		rrca
		jr   00005$
	00006$:
		and  #0x03              ; secondary slot for this page
		rlca
		rlca                    ; sec << 2
		or   d                  ; | primary
		or   #0x80              ; | SLOT_EXP
		ret
	__endasm;
}

// Sys_SetPageSlot(page=A, slotId=L).
// Sets the primary field for 'page' in the PPI register, and when slotId has
// SLOT_EXP set also programs the secondary-slot register (0xFFFF) of that
// primary slot for the page.
void Sys_SetPageSlot(u8 page, u8 slotId) __naked
{
	(void)page; (void)slotId;
	__asm
		ld   e, l               ; E = slotId (stable copy)
		add  a, a               ; A = page * 2
		ld   b, a               ; B = shift
		; --- primary set-bits: (slotId & 3) << shift ---
		ld   a, e
		and  #0x03
		ld   c, b
		inc  c
	00001$:
		dec  c
		jr   z, 00002$
		add  a, a
		jr   00001$
	00002$:
		ld   d, a               ; D = primary set-bits
		; --- clear mask: ~(3 << shift) ---
		ld   a, #0x03
		ld   c, b
		inc  c
	00003$:
		dec  c
		jr   z, 00004$
		add  a, a
		jr   00003$
	00004$:
		cpl
		ld   h, a               ; H = clear mask
		di
		in   a, (#0xA8)
		and  h
		or   d
		ld   l, a               ; L = new PPI (primary applied); kept for restore
		out  (#0xA8), a
		bit  7, e               ; expanded?
		jr   z, 00009$
		; --- program secondary slot register ---
		ld   a, e
		and  #0x03
		rrca
		rrca                    ; prim << 6
		ld   d, a
		ld   a, l
		and  #0x3F
		or   d                  ; page 3 -> prim
		out  (#0xA8), a         ; (no stack access until restore)
		ld   a, (#0xFFFF)
		cpl                     ; current secondary config for prim
		and  h                  ; clear this page secondary field
		ld   d, a
		ld   a, e
		rrca
		rrca
		and  #0x03              ; secondary slot number
		ld   c, b
		inc  c
	00005$:
		dec  c
		jr   z, 00006$
		add  a, a
		jr   00005$
	00006$:
		or   d
		ld   (#0xFFFF), a       ; write new secondary config
		ld   a, l
		out  (#0xA8), a         ; restore original page-3 primary
	00009$:
		ei
		ret
	__endasm;
}

// Sys_SetPage0Slot(slotId=A) -> select slotId in page 0.
// Page-0 specialisation of Sys_SetPageSlot: the shift is a compile-time 0, so the
// two runtime shift loops and the page*2 multiply collapse away (clear mask =
// 0xFC, primary set-bits = slotId & 3). Byte-identical PPI / 0xFFFF writes to the
// former Sys_SetPageSlot(0, slotId) tail-call, just without the general-case work.
void Sys_SetPage0Slot(u8 slotId) __naked
{
	(void)slotId;
	__asm
		ld   e, a               ; E = slotId (stable copy)
		and  #0x03              ; primary slot number
		ld   d, a               ; D = primary set-bits (shift 0)
		di
		in   a, (#0xA8)
		and  #0xFC              ; clear page-0 primary field (~3)
		or   d
		out  (#0xA8), a         ; A still = new PPI value below
		bit  7, e               ; expanded?
		jr   z, 00009$
		ld   l, a               ; L = new PPI (kept for restore; only needed here)
		; --- program secondary slot register (page 0, shift 0) ---
		ld   a, e
		and  #0x03
		rrca
		rrca                    ; prim << 6
		ld   d, a
		ld   a, l
		and  #0x3F
		or   d                  ; page 3 -> prim
		out  (#0xA8), a         ; (no stack access until restore)
		ld   a, (#0xFFFF)
		cpl                     ; current secondary config for prim
		and  #0xFC              ; clear page-0 secondary field
		ld   d, a
		ld   a, e
		rrca
		rrca
		and  #0x03              ; secondary slot number (shift 0)
		or   d
		ld   (#0xFFFF), a       ; write new secondary config
		ld   a, l
		out  (#0xA8), a         ; restore original page-3 primary
	00009$:
		ei
		ret
	__endasm;
}

// Sys_CheckSlot(cb) -> first slot ID for which cb() returns TRUE, else 0xFF.
// Iterates every primary slot, expanding into its four secondary slots when the
// primary is flagged expanded in EXPTBL.
u8 Sys_CheckSlot(CheckSlotCallback cb)
{
	for (u8 slot = 0; slot < 4; ++slot)
	{
		if (g_EXPTBL[slot] & SLOT_EXP)
		{
			for (u8 sub = 0; sub < 4; ++sub)
			{
				u8 slotId = SLOTEX(slot, sub);
				if (cb(slotId))
					return slotId;
			}
		}
		else if (cb(slot))
			return slot;
	}
	return SLOT_NOTFOUND;
}
