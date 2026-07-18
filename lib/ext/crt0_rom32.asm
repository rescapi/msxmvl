;; crt0_rom32.asm — msxmvl startup for a 32 KB MSX cartridge ROM at 0x4000-0xBFFF.
;;
;; crt0_rom16 plus one extra duty: at boot the BIOS enables the cartridge's slot
;; in PAGE 1 only (where it found the "AB" header). The ROM's second half lives
;; in page 2 (0x8000-0xBFFF), so this crt0 enables the cart's own slot there too,
;; via BIOS ENASLT — the same slot-ID construction farrt.asm uses for banked
;; ROMs (verified for non-expanded cart slots; the expanded path is by-the-book
;; but unverified on a real BIOS, see farrt.asm's header note).
;;
;; Link with:  --code-loc 0x4000 --data-loc 0xC000 --no-std-crt0
;; and pass this object FIRST, so the cartridge header lands exactly at 0x4000.
;; Build the image with makebin -s 49152 and drop the first 16 KB.

	.module crt0_rom32
	.globl	_main

	.globl	s__DATA, l__DATA
	.globl	s__INITIALIZER, l__INITIALIZER
	.globl	s__INITIALIZED

	HIMEM  = 0xFC4A         ; BIOS: top of free RAM
	ENASLT = 0x0024         ; BIOS: enable slot (A=ExxxSSPP) at page (H bits 7:6)
	EXPTBL = 0xFCC1         ; EXPTBL[pp] bit7 => primary slot pp is expanded
	SLTTBL = 0xFCC5         ; per-primary mirror of the 0xFFFF secondary register

;; ---------------------------------------------------------------------------
;; Cartridge header — first bytes of the ROM, inside _CODE so --code-loc places it.
	.area _CODE

	.db	0x41, 0x42      ; "AB"
	.dw	init
	.dw	0x0000          ; STATEMENT / DEVICE / TEXT: unused
	.dw	0x0000
	.dw	0x0000
	.db	0,0,0,0,0,0     ; reserved

init:
	di
	ld	sp, (HIMEM)

	;; ------------------------------------------------------------------
	;; 1. make the ROM's second half visible: enable the cart's page-1 slot
	;;    in page 2 as well. Build the ExxxSSPP slot ID, then ENASLT (which
	;;    corrupts all registers — nothing is live yet).
	in	a, (0xA8)
	rrca
	rrca                    ; page-1 field -> bits 1:0
	and	#0x03
	ld	e, a            ; E = primary slot PP
	ld	d, #0
	ld	hl, #EXPTBL
	add	hl, de
	ld	a, (hl)
	and	#0x80           ; expanded?
	or	e
	ld	b, a            ; B = slot ID so far (E00000PP)
	and	#0x80
	jr	z, slot_ready
	ld	hl, #SLTTBL     ; expanded: page-1 secondary slot from SLTTBL[PP]
	add	hl, de
	ld	a, (hl)
	rrca
	rrca                    ; page-1 secondary field -> bits 1:0
	and	#0x03
	rlca
	rlca                    ; -> SS position (bits 3:2)
	or	b
	ld	b, a
slot_ready:
	ld	a, b
	ld	h, #0x80        ; page 2
	call	ENASLT
	di                      ; ENASLT may leave interrupts enabled

	;; ------------------------------------------------------------------
	;; 2. zero the uninitialised globals (_DATA)
	ld	bc, #l__DATA
	ld	a, b
	or	a, c
	jr	Z, no_bss
	ld	hl, #s__DATA
	ld	(hl), #0x00
	ld	d, h
	ld	e, l
	inc	de
	dec	bc
	ld	a, b
	or	a, c
	jr	Z, no_bss
	ldir
no_bss:

	;; ------------------------------------------------------------------
	;; 3. copy the initialised globals from ROM into RAM
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, no_init
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
no_init:

	ei
	call	_main

crt0_halt:
	jr	crt0_halt

;; ---------------------------------------------------------------------------
;; Area order (see crt0_rom16.asm): initialisers in ROM, data in RAM.
	.area _HOME
	.area _CODE
	.area _INITIALIZER      ; <- ROM: the values
	.area _GSINIT
	.area _GSFINAL

	.area _DATA             ; <- RAM (--data-loc)
	.area _INITIALIZED      ; <- RAM: where the values are copied to
	.area _BSEG
	.area _BSS
	.area _HEAP
