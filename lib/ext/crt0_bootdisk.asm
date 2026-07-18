;; crt0_bootdisk.asm — msxmvl startup for a self-booting-disk payload.
;;
;; Sibling of crt0_dos.asm: same 0x0100 TPA model, same C startup contract (zero
;; the uninitialised globals, copy the initialised ones), so an existing DOS
;; example relinks against this unchanged. Two deltas, because there is no MSX-DOS
;; on the disk — the disk-ROM kernel booted us and jumped straight here:
;;
;;   1. There is no `JP <BDOS>` at 0x0005 (DOS installs that; nothing did here).
;;      The kernel BDOS is live at 0xF37D, so we install `JP 0xF37D` at 0x0005 —
;;      then every FCB call in lib/gen/dos.c and every DiskDOS_/DiskOS_ sector
;;      read (which all `call 0x0005`) works with no DOS present.
;;   2. main() returning cannot "terminate to DOS" — there is none. It soft-resets
;;      the machine instead (jp 0x0000, the disk-ROM reboot vector), which re-runs
;;      the boot disk.
;;
;; The disk boot sector (lib/ext/bootsector.asm) already set SP high and loaded us
;; to 0x0100; we just re-assert a known stack and get on with it.
;;
;; Link EXACTLY like a .COM:  --code-loc 0x0100 --data-loc 0 --no-std-crt0,
;; this object FIRST so execution begins at 0x0100.

	.module crt0_bootdisk
	.globl	_main

	.globl	s__DATA, l__DATA
	.globl	s__INITIALIZER, l__INITIALIZER
	.globl	s__INITIALIZED

	KBDOS = 0xF37D          ; disk-ROM kernel BDOS entry (live post-boot)
	BDOSVE = 0x0005         ; the standard BDOS entry hook we synthesise

	.area _CODE

init:
	;; Stack at 0xE000: below the disk-ROM kernel's high work area AND above the
	;; payload ceiling (the boot loader tops out the TPA at 0xBEFF). This margin
	;; is load-bearing -- a stack up in 0xF000+ collides with the kernel work RAM
	;; and the very BDOS calls we route through 0xF37D crash (proven on the 8245).
	ld	sp, #0xE000

	;; ------------------------------------------------------------------
	;; 0. synthesise the BDOS entry hook: `JP 0xF37D` at 0x0005 so the FCB
	;;    API (dos.c) and DiskDOS_/DiskOS_ sector reads run without DOS.
	ld	a, #0xC3        ; JP opcode
	ld	(BDOSVE), a
	ld	hl, #KBDOS
	ld	(BDOSVE + 1), hl

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

	;; No DOS to return to: soft-reset through the disk-ROM reboot vector,
	;; which re-runs the boot disk.
	jp	0x0000

;; ---------------------------------------------------------------------------
;; Area order (matches crt0_dos): _INITIALIZER travels with the code (it is part
;; of the loaded image); _INITIALIZED is the RAM it is copied into.
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
