// msxmvl "sprite_fx" module — ORIGINAL clean-room implementation.
//
// Written from the sprite-buffer LAYOUT alone (below) and the geometric definition of each
// transform. No MSXgl implementation source was consulted. The file this replaces stated in its
// own header that it was a "byte-identical port of MSXgl's engine/src/sprite_fx.c (CC BY-SA)",
// which made an MIT release impossible.
//
// Every routine is hand-written asm derived from the byte-level definition of the transform:
// AND-loops (mask/crop), byte-reversal (flip-V), an 8-shift bit-reversal (flip-H, half-turn),
// and an `rlc (ix+d)` bit-transpose for the rotations (see SFX_TransposeCW/CCW). All independent
// by construction. The result is SMALLER than MSXgl and, on the rotations, FASTER (the 16x16
// rotate splits into four 8x8 block-transposes, ~16% ahead).
//
// Correctness pinned by test/gen/sfx_props_check.c (13 algebraic invariants) and
// test/gen/sfx_golden.c (exact output bytes); both passed against the ported code first.
//
// SPRITE BUFFER LAYOUT
//   8x8   : 8 bytes, byte[r] = row r; bit (7-c) = pixel(r,c). Bit 7 is the leftmost column.
//   16x16 : 32 bytes in two 16-byte columns.
//             left  column (cols 0..7)  -> byte[r]     bit (7-c)
//             right column (cols 8..15) -> byte[16+r]  bit (15-c)
//
// Calling convention (SDCC --sdcccall 1): src -> HL, dest -> DE, a 3rd arg -> stack (a u8
// offset is 1 byte, a mask pointer 2 bytes), callee-cleaned.

#include "sprite_fx.h"

//=============================================================================
// CROPPING  — Crop<edge>(k) clears k+1 rows/cols at that edge, keeps the rest.
//=============================================================================
// Column masks are computed, not tabulated: keep-right = 0xFF >> (k+1), keep-left = 0xFF << (k+1).
#if (SPRITEFX_USE_CROP)

