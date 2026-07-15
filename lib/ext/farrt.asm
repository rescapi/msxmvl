;; farrt.asm -- reusable RESIDENT runtime (crt0) for a banked ASCII-8 ROM.
;;
;; Provides the cart header, INIT (slot-select so the page-2 window is visible +
;; memseg shadow init), the far-call JP-(IX) helper, and the spin the test
;; harness breaks on. Links FIRST in the resident bank so the header sits at
;; 0x4000. The user's C `main` and the bankpack-generated far thunks link after.
;;
;; Shared with the generated thunks by convention (ASCII-8 backend):
;;   SHADOW     = 0xE020  RAM byte: current segment in the page-2 window
;;   ADDR_BANK2 = 0x7000  ASCII-8 bank-2 select
;;   HOME       = 0       segment restored to at the top level
;;
;; Slot handling: INIT builds the cart's ExxxSSPP slot ID (primary from RSLREG,
;; and secondary from SLTTBL when EXPTBL marks the primary expanded) and calls
;; BIOS ENASLT to enable it in page 2. VERIFIED for non-expanded cart slots
;; (openMSX C-BIOS, the common case for external cartridges). The expanded-cart
;; path is by-the-book but UNVERIFIED: under C-BIOS's emulated expanded config
;; ENASLT mismapped the secondary slot, and full validation needs a real-BIOS
;; machine. See far-memory-banking notes.

	ADDR_BANK2 = 0x7000
	SHADOW     = 0xE020
	HOME       = 0

	ENASLT = 0x0024		; BIOS: enable slot (A=ExxxSSPP) at page (H bits 7:6)
	EXPTBL = 0xFCC1		; 4 bytes: EXPTBL[pp] bit7 => primary slot pp is expanded
	SLTTBL = 0xFCC5		; 4 bytes: per-primary mirror of the 0xFFFF secondary reg

	.globl _main		; user entry (C)
	.globl _mark_end	; harness breakpoint
	.area _CODE

_start:
	.db 0x41, 0x42		; "AB"
	.dw init
	.dw 0
	.dw 0
	.dw 0
	.db 0,0,0,0,0,0

init:
	;; Make the page-2 window visible by enabling the cartridge's own slot (the
	;; one mapped in page 1) into page 2, via ENASLT -- so it works whether or
	;; not the cart's primary slot is expanded. Build the ExxxSSPP slot ID for
	;; page 1, then ENASLT it at page 2 (H=0x80). ENASLT corrupts all registers,
	;; which is fine here: nothing is live yet at INIT entry.

	;; primary slot of page 1 = bits 3:2 of the primary slot register (RSLREG)
	in a,(0xA8)
	rrca
	rrca			; page-1 field -> bits 1:0
	and #0x03
	ld e,a			; E = primary slot PP (0..3)
	ld d,#0

	;; expanded? EXPTBL[PP] bit7
	ld hl,#EXPTBL
	add hl,de
	ld a,(hl)
	and #0x80		; expanded flag
	or e			; A = E00000PP (expanded flag + primary slot)
	ld b,a			; B = slot ID (secondary bits still 0)
	and #0x80
	jr z,slot_ready		; not expanded -> B is complete

	;; expanded: page-1 secondary slot from SLTTBL[PP] (mirror of 0xFFFF)
	ld hl,#SLTTBL
	add hl,de
	ld a,(hl)
	rrca
	rrca			; page-1 secondary field -> bits 1:0
	and #0x03
	rlca
	rlca			; secondary -> SS position (bits 3:2)
	or b			; merge into slot ID
	ld b,a
slot_ready:
	ld a,b			; A = ExxxSSPP for the cart slot
	ld h,#0x80		; page 2 (bits 7:6 = 10)
	call ENASLT

	;; MemSeg_Init(HOME): shadow and hardware agree
	ld a,#HOME
	ld (SHADOW),a
	ld (ADDR_BANK2),a

	call _main		; run the program; results left in RAM
_mark_end:
	jr _mark_end		; spin so the harness can read RAM
