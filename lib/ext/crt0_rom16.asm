;; crt0_rom16.asm — msxmvl startup for a 16 KB MSX cartridge ROM at 0x4000.
;;
;; Replaces the vendored Fusion-C crt0, which did NEITHER half of the C startup
;; contract: it never zeroed _DATA (so `static u8 n;` began as RAM garbage) and
;; never copied _INITIALIZER -> _INITIALIZED (so `static u8 x = 5;` read back as
;; garbage). It did not even DECLARE _INITIALIZER, so the linker parked it after
;; _DATA -- in RAM, where the initialiser bytes were not part of the ROM image at
;; all. This crt0 does both, and declares the areas in an order that puts the
;; initialisers in ROM where they belong.
;;
;; Link with:  --code-loc 0x4000 --data-loc 0xC000 --no-std-crt0
;; and pass this object FIRST, so the cartridge header lands exactly at 0x4000.

	.module crt0_rom16
	.globl	_main

	;; Linker-defined area bounds. In a standalone .asm these must be declared
	;; external explicitly -- sdasz80 will not guess.
	.globl	s__DATA, l__DATA
	.globl	s__INITIALIZER, l__INITIALIZER
	.globl	s__INITIALIZED

	HIMEM = 0xFC4A          ; BIOS: top of free RAM

;; ---------------------------------------------------------------------------
;; The cartridge header must be the very first thing in the ROM. It lives inside
;; _CODE (rather than its own area) so that --code-loc alone places it, with no
;; extra linker flags to get wrong.
	.area _CODE

	.db	0x41, 0x42      ; "AB" — the magic the BIOS looks for
	.dw	init            ; INIT:      called at boot
	.dw	0x0000          ; STATEMENT: unused
	.dw	0x0000          ; DEVICE:    unused
	.dw	0x0000          ; TEXT:      unused
	.db	0,0,0,0,0,0     ; reserved

init:
	di
	ld	sp, (HIMEM)     ; stack at the top of free RAM

	;; ------------------------------------------------------------------
	;; 1. zero the uninitialised globals (_DATA)
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
	;; 2. copy the initialised globals from ROM into RAM
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

	;; main() is not supposed to return -- there is no OS to return to.
	;; If it does, spin rather than run off into RAM.
	;; NOT called _mark_end: that name is the test harness's breakpoint for "the
	;; example finished", and examples end in their own for(;;) inside main().
crt0_halt:
	jr	crt0_halt

;; ---------------------------------------------------------------------------
;; Area order. The linker lays areas out in the order it first sees them, so this
;; block is what decides that _INITIALIZER ends up in ROM (right after the code)
;; while _INITIALIZED / _DATA sit in RAM at --data-loc.
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