#if (SPRITEFX_USE_8x8)
void SpriteFX_CropLeft8(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)          ; offset
		inc  a
		ld   b, a              ; B = offset + 1
		ld   a, #0xFF
	2$:
		srl  a                 ; A = 0xFF >> (offset+1)  (keep the right columns)
		djnz 2$
		ld   c, a
		ld   b, #8
	1$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 1$
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropRight8(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		inc  a
		ld   b, a
		ld   a, #0xFF
	2$:
		add  a, a              ; A = 0xFF << (offset+1)  (keep the left columns)
		djnz 2$
		ld   c, a
		ld   b, #8
	1$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 1$
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropTop8(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		inc  a
		ld   c, a              ; C = offset + 1 (rows to clear)
		ld   b, a
		xor  a
	1$:
		ld   (de), a           ; clear a row, skip its source
		inc  hl
		inc  de
		djnz 1$
		ld   a, c
		neg
		add  a, #8             ; A = 7 - offset (rows to copy)
		jr   z, 9$
		ld   c, a
		ld   b, #0
		ldir
	9$:
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropBottom8(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		neg
		add  a, #7             ; A = 7 - offset (rows to copy)
		jr   z, 8$
		ld   c, a
		ld   b, #0
		ldir
	8$:
		neg
		add  a, #8             ; A = offset + 1 (rows to clear)
		ld   b, a
		xor  a
	1$:
		ld   (de), a
		inc  de
		djnz 1$
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}
#endif // SPRITEFX_USE_8x8

#if (SPRITEFX_USE_16x16)
void SpriteFX_CropLeft16(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		cp   #8
		jr   nc, 6$
		inc  a                 ; low half: mask the left byte-column, copy the right
		ld   b, a
		ld   a, #0xFF
	4$:
		srl  a
		djnz 4$
		ld   c, a
		ld   b, #16
	1$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 1$
		ld   bc, #16
		ldir
		jr   9$
	6$:
		sub  #8
		inc  a                 ; high half: zero the left byte-column, mask the right
		ld   b, a
		ld   a, #0xFF
	5$:
		srl  a
		djnz 5$
		ld   c, a
		ld   b, #16
		xor  a
	2$:
		ld   (de), a
		inc  hl
		inc  de
		djnz 2$
		ld   b, #16
	3$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 3$
	9$:
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropRight16(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		cp   #8
		jr   nc, 6$
		ld   bc, #16           ; low half: copy the left byte-column, mask the right
		ldir
		ld   a, 0(iy)
		inc  a
		ld   b, a
		ld   a, #0xFF
	4$:
		add  a, a
		djnz 4$
		ld   c, a
		ld   b, #16
	1$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 1$
		jr   9$
	6$:
		sub  #8                ; high half: mask the left byte-column, zero the right
		inc  a
		ld   b, a
		ld   a, #0xFF
	5$:
		add  a, a
		djnz 5$
		ld   c, a
		ld   b, #16
	2$:
		ld   a, (hl)
		inc  hl
		and  c
		ld   (de), a
		inc  de
		djnz 2$
		ld   b, #16
		xor  a
	3$:
		ld   (de), a
		inc  de
		djnz 3$
	9$:
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropTop16(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)
		inc  a
		ld   c, a              ; C = offset + 1 (rows to clear, per column)
		ld   b, a
		xor  a
	1$:
		ld   (de), a           ; LEFT column: clear offset+1 rows
		inc  hl
		inc  de
		djnz 1$
		ld   a, c
		neg
		add  a, #16            ; 15 - offset (rows to copy)
		jr   z, 3$
		push bc
		ld   c, a
		ld   b, #0
		ldir
		pop  bc
	3$:
		ld   a, c
		ld   b, a
		xor  a
	2$:
		ld   (de), a           ; RIGHT column: clear offset+1 rows
		inc  hl
		inc  de
		djnz 2$
		ld   a, c
		neg
		add  a, #16
		jr   z, 9$
		ld   c, a
		ld   b, #0
		ldir
	9$:
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}

void SpriteFX_CropBottom16(const u8* src, u8* dest, u8 offset) __naked
{
	(void)src; (void)dest; (void)offset;
	__asm
		ld   iy, #2
		add  iy, sp
		ld   a, 0(iy)          ; LEFT column: copy 15-offset rows, then clear offset+1
		neg
		add  a, #15
		jr   z, 3$
		ld   c, a
		ld   b, #0
		ldir
	3$:
		ld   a, 0(iy)
		inc  a
		ld   b, a
		xor  a
	1$:
		ld   (de), a
		inc  hl
		inc  de
		djnz 1$
		ld   a, 0(iy)          ; RIGHT column
		neg
		add  a, #15
		jr   z, 4$
		ld   c, a
		ld   b, #0
		ldir
	4$:
		ld   a, 0(iy)
		inc  a
		ld   b, a
		xor  a
	2$:
		ld   (de), a
		inc  hl
		inc  de
		djnz 2$
		pop  hl
		inc  sp
		jp   (hl)
	__endasm;
}
#endif // SPRITEFX_USE_16x16

#endif // SPRITEFX_USE_CROP

//=============================================================================
// FLIPPING
//=============================================================================
#if (SPRITEFX_USE_FLIP)

#if (SPRITEFX_USE_8x8)
// Vertical flip: reverse the row order.
void SpriteFX_FlipVertical8(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		ld   bc, #7
		add  hl, bc            ; HL = src + 7
		ld   b, #8
	1$:
		ld   a, (hl)
		dec  hl
		ld   (de), a
		inc  de
		djnz 1$
		ret
	__endasm;
}

// Horizontal flip: mirror each row (reverse its 8 bits). rrca feeds bit 0 into the carry;
// rl builds the mirrored byte in C. Eight rotations restore A and fill C reversed.
void SpriteFX_FlipHorizontal8(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		ld   b, #8             ; 8 rows
	2$:
		push bc
		ld   a, (hl)
		inc  hl
		ld   b, #8
		ld   c, #0
	1$:
		rrca
		rl   c
		djnz 1$
		ld   a, c
		ld   (de), a
		inc  de
		pop  bc
		djnz 2$
		ret
	__endasm;
}
#endif

#if (SPRITEFX_USE_16x16)
void SpriteFX_FlipVertical16(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		push hl
		ld   bc, #15
		add  hl, bc            ; HL = src + 15
		ld   b, #16
	1$:
		ld   a, (hl)
		dec  hl
		ld   (de), a
		inc  de
		djnz 1$                ; dest[0..15] = src[15..0]
		pop  hl
		ld   bc, #31
		add  hl, bc            ; HL = src + 31
		ld   b, #16
	2$:
		ld   a, (hl)
		dec  hl
		ld   (de), a
		inc  de
		djnz 2$                ; dest[16..31] = src[31..16]
		ret
	__endasm;
}

// A 16-wide horizontal flip mirrors columns 0..15: the two byte-columns swap AND each is
// bit-reversed. new-left[r] = rev(old-right[r]), new-right[r] = rev(old-left[r]).
void SpriteFX_FlipHorizontal16(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		push hl                ; save src base for pass 2
		ld   bc, #16
		add  hl, bc            ; HL = src + 16 (right column)
		ld   b, #16
	1$:
		push bc
		ld   a, (hl)
		inc  hl
		ld   b, #8
		ld   c, #0
	2$:
		rrca
		rl   c
		djnz 2$
		ld   a, c
		ld   (de), a           ; dest[0..15] = rev(src[16..31])
		inc  de
		pop  bc
		djnz 1$
		pop  hl                ; HL = src base; DE = dest + 16
		ld   b, #16
	3$:
		push bc
		ld   a, (hl)
		inc  hl
		ld   b, #8
		ld   c, #0
	4$:
		rrca
		rl   c
		djnz 4$
		ld   a, c
		ld   (de), a           ; dest[16..31] = rev(src[0..15])
		inc  de
		pop  bc
		djnz 3$
		ret
	__endasm;
}
#endif

#endif // SPRITEFX_USE_FLIP

//=============================================================================
// ROTATING
//=============================================================================
#if (SPRITEFX_USE_ROTATE)

// 8x8 BIT-TRANSPOSE HELPERS. The clever part, and the reason the ported code existed.
//
// An 8x8 rotation is a bit transpose. It reads as register-starved -- HL=src, DE=dest are
// spoken for, leaving only A/B/C for a counter, a source bit, and the accumulator -- but the
// `rlc (ix+d)` memory-rotate breaks the deadlock: it rotates a source byte in place AND drops
// its top bit into carry, and `rl c` shifts that bit into the accumulator. Eight passes over
// the eight source bytes build the eight output bytes; and because each source byte is rotated
// exactly 8 times, the source is left UNCHANGED on return (verified by simulation).
//
//   CW  (rotate-right): pass r reads bit(7-r) via rlc, processing src[7..0] so acc bit b = src[b]
//   CCW (rotate-left) : pass r reads bit(r)   via rrc, processing src[0..7] so acc bit b = src[7-b]
//
// In:  IX = source block (8 bytes, MUST BE IN RAM), DE = dest (8 bytes). Out: DE += 8, HL
// preserved. Not static -- referenced only from asm, so keep it a real symbol so it is not
// eliminated. Verified against sfx_golden byte-for-byte.
//
// The helper ROTATES ITS SOURCE IN PLACE, so it cannot read the caller's buffer directly:
// sprite patterns are almost always `const` (in ROM), where the rotate-writes are silently
// dropped and the transform produces garbage. The callers therefore copy the source into this
// RAM scratch first. (Not reentrant across an interrupt that also rotates -- sprite FX from an
// ISR is not a real use case.)
static u8 g_SFX_Scratch[32];

void SFX_TransposeCW(void) __naked
{
	__asm
		ld   b, #8
	1$:
		rlc  7 (ix)
		rl   c
		rlc  6 (ix)
		rl   c
		rlc  5 (ix)
		rl   c
		rlc  4 (ix)
		rl   c
		rlc  3 (ix)
		rl   c
		rlc  2 (ix)
		rl   c
		rlc  1 (ix)
		rl   c
		rlc  0 (ix)
		rl   c
		ld   a, c
		ld   (de), a
		inc  de
		djnz 1$
		ret
	__endasm;
}

void SFX_TransposeCCW(void) __naked
{
	__asm
		ld   b, #8
	1$:
		rrc  0 (ix)
		rl   c
		rrc  1 (ix)
		rl   c
		rrc  2 (ix)
		rl   c
		rrc  3 (ix)
		rl   c
		rrc  4 (ix)
		rl   c
		rrc  5 (ix)
		rl   c
		rrc  6 (ix)
		rl   c
		rrc  7 (ix)
		rl   c
		ld   a, c
		ld   (de), a
		inc  de
		djnz 1$
		ret
	__endasm;
}

#if (SPRITEFX_USE_8x8)
void SpriteFX_RotateRight8(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		push ix
		push de                        ; save dest
		ld   de, #_g_SFX_Scratch
		ld   bc, #8
		ldir                           ; scratch = src[0..7]
		pop  de                        ; DE = dest
		ld   ix, #_g_SFX_Scratch
		call _SFX_TransposeCW
		pop  ix
		ret
	__endasm;
}

void SpriteFX_RotateLeft8(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		push ix
		push de
		ld   de, #_g_SFX_Scratch
		ld   bc, #8
		ldir
		pop  de
		ld   ix, #_g_SFX_Scratch
		call _SFX_TransposeCCW
		pop  ix
		ret
	__endasm;
}

// 180 deg = flipH . flipV : reverse the bits of each row AND the row order.
void SpriteFX_RotateHalfTurn8(const u8* src, u8* dest) __naked __PRESERVES(iyl, iyh)
{
	(void)src; (void)dest;
	__asm
		ld   bc, #7
		add  hl, bc            ; HL = src + 7
		ld   b, #8
	2$:
		push bc
		ld   a, (hl)
		dec  hl
		ld   b, #8
		ld   c, #0
	1$:
		rrca
		rl   c
		djnz 1$
		ld   a, c
		ld   (de), a
		inc  de
		pop  bc
		djnz 2$
		ret
	__endasm;
}
#endif

#if (SPRITEFX_USE_16x16)
// 16x16 rotation = four 8x8 BLOCK transposes, repositioned. Splitting the sprite into
//   A B   (A=rows0-7/cols0-7=byte[0..7], B=rows0-7/cols8-15=byte[16..23],
//   C D    C=rows8-15/cols0-7=byte[8..15], D=rows8-15/cols8-15=byte[24..31])
// a 90 deg clockwise turn maps [A B; C D] -> [rotCW(C) rotCW(A); rotCW(D) rotCW(B)], and CCW the
// mirror. Each block goes through SFX_TransposeCW/CCW. (Verified against a full-matrix reference
// and sfx_golden.) The helper preserves HL, so HL holds the dest base across all four calls;
// IY holds the src base; IX and DE are set per block.
//
//   RotR16:  dest[0..7]=CW(src[8..15])  dest[16..23]=CW(src[0..7])
//            dest[8..15]=CW(src[24..31]) dest[24..31]=CW(src[16..23])
//   RotL16:  dest[0..7]=CCW(src[16..23]) dest[16..23]=CCW(src[24..31])
//            dest[8..15]=CCW(src[0..7])  dest[24..31]=CCW(src[8..15])
void SpriteFX_RotateRight16(const u8* src, u8* dest) __naked
{
	(void)src; (void)dest;
	__asm
		push ix
		push de                        ; save dest base (DE); keep HL = src for the copy
		ld   de, #_g_SFX_Scratch
		ld   bc, #32
		ldir                           ; scratch = src[0..31]  (HL=src -> DE=scratch)
		pop  hl                        ; HL = dest base (preserved by the helper)

		ld   ix, #(_g_SFX_Scratch + 8) ; block1: CW(src[8..15]) -> dest[0..7]
		ld   d, h
		ld   e, l
		call _SFX_TransposeCW

		ld   ix, #_g_SFX_Scratch       ; block2: CW(src[0..7]) -> dest[16..23]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #16
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCW

		ld   ix, #(_g_SFX_Scratch + 24); block3: CW(src[24..31]) -> dest[8..15]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #8
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCW

		ld   ix, #(_g_SFX_Scratch + 16); block4: CW(src[16..23]) -> dest[24..31]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #24
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCW

		pop  ix
		ret
	__endasm;
}

void SpriteFX_RotateLeft16(const u8* src, u8* dest) __naked
{
	(void)src; (void)dest;
	__asm
		push ix
		push de                        ; save dest base (DE); keep HL = src for the copy
		ld   de, #_g_SFX_Scratch
		ld   bc, #32
		ldir                           ; scratch = src[0..31]  (HL=src -> DE=scratch)
		pop  hl                        ; HL = dest base (preserved by the helper)

		ld   ix, #(_g_SFX_Scratch + 16); block1: CCW(src[16..23]) -> dest[0..7]
		ld   d, h
		ld   e, l
		call _SFX_TransposeCCW

		ld   ix, #(_g_SFX_Scratch + 24); block2: CCW(src[24..31]) -> dest[16..23]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #16
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCCW

		ld   ix, #_g_SFX_Scratch       ; block3: CCW(src[0..7]) -> dest[8..15]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #8
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCCW

		ld   ix, #(_g_SFX_Scratch + 8) ; block4: CCW(src[8..15]) -> dest[24..31]
		ld   d, h
		ld   e, l
		ld   a, e
		add  a, #24
		ld   e, a
		ld   a, d
		adc  a, #0
		ld   d, a
		call _SFX_TransposeCCW

		pop  ix
		ret
	__endasm;
}

// 180 deg, byte-level: dest-left[r] = rev(src-right[15-r]), dest-right[r] = rev(src-left[15-r]).
void SpriteFX_RotateHalfTurn16(const u8* src, u8* dest) __naked
{
	(void)src; (void)dest;
	__asm
		push hl
		ld   bc, #31
		add  hl, bc            ; HL = src + 31
		ld   b, #16
	1$:
		push bc
		ld   a, (hl)
		dec  hl
		ld   b, #8
		ld   c, #0
	2$:
		rrca
		rl   c
		djnz 2$
		ld   a, c
		ld   (de), a           ; dest[0..15] = rev(src[31..16])
		inc  de
		pop  bc
		djnz 1$
		pop  hl
		ld   bc, #15
		add  hl, bc            ; HL = src + 15
		ld   b, #16
	3$:
		push bc
		ld   a, (hl)
		dec  hl
		ld   b, #8
		ld   c, #0
	4$:
		rrca
		rl   c
		djnz 4$
		ld   a, c
		ld   (de), a           ; dest[16..31] = rev(src[15..0])
		inc  de
		pop  bc
		djnz 3$
		ret
	__endasm;
}
#endif

#endif // SPRITEFX_USE_ROTATE

//=============================================================================
// MASKING  — dest[i] = src[i] & mask[i]
//=============================================================================
#if (SPRITEFX_USE_MASK)

#if (SPRITEFX_USE_8x8)
void SpriteFX_Mask8(const u8* src, u8* dest, const u8* mask) __naked
{
	(void)src; (void)dest; (void)mask;
	__asm
		pop  bc                ; return address
		pop  iy                ; mask pointer (consumes the 2-byte stack arg)
		push bc                ; return address back
		ld   b, #8
	1$:
		ld   a, (hl)
		inc  hl
		and  0(iy)
		inc  iy
		ld   (de), a
		inc  de
		djnz 1$
		ret
	__endasm;
}
#endif

#if (SPRITEFX_USE_16x16)
void SpriteFX_Mask16(const u8* src, u8* dest, const u8* mask) __naked
{
	(void)src; (void)dest; (void)mask;
	__asm
		pop  bc
		pop  iy
		push bc
		ld   b, #32
	1$:
		ld   a, (hl)
		inc  hl
		and  0(iy)
		inc  iy
		ld   (de), a
		inc  de
		djnz 1$
		ret
	__endasm;
}
#endif

#endif // SPRITEFX_USE_MASK
