;; crt0_rom48.asm — msxmvl startup for a 48 KB MSX cartridge ROM at 0x0000-0xBFFF.
;;
;; Pages 0, 1 AND 2 are cartridge ROM. The BIOS boots us through the page-1
;; header as usual; this crt0 then enables the cart's slot in page 2 (ENASLT)
;; and finally in PAGE 0 — after which THE BIOS IS GONE. That is the contract:
;;
;;   *** After boot there are NO BIOS calls, ever. ***
;;   The library drives hardware directly anyway (VDP/PSG/PPI), so most msxmvl
;;   modules are fine; anything documented as calling the BIOS is not.
;;
;; Interrupts: with page 0 ours, the IM1 vector 0x0038 is ours too. It jumps
;; through a 3-byte RAM slot (_g_ISRJump: jp <handler>) so a program can retarget
;; the interrupt by rewriting one pointer — no BIOS ISR overhead at all. The
;; default handler only acknowledges the VDP (reads S#0) and returns.
;;
;; Page-0 slot switching is done by writing the primary slot register (0xA8)
;; directly: ENASLT itself executes from page 0 and cannot be used to replace
;; the page it is running from. Direct write handles NON-EXPANDED cart slots
;; (the common case, and what openMSX's carta emulates); an expanded-slot cart
;; would need the 0xFFFF secondary-register dance with page 3 switched — the
;; same unverified territory farrt.asm documents.
;;
;; Link with:  --code-loc 0x4000 --data-loc 0xC000 --no-std-crt0, this object
;; FIRST. Build with makebin -s 49152 and use the whole image (no 16 KB skip).
;; Page 0 below 0x4000 is free for __at const data (0x0040-0x3FFF).

	.module crt0_rom48
	.globl	_main
	.globl	_g_ISRJump

	.globl	s__DATA, l__DATA
	.globl	s__INITIALIZER, l__INITIALIZER
	.globl	s__INITIALIZED

	HIMEM  = 0xFC4A         ; BIOS: top of free RAM (read BEFORE page 0 goes)
	ENASLT = 0x0024         ; BIOS: enable slot -- usable for pages 1-3 only here
	EXPTBL = 0xFCC1
	SLTTBL = 0xFCC5
	VDP_S  = 0x99           ; VDP status port: reading S#0 acks the interrupt

;; ---------------------------------------------------------------------------
;; Page 0: reset stub + interrupt vector. Absolute area — everything else in
;; this file links normally at --code-loc.
	.area _P0VEC (ABS)
	.org	0x0000
	jp	crt0_halt       ; a stray rst 0 parks instead of running off

	.org	0x0038
	jp	_g_ISRJump      ; IM1 vector -> RAM jp -> current handler

;; ---------------------------------------------------------------------------
	.area _CODE

	.db	0x41, 0x42      ; "AB" — page-1 header the BIOS boots through
	.dw	init
	.dw	0x0000
	.dw	0x0000
	.dw	0x0000
	.db	0,0,0,0,0,0

init:
	di
	ld	sp, (HIMEM)

	;; ------------------------------------------------------------------
	;; 1. enable the cart's slot in page 2 (same sequence as crt0_rom32)
	in	a, (0xA8)
	rrca
	rrca
	and	#0x03
	ld	e, a            ; E = cart primary slot PP
	ld	d, #0
	ld	hl, #EXPTBL
	add	hl, de
	ld	a, (hl)
	and	#0x80
	or	e
	ld	b, a
	and	#0x80
	jr	z, slot_ready
	ld	hl, #SLTTBL
	add	hl, de
	ld	a, (hl)
	rrca
	rrca
	and	#0x03
	rlca
	rlca
	or	b
	ld	b, a
slot_ready:
	ld	a, b
	ld	h, #0x80        ; page 2
	call	ENASLT
	di                      ; ENASLT may re-enable interrupts

	;; ------------------------------------------------------------------
	;; 2. C startup contract: zero _DATA, copy _INITIALIZER -> _INITIALIZED
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
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, no_init
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
no_init:

	;; ------------------------------------------------------------------
	;; 3. arm the RAM interrupt jp BEFORE page 0 switches
	ld	a, #0xC3        ; jp opcode
	ld	(_g_ISRJump), a
	ld	hl, #default_isr
	ld	(_g_ISRJump+1), hl

	;; ------------------------------------------------------------------
	;; 4. page 0 <- the cart's slot, by direct primary-slot-register write
	;;    (non-expanded slots; see header). Self-contained: the cart's primary
	;;    slot is re-read from the page-1 field of RSLREG right here, because
	;;    the init code above has long since recycled every register.
	;;    The BIOS is gone from here on.
	in	a, (0xA8)
	ld	b, a
	rrca
	rrca
	and	#0x03           ; page-1 field = the cart's primary slot
	ld	c, a
	ld	a, b
	and	#0xFC           ; clear the page-0 field (bits 1:0)
	or	c
	out	(0xA8), a

	im	1
	ei
	call	_main

crt0_halt:
	jr	crt0_halt

;; ---------------------------------------------------------------------------
;; Default interrupt handler: acknowledge the VDP and return. Retarget by
;; writing a new handler address at _g_ISRJump+1 (or use the isr module).
default_isr:
	push	af
	in	a, (VDP_S)      ; read S#0: clears the VDP interrupt line
	pop	af
	ei
	reti

;; ---------------------------------------------------------------------------
;; Area order (see crt0_rom16.asm): initialisers in ROM, data in RAM.
	.area _HOME
	.area _CODE
	.area _INITIALIZER
	.area _GSINIT
	.area _GSFINAL

	.area _DATA
_g_ISRJump::
	.ds	3               ; jp <handler> — the live interrupt indirection
	.area _INITIALIZED
	.area _BSEG
	.area _BSS
	.area _HEAP
