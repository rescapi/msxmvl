// msxmvl clean-room reimplementation of MSXgl "bios_hook" module.
//
// Only BIOS_SetHookCallback is a real (non-inline) function; every other
// public entry point is an inline defined in bios_hook.h.
//
// BIOS_SetHookCallback installs a "safe" inter-slot hook: it writes an
// RST #30 (CALSLT) trampoline at the hook address, with the slot ID resolved
// to whichever slot currently pages in the callback's own address. That way
// the hook works even if the callback lives in a ROM/RAM slot that is not
// the one paged in at hook-call time.
//
// Slot-ID byte layout (as expected by RST #30 / CALSLT):
//   bit0-1 = primary slot, bit2-3 = secondary slot, bit7 = expanded flag.

#include "bios_hook.h"

// System work-area tables used to resolve the current slot configuration.
//   PSLREG   : primary slot select register (readable at I/O port 0xA8)
//   EXPTBL   : 0xFCC1 (4 bytes) - bit7 set => primary slot is expanded
//   SLTTBL   : 0xFCC5 (4 bytes) - mirror of each expanded slot's subslot reg

//-----------------------------------------------------------------------------
// Set a safe hook interslot jump to a given function.
//   sdcccall(1): HL = hook, DE = cb
void BIOS_SetHookCallback(u16 hook, callback cb) __naked
{
	hook; cb;
	__asm
		push	de			; save cb for the final write
		push	hl			; save hook address

		; --- page number of cb = (D >> 6) ---
		ld		a, d
		rlca
		rlca
		and		#0x03
		ld		b, a		; B = page (0..3), kept until secondary calc

		; --- primary slot of that page ---
		in		a, (#0xA8)	; A = primary slot select register (PSLREG)
		push	bc
		inc		b			; shift A right by page*2 bits
		jr		00002$
	00001$:
		rrca
		rrca
	00002$:
		dec		b
		jr		nz, 00001$
		pop		bc			; restore B = page
		and		#0x03
		ld		c, a		; C = primary slot (0..3)
		ld		e, a		; E = slot ID (primary in bits 0-1)

		; --- expanded? EXPTBL[primary] at 0xFCC1, bit7 ---
		ld		hl, #0xFCC1
		ld		a, c
		add		a, l
		ld		l, a
		ld		a, #0x00
		adc		a, h
		ld		h, a
		bit		7, (hl)
		jr		z, 00006$	; not expanded -> slot ID complete

		; --- expanded: add secondary slot + expanded flag ---
		set		7, e		; expanded flag (bit7)
		ld		hl, #0xFCC5	; SLTTBL[primary]
		ld		a, c
		add		a, l
		ld		l, a
		ld		a, #0x00
		adc		a, h
		ld		h, a
		ld		a, (hl)		; subslot config for this primary slot
		inc		b			; shift right by page*2 bits
		jr		00004$
	00003$:
		rrca
		rrca
	00004$:
		dec		b
		jr		nz, 00003$
		and		#0x03
		rlca
		rlca				; secondary slot into bits 2-3
		or		e
		ld		e, a		; E = full slot ID

	00006$:
		; --- write the RST #30 trampoline: F7, slot, cb_lo, cb_hi ---
		pop		hl			; HL = hook
		ld		(hl), #0xF7	; RST #30 (CALSLT)
		inc		hl
		ld		(hl), e		; slot ID
		inc		hl
		pop		de			; DE = cb
		ld		(hl), e		; cb low byte
		inc		hl
		ld		(hl), d		; cb high byte
		ret
	__endasm;
}
