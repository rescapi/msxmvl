;; banked.asm -- BANKED routine (ROM segment 4), placed at 0x8000 via `-b _CODE=0x8000`.
;; This is the code that only exists in address space once its segment is paged
;; into the ASCII-8 bank-2 window. It is reached via `call 0x8001` from the
;; resident bank. It proves it executed by writing a signature to RAM, then rets.
;;
;; Byte 0 of the segment (0x8000) is a distinctive marker so the resident code
;; can confirm the window really switched (reads 0xB4 here vs 0x41 for seg 0).

	RES = 0xE000		; shared self-check block in RAM

	.area _CODE

_bstart:
	.db 0xB4		; 0x8000: segment marker (not executed)

	.globl _res_helper	; resolved by bankpack from the resident bank's map

;; entry at 0x8001:
	ld a,#0x5A		; "banked code ran" signature
	ld (RES + 3),a		; -> RES[3]
	call _res_helper	; bank -> resident call (page 1 is always mapped)
	ret
