// msxmvl clean-room reimplementation of MSXgl "math" module.
// Implemented from the module header (interface/contract) + standard Z80 knowledge.
// SDCC --sdcccall 1. Register-preservation contracts (__PRESERVES) are honored via
// naked asm that pushes/pops exactly the declared registers.
//
// Calling convention (SDCC 4.2, --sdcccall 1), verified empirically:
//   plain   : arg1 8b->A  16b->HL  32b->DEHL ; ret 8b->A  16b->DE  32b->DEHL
//   fastcall: arg  8b->L  16b->HL             ; ret 8b->L  16b->HL

#include "math.h"

//=============================================================================
// Random state
//=============================================================================
#if (RANDOM_8_METHOD == RANDOM_8_ION)
u16 g_Rand8 = 1;	// ION keeps a 16-bit state (init 1, as MSXgl's g_RandomSeed8)
#elif (RANDOM_8_METHOD != RANDOM_8_NONE)
u8 g_Rand8 = 0;
#endif
#if (RANDOM_16_METHOD != RANDOM_16_NONE)
u16 g_Rand16 = 1;
#endif

//=============================================================================
// Quick math
//=============================================================================

// FASTCALL i8 -> i8 : arg in L, return in L. Preserve A,B,C,IY.
// NOTE: matches MSXgl's binary exactly — it divides the byte as UNSIGNED (no sign
// handling), so Math_Div10(-100) == 156/10 == 15, not -10.
i8 Math_Div10(i8 val) __FASTCALL __NAKED __PRESERVES(a, b, c, iyl, iyh)
{
	val;
	__asm
		ld	d, #0		; magic-multiply /10 (loopless): quotient = (val * 0x1A) >> 8
		ld	h, d
		ld	e, l
		add	hl, hl
		add	hl, de
		add	hl, hl
		add	hl, hl
		add	hl, de
		add	hl, hl
		ld	l, h		; quotient -> L (FASTCALL return)
		ret
	__endasm;
}

// FASTCALL i16 -> i16 : arg in HL, return in HL. Preserve B,D,E,IY.
// NOTE: matches MSXgl's binary exactly — UNSIGNED division of the 16-bit value (no sign
// handling), so Math_Div10_16b(-1000) == 64536/10 == 6453, not -100.
i16 Math_Div10_16b(i16 val) __FASTCALL __NAKED __PRESERVES(b, d, e, iyl, iyh)
{
	val;
	__asm
		; Byte-identical to MSXgl: shift-subtract /10 with the first 3 shifts un-subtracted.
		; NOTE: like MSXgl, this uses B as the djnz counter, so B is NOT preserved despite the
		; __PRESERVES(b) header — msxmvl mirrors MSXgl's actual behavior for exact compatibility.
		ld	bc, #0x0D0A	; B=13 iterations, C=10 divisor
		xor	a		; remainder = 0
		add	hl, hl
		rla
		add	hl, hl
		rla
		add	hl, hl
		rla
	00012$:
		add	hl, hl
		rla
		cp	c
		jr	C, 00013$
		sub	c
		inc	l
	00013$:
		djnz	00012$
		ret
	__endasm;
}

// plain u8 -> u8 : arg in A, return in A. Preserve B,C,D,E,IY.
// Constant-time nibble/daa modulo (20 bytes, 83cc) — never slower than a subtraction loop.
u8 Math_Mod10(u8 val) __NAKED __PRESERVES(b, c, d, e, iyl, iyh)
{
	val;
	__asm
		ld	h, a		; add nibbles
		rrca
		rrca
		rrca
		rrca
		add	a, h
		adc	a, #0		; n mod 15 (+1) in both nibbles
		daa
		ld	l, a
		sub	h		; test if quotient is even or odd
		rra
		sbc	a, a
		and	#5
		add	a, l
		daa
		and	#0x0F
		ret
	__endasm;
}

// FASTCALL u16 -> u8 : arg in HL, return in L. Preserve B,C,D,E,IY.
// Constant-time byte+nibble/daa modulo (24 bytes, 98cc) — never slower than a 16-shift loop.
u8 Math_Mod10_16b(u16 val) __FASTCALL __NAKED __PRESERVES(b, c, d, e, iyl, iyh)
{
	val;
	__asm
		ld	a, h		; add bytes
		add	a, l
		adc	a, #0		; n mod 255 (+1)
		ld	h, a		; add nibbles
		rrca
		rrca
		rrca
		rrca
		add	a, h
		adc	a, #0		; n mod 15 (+1) in both nibbles
		daa
		ld	h, a
		sub	l		; test if quotient is even or odd
		rra
		sbc	a, a
		and	#5
		add	a, h
		daa
		and	#0x0F
		ld	l, a
		ret
	__endasm;
}

// plain u8 -> u8 : arg in A, return in A. Preserve C,D,E,H,L,IY (B is scratch).
// John Metcalf's loopless bit-reversal (17 bytes / 66 cycles) — far faster than a shift loop.
u8 Math_Flip(u8 val) __NAKED __PRESERVES(c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		ld	b, a		; a = 76543210
		rlca
		rlca			; a = 54321076
		xor	b
		and	#0xAA
		xor	b		; a = 56341270
		ld	b, a
		rlca
		rlca
		rlca			; a = 41270563
		rrc	b		; b = 05634127
		xor	b
		and	#0x66
		xor	b		; a = 01234567
		ret
	__endasm;
}

