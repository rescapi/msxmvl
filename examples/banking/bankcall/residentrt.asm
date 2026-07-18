;; residentrt.asm -- RESIDENT bank (segment 0) for the bank->bank call proof.
;; Contains: cart header + INIT, the far-call THUNKS, the resident far-call
;; JP-(IX) helper, the DISPATCH TABLE (patched by bankpack), and the RAM shadow.
;;
;; Demo: main calls farA(5) [segment 5]; farA calls farB(6) [segment 6] via the
;; resident thunk; the nesting must restore segment 5 so farA survives farB's
;; call. Expected result farA(5) = (5+1+100)+10 = 116 (0x74).
;;
;; Thunk convention (SDCC --sdcccall 1): 16-bit arg in HL, 16-bit return in DE.
;; The thunk only uses A / IX / the stack, so HL (arg) and DE (return) pass
;; through untouched. Previous segment is saved on the CPU stack -> nesting-safe.
;;
;;   RES[0] = 0xA5 iff farA(5) == 116, else 0x00
;;   RES[1] = low byte of the result (expect 0x74)
;;   RES[2] = high byte of the result (expect 0x00)

	ADDR_BANK2 = 0x7000	; ASCII-8 bank-2 select (segment shown at 0x8000)
	SEG_A      = 5		; segment holding farA
	SEG_B      = 6		; segment holding farB
	HOME       = 0		; window home segment (restored to at top level)
	SHADOW     = 0xE020	; RAM byte: current window segment (memseg shadow)
	RES        = 0xE000	; result block

	.globl _call_farA
	.globl _call_farB
	.globl _fartab
	.globl _mark_end
	.area _CODE

_start:
	.db 0x41, 0x42		; "AB"
	.dw init
	.dw 0
	.dw 0
	.dw 0
	.db 0,0,0,0,0,0

init:
	;; make the page-2 window visible: page2 slot := page1 (cart) slot
	in a,(0xA8)
	ld b,a
	and #0x0C
	rlca
	rlca
	ld c,a
	ld a,b
	and #0xCF
	or c
	out (0xA8),a

	;; MemSeg_Init(HOME): shadow + hardware agree
	ld a,#HOME
	ld (SHADOW),a
	ld (ADDR_BANK2),a

	xor a
	ld (RES + 0),a
	ld (RES + 1),a
	ld (RES + 2),a

	;; result = farA(5)   (resident -> bank A -> bank B, nested)
	ld hl,#5
	call _call_farA
	ld a,e			; return is in DE
	ld (RES + 1),a
	ld a,d
	ld (RES + 2),a

	;; verdict: DE == 116 (0x0074)
	ld a,e
	cp #0x74
	jr nz,fail
	ld a,d
	or a
	jr nz,fail
	ld a,#0xA5
	ld (RES + 0),a
	jr done
fail:
	xor a
	ld (RES + 0),a
done:
_mark_end:
	jr _mark_end

;; ------------------------------------------------------------------
;; Far-call thunks. arg in HL, return in DE (both untouched). Previous
;; segment pushed on the stack for nesting; target address read from the
;; dispatch table (patched post-link -> resident never needs a bank address).
;; ------------------------------------------------------------------
_call_farA:
	ld a,(SHADOW)		; A = previous segment
	push af			; save it (nesting-safe)
	ld a,#SEG_A
	ld (SHADOW),a
	ld (ADDR_BANK2),a	; map segment A into the window
	ld ix,(_fartab + 0)	; IX = &farA (patched by bankpack)
	call _far_jp_ix		; run farA(HL); returns DE; comes back here
	pop af			; A = previous segment
	ld (SHADOW),a
	ld (ADDR_BANK2),a	; restore it
	ret

_call_farB:
	ld a,(SHADOW)
	push af
	ld a,#SEG_B
	ld (SHADOW),a
	ld (ADDR_BANK2),a
	ld ix,(_fartab + 2)	; IX = &farB (patched by bankpack)
	call _far_jp_ix
	pop af
	ld (SHADOW),a
	ld (ADDR_BANK2),a
	ret

_far_jp_ix:
	jp (ix)			; "call" the address in IX (return lands back in the thunk)

;; ------------------------------------------------------------------
;; Dispatch table: slot 0 = &farA, slot 1 = &farB. Zero here; bankpack
;; patches the real run-addresses into the ROM image after linking the banks.
;; ------------------------------------------------------------------
_fartab:
	.dw 0			; slot 0 -> _farA
	.dw 0			; slot 1 -> _farB
