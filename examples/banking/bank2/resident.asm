;; resident.asm -- RESIDENT bank (ROM segment 0), placed at 0x4000 via `-b _CODE=0x4000`.
;; Minimal proof of the ASCII-8 far-call mechanism. Contains the cart header,
;; the INIT entry C-BIOS calls, and a hand-rolled "far call" into a banked
;; segment. No cross-bank symbols: the banked entry (0x8001) and the mapper
;; register (0x7000 = ASCII-8 bank 2 select) are absolute constants.
;;
;; Self-check result bytes written to RAM at 0xE000 (read back by the harness):
;;   RES[0] = 0xA5 iff the whole sequence passed, else 0x00
;;   RES[1] = byte at 0x8000 BEFORE mapping   (expect 0x41 = 'A', reset seg 0)
;;   RES[2] = byte at 0x8000 AFTER  mapping   (expect 0xB4 = banked seg marker)
;;   RES[3] = value the BANKED code wrote      (expect 0x5A -> it really executed)
;;   RES[4] = byte at 0x8000 AFTER  restore   (expect 0x41 = 'A', back to seg 0)
;; (RES avoids the name `R`, which sdasz80 treats as the Z80 refresh register.)

	ADDR_BANK2 = 0x7000	; ASCII-8: write here selects the segment shown at 0x8000
	SEG_BANKED = 4		; ROM segment holding the banked routine
	WINDOW     = 0x8000	; ASCII-8 bank-2 window base
	BANK_ENTRY = 0x8001	; banked routine entry (0x8000 holds a 1-byte marker)
	RES        = 0xE000	; self-check result block in RAM

	.globl _mark_end
	.area _CODE

;; --- MSX ROM cartridge header (16 bytes) ---
_start:
	.db 0x41, 0x42		; "AB" signature
	.dw init		; INIT entry point (C-BIOS calls this)
	.dw 0			; STATEMENT
	.dw 0			; DEVICE
	.dw 0			; TEXT (BASIC)
	.db 0,0,0,0,0,0		; reserved -> header is exactly 0x10 bytes

;; --- INIT: runs the far-call proof ---
init:
	;; C-BIOS calls INIT with only PAGE 1 (0x4000) pointing at our cart slot.
	;; The banked window lives in PAGE 2 (0x8000), which still shows another slot.
	;; Copy page 1's primary slot into page 2's field (port 0xA8) so 0x8000 reads
	;; the cart. (Non-expanded-slot path; adequate for the C-BIOS proof machine.)
	in a,(0xA8)
	ld b,a			; B = current primary slot register
	and #0x0C		; isolate page-1 slot (bits 3:2)
	rlca
	rlca			; shift into page-2 field (bits 5:4)
	ld c,a
	ld a,b
	and #0xCF		; clear old page-2 field
	or c			; page 2 := page 1's slot
	out (0xA8),a

	;; clear RES[0..5] so the dump is clean and RES[3] can only be set by banked code
	xor a
	ld (RES + 0),a
	ld (RES + 1),a
	ld (RES + 2),a
	ld (RES + 3),a
	ld (RES + 4),a
	ld (RES + 5),a

	;; RES[1] = window byte BEFORE mapping (reset state = segment 0 -> 'A')
	ld a,(WINDOW)
	ld (RES + 1),a

	;; --- MemSeg_Window(SEG_BANKED): map the banked segment into bank 2 ---
	;; (previous segment is 0 by reset; a real memseg would read it from the shadow)
	ld a,#SEG_BANKED
	ld (ADDR_BANK2),a

	;; RES[2] = window byte AFTER mapping (expect banked marker 0xB4)
	ld a,(WINDOW)
	ld (RES + 2),a

	;; --- far call into the banked routine (writes RES[3]=0x5A, then ret) ---
	call BANK_ENTRY

	;; --- MemSeg_Restore(0): put segment 0 back in bank 2 ---
	xor a
	ld (ADDR_BANK2),a

	;; RES[4] = window byte AFTER restore (expect 'A' again -> restore worked)
	ld a,(WINDOW)
	ld (RES + 4),a

	;; --- verdict: all three conditions must hold ---
	ld a,(RES + 2)
	cp #0xB4
	jr nz,fail
	ld a,(RES + 3)
	cp #0x5A
	jr nz,fail
	ld a,(RES + 4)
	cp #0x41
	jr nz,fail
	ld a,(RES + 5)		; set by _res_helper, called FROM the banked segment
	cp #0x3C
	jr nz,fail
	ld a,#0xA5		; PASS sentinel
	ld (RES + 0),a
	jr done
fail:
	xor a
	ld (RES + 0),a
done:
_mark_end:
	jr _mark_end		; spin so the harness can break here and dump RAM

;; --- resident helper, called by the BANKED routine (bank -> resident call) ---
;; Proves bankpack's symbol injection: the banked segment references this by
;; name and bankpack supplies its absolute address at link time. Page 1 is
;; always mapped, so a banked routine can always `call` into resident code.
	.globl _res_helper
_res_helper:
	ld a,#0x3C
	ld (RES + 5),a
	ret
