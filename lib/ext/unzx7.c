// msxmvl EXTENSION module: unzx7 -- runtime ZX7 decompressor.
//
// Decompressor for the ZX7 format by Einar Saukas (format only; independent
// implementation). Clean-room: the RAM decoder below was written from the ZX7
// format (a literal-first LZ77/LZSS stream -- one control bit picks literal vs
// match; match lengths are NON-interlaced Elias-gamma, offsets are 7 or 11 bits),
// with the reference decoders used only as a behavioural oracle. See unzx7_DOC.md
// for the measured size/speed comparison.
//
// Toolchain: SDCC --sdcccall 1. arg1 -> HL, arg2 -> DE, so UnZX7's naked body is
// the decoder itself -- no argument shuffling and no wrapper.
#include "unzx7.h"

//=============================================================================
// RAM DECODER
//=============================================================================
// Register use: HL = compressed source, DE = output cursor, BC = match length /
// LDIR counter, A = an 8-bit MSB-first bit reservoir (the 0x80 seed doubles as an
// "empty, reload" sentinel). The saved output cursor is parked on the stack while
// a match is being copied.
//
// ZX7 stream: the first byte is always a literal. Thereafter each token is one
// control bit -- 0 = a literal byte copied straight from the stream, 1 = a match.
// A match is a length (non-interlaced Elias-gamma value, +1) then an offset (a
// byte whose top bit flags 4 extra bits from the reservoir). End-of-stream is a
// gamma prefix of 16 leading zeros; we detect it by counting, so we never read
// past the end of the compressed block.
void UnZX7(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;   // src in HL, dst in DE (sdcccall 1)
	__asm
		ld   a, #0x80          ; reservoir empty -> first bit forces a reload
	zx7_literal:
		ldi                    ; emit one literal byte straight from the stream
	zx7_main:
		call zx7_bit           ; control bit: 0 = literal, 1 = match
		jr   NC, zx7_literal
		;; ---- match length: non-interlaced Elias-gamma ----
		push de                ; park the output cursor; frees D as scratch
		ld   d, #0             ; D = count of leading zeros
	zx7_lcount:
		call zx7_bit
		jr   C, zx7_lbits      ; the terminating 1 bit ends the zero run
		inc  d
		bit  4, d              ; 16 leading zeros = the end-of-stream marker
		jr   Z, zx7_lcount
		pop  hl                ; end of stream: drop the parked cursor
		ret
	zx7_lbits:
		ld   bc, #1            ; value = 1 (the implicit leading one)
		inc  d                 ; read exactly D data bits, MSB-first
	zx7_lloop:
		dec  d
		jr   Z, zx7_offset
		call zx7_bit
		rl   c
		rl   b
		jr   zx7_lloop
	zx7_offset:
		inc  bc                ; length = gamma value + 1  (D is 0 here)
		;; ---- match offset ----
		ld   e, (hl)           ; offset byte: bit7 = "4 more bits" flag, bits6..0 = low
		inc  hl
		sla  e                 ; CF = flag ; E = (offbyte & 0x7F) << 1
		jr   NC, zx7_ocopy     ; flag clear => 7-bit offset, no extra bits
		ld   d, #0x10          ; walking marker: gather 4 high bits into D
	zx7_obit:
		call zx7_bit
		rl   d
		jr   NC, zx7_obit      ; the 0x10 marker exits after exactly 4 bits; D = n
		inc  d                 ; D = n + 1
		srl  d                 ; CF = (n+1) & 1 ; D = (n+1) >> 1
	zx7_ocopy:
		rr   e                 ; fold that top bit back into E ; DE = offset value
		scf                    ; distance = offset value + 1 -> the extra -1 below
		ex   (sp), hl          ; HL = output cursor ; park the source on the stack
		push hl
		sbc  hl, de            ; HL = dst - offset - 1 = match source
		pop  de                ; DE = output cursor
		ldir                   ; copy the match
		pop  hl                ; restore the source pointer
		jr   zx7_main
	;; MSB-first bit reservoir; returns the next bit in CF.
	zx7_bit:
		add  a, a              ; shift out the next bit
		ret  NZ                ; reservoir still holds bits
		ld   a, (hl)           ; spent: pull the next 8 bits from the stream
		inc  hl
		rla
		ret
	__endasm;
}
