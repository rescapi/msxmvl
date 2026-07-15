;; crt0_dos.asm — msxmvl startup for an MSX-DOS .COM program.
;;
;; Replaces the vendored Fusion-C DOS crt0. A .COM is loaded at 0x0100 in the TPA
;; with the stack already set up by DOS, so there is less to do than for a ROM --
;; but the C startup contract is the same: zero the uninitialised globals, then
;; copy the initialised ones into place. Skip either and `static` variables are
;; quietly wrong.
;;
;; Link with:  --code-loc 0x0100 --data-loc 0 --no-std-crt0
;; and pass this object FIRST, so execution begins at 0x0100.

	.module crt0_dos
	.globl	_main

	.globl	s__DATA, l__DATA
	.globl	s__INITIALIZER, l__INITIALIZER
	.globl	s__INITIALIZED

	BDOS = 0x0005

	.area _CODE

init:
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
	;; 2. copy the initialised globals into place
	ld	bc, #l__INITIALIZER
	ld	a, b
	or	a, c
	jr	Z, no_init
	ld	de, #s__INITIALIZED
	ld	hl, #s__INITIALIZER
	ldir
no_init:

	call	_main

	;; Unlike a ROM, a .COM *can* return: hand control back to MSX-DOS.
	ld	c, #0x00        ; BDOS 0x00 = terminate program
	jp	BDOS

;; ---------------------------------------------------------------------------
;; Area order: _INITIALIZER must sit with the code (it is part of the loaded
;; image), and _INITIALIZED is the RAM it gets copied into.
	.area _HOME
	.area _CODE
	.area _INITIALIZER
	.area _GSINIT
	.area _GSFINAL

	.area _DATA
	.area _INITIALIZED
	.area _BSEG
	.area _BSS
	.area _HEAP
