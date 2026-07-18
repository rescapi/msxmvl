// msxmvl EXTENSION module: unpletter -- runtime Pletter v0.5c decompressor.
//
// Decompressor for the Pletter v0.5c format by XL2S Entertainment (format only;
// independent implementation). Clean-room: the RAM decoder below was written from
// the Pletter v0.5c stream layout, with the reference depacker used only as a
// behavioural oracle. See unpletter_DOC.md for the measured size/speed comparison.
//
// Toolchain: SDCC --sdcccall 1. arg1 -> HL, arg2 -> DE, so UnPletter's naked body
// is the decoder itself -- no argument shuffling and no wrapper.
//
// FORMAT (what the code below implements):
//   * The first stream byte is the opening control byte. Its top 3 bits hold the
//     mode index q-1 (q in 1..6): q sets how many extra high bits a "far" offset
//     carries. The low 5 bits are the first 5 control bits, so the byte doubles as
//     the initial bit reservoir.
//   * Bits are read MSB-first from a running byte pointer. Control bytes and raw
//     data bytes (literals, offset low bytes) share that one pointer -- a control
//     byte is only fetched when the reservoir runs dry, exactly reproducing the
//     encoder's lazily-claimed "event byte" interleave.
//   * The output always opens with one literal byte. Then each control bit picks:
//       0 -> copy one literal byte from the stream.
//       1 -> a match: an interlaced Elias-gamma length (value+1 = bytes to copy),
//            then an offset. Offset low byte c: if bit7 is clear the offset is
//            c+1 (1..128, "near"); if bit7 is set, q extra high bits are read from
//            the bitstream and folded in for a "far" offset up to 8320.
//   * End of stream: a length gamma of 34 one-bits overflows 16 bits; that carry
//     is the terminator and returns from the decoder.
#include "unpletter.h"

//=============================================================================
// RAM DECODER
//=============================================================================
// Register use: HL = compressed source, DE = output cursor, BC = match offset
// then LDIR counter, A = MSB-first bit reservoir (a planted guard bit doubles as
// the "empty, reload" sentinel), IX = the far-offset entry for this stream's mode,
// IY = the post-match loop re-entry, and the alternate HL = the Elias-gamma
// length accumulator. The routine does `di` on entry and leaves interrupts
// disabled on return (see the di comment below). sdasz80 rejects `ld ixl,e` /
// `ld ixh,e`, so those two loads are emitted as their DD-prefixed bytes.
void UnPletter(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;   // src in HL, dst in DE (sdcccall 1)
	__asm
		;; The decoder trashes IX, IY and the alternate register set (exx). SDCC
		;; treats IX as callee-saved, so preserve IX/IY across the call, and mask
		;; interrupts because a C-BIOS-style ISR also uses those registers mid-decode.
		;; Interrupts are left disabled on return (the decode ends in the alternate
		;; bank, where re-enabling them would let a bank-swapping ISR corrupt the
		;; caller); the caller re-enables with ei once its load phase is done.
		push ix
		push iy
		di
		;; ---- header: pull mode q-1 from the top 3 bits, seed the reservoir ----
		ld   a, (hl)           ; opening control byte
		inc  hl
		exx                    ; build the mode-table index in the alternate DE
		ld   de, #0
		add  a, a              ; shift out mode bit2 (carry); drop it from A
		inc  a                 ; plant the reservoir guard bit in bit0
		rl   e
		add  a, a              ; mode bit1
		rl   e
		add  a, a              ; mode bit0
		rl   e
		rl   e                 ; E = 2*(q-1) : word index into pl_modes
		ld   hl, #pl_modes
		add  hl, de
		ld   e, (hl)
		.db  0xDD, 0x5B        ; ld ixl,e  -- far-offset entry (low byte)
		inc  hl
		ld   e, (hl)
		.db  0xDD, 0x63        ; ld ixh,e  -- far-offset entry (high byte)
		ld   e, #1             ; alt DE = 1 : the Elias-gamma seed / length base
		exx
		ld   iy, #pl_loop      ; where a finished match returns to
	pl_literal:
		ldi                    ; copy one literal byte, source -> dest
	pl_loop:
		add  a, a              ; next control bit
		call Z, pl_getbit
		jr   NC, pl_literal    ; 0 -> another literal
		;; ---- match: interlaced Elias-gamma length into the alternate HL ----
		exx
		ld   h, d
		ld   l, e              ; alt HL = 1 (gamma seed; header planted alt DE = 1)
	pl_getlen:
		add  a, a
		call Z, pl_getbitexx
		jr   NC, pl_lenok
	pl_getlen_lus:
		add  a, a
		call Z, pl_getbitexx
		adc  hl, hl            ; shift a length bit in
		jp   C, pl_done        ; overflow past 16 bits = the 34-bit end marker
		add  a, a
		call Z, pl_getbitexx
		jr   NC, pl_lenok
		add  a, a
		call Z, pl_getbitexx
		adc  hl, hl
		jp   C, pl_done
		add  a, a
		call Z, pl_getbitexx
		jp   C, pl_getlen_lus
	pl_lenok:
		inc  hl                ; length = gamma value + 1  (parked in alt HL)
		exx
		;; ---- offset: one stream byte, plus mode-many high bits when "far" ----
		ld   c, (hl)           ; offset low byte
		inc  hl
		ld   b, #0
		bit  7, c
		jp   Z, pl_offsok      ; bit7 clear -> near offset = C+1
		jp   (ix)              ; bit7 set   -> read q extra high bits (by mode)
	pl_mode6:
		add  a, a
		call Z, pl_getbit
		rl   b
	pl_mode5:
		add  a, a
		call Z, pl_getbit
		rl   b
	pl_mode4:
		add  a, a
		call Z, pl_getbit
		rl   b
	pl_mode3:
		add  a, a
		call Z, pl_getbit
		rl   b
	pl_mode2:
		add  a, a
		call Z, pl_getbit
		rl   b
		add  a, a
		call Z, pl_getbit      ; final high bit folds the marker into the offset
		jr   NC, pl_offsok
		or   a                 ; clear carry for the coming sbc
		inc  b
		res  7, c
	pl_offsok:
		inc  bc                ; BC = offset
		push hl                ; park the source pointer
		exx
		push hl                ; push length (alt HL)
		exx
		ld   l, e
		ld   h, d              ; HL = dest cursor
		sbc  hl, bc            ; HL = dest - offset  (the match source)
		pop  bc                ; BC = length
		ldir                   ; copy the match (handles overlapping runs)
		pop  hl                ; restore the source pointer
		jp   (iy)              ; back to pl_loop
	;; single exit (end marker): restore the caller IX/IY and return. Reached in
	;; the alternate bank, but IX/IY and the stack are not banked, so this is safe.
	pl_done:
		pop  iy
		pop  ix
		ret
	;; -- one-instruction reservoir refill: MSB -> carry, planted guard -> bit0 --
	pl_getbit:
		ld   a, (hl)
		inc  hl
		rla
		ret
	pl_getbitexx:
		exx
		ld   a, (hl)
		inc  hl
		exx
		rla
		ret
	;; far-offset entry table, indexed by (q-1); q=1 has no extra high bits.
	pl_modes:
		.dw  pl_offsok
		.dw  pl_mode2
		.dw  pl_mode3
		.dw  pl_mode4
		.dw  pl_mode5
		.dw  pl_mode6
	__endasm;
}