// plain u16 -> u16 : arg in HL, return in DE. Preserve C,IY (B,D,E,H,L scratch).
// Byte-wise flip via Math_Flip (loopless) — matches MSXgl and is far faster than a 16-shift loop.
u16 Math_Flip_16b(u16 val) __NAKED __PRESERVES(c, iyl, iyh)
{
	val;
	__asm
		ld	a, h
		call	_Math_Flip	; flip high bits
		ld	e, a
		ld	a, l
		call	_Math_Flip	; flip low bits
		ld	d, a		; DE = flipped(low):flipped(high)
		ret
	__endasm;
}

// FASTCALL i16 -> i16 : arg in HL, return in HL. Preserve B,C,D,E,IY.
i16 Math_Negative16(i16 val) __FASTCALL __NAKED __PRESERVES(b, c, d, e, iyl, iyh)
{
	val;
	__asm
		xor	a
		sub	l
		ld	l, a
		sbc	a, a		; A = -carry (0xFF if borrow, else 0) — 1 byte vs ld a,#0
		sub	h
		ld	h, a
		ret
	__endasm;
}

// plain u16 -> u16 : arg in HL, return in DE. Preserve A,B,C,IY.
u16 Math_Swap(u16 val) __NAKED __PRESERVES(a, b, c, iyl, iyh)
{
	val;
	__asm
		ld	d, l		; DE high byte = input low byte
		ld	e, h		; DE low byte = input high byte
		ret
	__endasm;
}

// plain i8 -> i8 : arg in A, return in A. Arithmetic right shifts.
// NOTE: matches MSXgl's binary exactly — for negative values it adds 2^n before the
// arithmetic shift (a round-toward-zero bias), so e.g. Math_SignedDiv2(-8) == -3, not -4.
i8 Math_SignedDiv2(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		bit	7, a
		jr	Z, 00201$
		add	a, #2
	00201$:
		sra	a
		ret
	__endasm;
}

i8 Math_SignedDiv4(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		bit	7, a
		jr	Z, 00202$
		add	a, #4
	00202$:
		sra	a
		sra	a
		ret
	__endasm;
}

i8 Math_SignedDiv8(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		bit	7, a
		jr	Z, 00203$
		add	a, #8
	00203$:
		sra	a
		sra	a
		sra	a
		ret
	__endasm;
}

i8 Math_SignedDiv16(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		bit	7, a
		jr	Z, 00204$
		add	a, #16
	00204$:
		sra	a
		sra	a
		sra	a
		sra	a
		ret
	__endasm;
}

i8 Math_SignedDiv32(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		bit	7, a
		jr	Z, 00205$
		add	a, #32
	00205$:
		sra	a
		sra	a
		sra	a
		sra	a
		sra	a
		ret
	__endasm;
}

//=============================================================================
// Random - 8 bits
//=============================================================================
#if (RANDOM_8_METHOD == RANDOM_8_ION)

// ION Random (Joe Wingbermuehle) -- 16-bit state, full 0..255 result. This is MSXgl's
// default RANDOM_8_METHOD, so msxmvl defaults to it too: same 0..255 range and, using
// MSXgl's "better distribution" variant, the same generated sequence. The older
// RANDOM_8_RACC (R-register accumulation) is a smaller/faster generator but only yields
// 0..127, so it is now opt-in rather than the default (it broke drop-in range parity).
void Math_SetRandomSeed8(u8 seed)
{
	g_Rand8 = (u16)seed;
}

u8 Math_GetRandom8() __NAKED
{
	__asm
		ld	hl, (_g_Rand8)
		ld	a, r
		ld	d, a
		ld	e, a
		add	hl, de
		xor	a, l
		add	a, a
		xor	a, h
		ld	l, a
		ld	(_g_Rand8), hl
		ret				; result in A (0..255)
	__endasm;
}

#elif (RANDOM_8_METHOD != RANDOM_8_NONE)

// RANDOM_8_RACC: R-register accumulation, 7-bit result (0..127). Opt in with
// -DRANDOM_8_METHOD=RANDOM_8_RACC when the narrower range and smaller code are wanted.
void Math_SetRandomSeed8(u8 seed)
{
	g_Rand8 = seed;
}

u8 Math_GetRandom8() __NAKED
{
	__asm
		ld	a, r
		ld	hl, #_g_Rand8
		add	a, (hl)
		ld	(hl), a
		and	#0x7F
		ret
	__endasm;
}

#endif // RANDOM_8_METHOD

//=============================================================================
// Random - 16 bits (RANDOM_16_XORSHIFT: xorshift 7,9,8)
//=============================================================================
#if (RANDOM_16_METHOD != RANDOM_16_NONE)

void Math_SetRandomSeed16(u16 seed)
{
	g_Rand16 = seed ? seed : 1;	// avoid a zero state (xorshift lock-up)
}

// 16-bit xorshift PRNG by John Metcalf. Byte-identical to MSXgl's RANDOM_16_XORSHIFT
// (20 bytes, 86 cycles); FASTCALL -> HL. Uses msxmvl's g_Rand16 (init 1, same as MSXgl's
// g_RandomSeed16), so the generated sequence is identical to MSXgl's.
u16 Math_GetRandom16() __NAKED __FASTCALL
{
	__asm
		ld		hl, (_g_Rand16)
		ld		a, h
		rra
		ld		a, l
		rra
		xor		h
		ld		h, a
		ld		a, l
		rra
		ld		a, h
		rra
		xor		l
		ld		l, a
		xor		h
		ld		h, a
		ld		(_g_Rand16), hl
		ret
	__endasm;
}

#endif // RANDOM_16_METHOD
