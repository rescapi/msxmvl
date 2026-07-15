// msxmvl clean-room reimplementation of MSXgl "math" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/math.h (interface only) + Z80 knowledge.
#ifndef MSXMVL_MATH_H
#define MSXMVL_MATH_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __FASTCALL
#define __FASTCALL		__z88dk_fastcall
#endif
#ifndef __NAKED
#define __NAKED			__naked
#endif
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// Random method selection (defaults match MSXgl / msxmvl build)
//=============================================================================
#define RANDOM_8_NONE		0
#define RANDOM_8_REGISTER	1
#define RANDOM_8_RACC		2
#define RANDOM_8_ION		3
#define RANDOM_8_MEMORY		4

#define RANDOM_16_NONE		0
#define RANDOM_16_LINEAR	1
#define RANDOM_16_XORSHIFT	2
#define RANDOM_16_LFSR_LCG	3

#ifndef RANDOM_8_METHOD
#define RANDOM_8_METHOD		RANDOM_8_ION		// match MSXgl's default (full 0..255 range)
#endif
#ifndef RANDOM_16_METHOD
#define RANDOM_16_METHOD	RANDOM_16_XORSHIFT
#endif

//=============================================================================
// Vector structures
//=============================================================================
typedef struct VectorU8  { u8  x; u8  y; } VectorU8;
typedef struct VectorI8  { i8  x; i8  y; } VectorI8;
typedef struct VectorU16 { u16 x; u16 y; } VectorU16;
typedef struct VectorI16 { i16 x; i16 y; } VectorI16;

#define VECTOR(type) Vector_##type
#define DEFVECTOR(type) typedef struct VECTOR(type) { type x; type y; } VECTOR(type)

//=============================================================================
// Helper macros
//=============================================================================
#define ABS8(i)				(((u8)(i) & 0x80) ? ~((u8)(i) - 1) : (i))
#define ABS16(i)			(((u16)(i) & 0x8000) ? ~((u16)(i) - 1) : (i))
#define ABS32(i)			(((u32)(i) & 0x80000000) ? ~((u32)(i) - 1) : (i))
#define INVERT(a)			((^(a))++)
#define MERGE44(a, b)		((u8)(((a) & 0x0F) << 4 | ((b) & 0x0F)))
#define MERGE88(a, b)		((u16)((u8)(a) << 8 | (u8)(b)))
#define MOD_POW2(a, b)		(((a) & ((b) - 1)))
#define CLAMP(a, b, c)		(((a) < (b)) ? (b) : ((a) > (c)) ? (c) : (a))
#define CLAMP8(a, b, c)		(((i8)(a) < (i8)(b)) ? (b) : ((i8)(a) > (i8)(c)) ? (c) : (a))
#define CLAMP16(a, b, c)	(((i16)(a) < (i16)(b)) ? (b) : ((i16)(a) > (i16)(c)) ? (c) : (a))
#define MAX(a, b)			(((a) > (b)) ? (a) : (b))
#define MIN(a, b)			(((a) > (b)) ? (b) : (a))

//=============================================================================
// Quick math functions
//=============================================================================

// 8-bits fast division by 10. Returns val / 10.
i8 Math_Div10(i8 val) __FASTCALL __PRESERVES(a, b, c, iyl, iyh);

// 16-bits fast division by 10. Returns val / 10.
i16 Math_Div10_16b(i16 val) __FASTCALL __PRESERVES(b, d, e, iyl, iyh);

// 8-bits fast modulo-10. Returns val % 10.
u8 Math_Mod10(u8 val) __PRESERVES(b, c, d, e, iyl, iyh);

// 16-bits fast modulo-10. Returns val % 10.
u8 Math_Mod10_16b(u16 val) __FASTCALL __PRESERVES(b, c, d, e, iyl, iyh);

// Bits flip routine (reverse the 8 bits of val).
u8 Math_Flip(u8 val) __PRESERVES(c, d, e, h, l, iyl, iyh);

// Bits flip routine (reverse the 16 bits of val).
u16 Math_Flip_16b(u16 val) __PRESERVES(c, iyl, iyh);

// Get the negative (additive inverse) of a 8-bit value.
inline i8 Math_Negative(i8 val) { return -val; }

// Get the negative (additive inverse) of a 16-bit value.
i16 Math_Negative16(i16 val) __FASTCALL __PRESERVES(b, c, d, e, iyl, iyh);

// Swap MSB and LSB bytes of a 16-bits value.
u16 Math_Swap(u16 val) __PRESERVES(a, b, c, iyl, iyh);

// Divide a signed 8-bits integer by 2/4/8/16/32 using arithmetic shift.
i8 Math_SignedDiv2(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh);
i8 Math_SignedDiv4(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh);
i8 Math_SignedDiv8(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh);
i8 Math_SignedDiv16(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh);
i8 Math_SignedDiv32(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh);

// Absolute value of a signed 8/16/32-bits integer.
inline u8  Math_Abs(i8 val)     { return (((u8)(val)  & 0x80) ? ~((u8)(val) - 1) : (val)); }
inline u16 Math_Abs_16b(i16 val){ return (((u16)(val) & 0x8000) ? ~((u16)(val) - 1) : (val)); }
inline u32 Math_Abs_32b(i32 val){ return (((u32)(val) & 0x80000000) ? ~((u32)(val) - 1) : (val)); }

//=============================================================================
// Random routines
//=============================================================================

#if (RANDOM_8_METHOD == RANDOM_8_NONE)
	#define Math_SetRandomSeed8(seed)
	#define Math_GetRandom8() 0
#else
// Initialize 8-bit random generator seed.
void Math_SetRandomSeed8(u8 seed);

// Generate an 8-bit pseudorandom number.
u8 Math_GetRandom8();

// Generate an 8-bit pseudorandom number in [0, max-1].
inline u8 Math_GetRandomMax8(u8 max) { return Math_GetRandom8() % max; }

// Generate an 8-bit pseudorandom number in [min, max-1].
inline u8 Math_GetRandomRange8(u8 min, u8 max) { return min + Math_GetRandom8() % (max - min); }
#endif

#if (RANDOM_16_METHOD == RANDOM_16_NONE)
	#define Math_SetRandomSeed16(seed)
	#define Math_GetRandom16() 0
#else
// Initialize 16-bit random generator seed.
void Math_SetRandomSeed16(u16 seed);

// Generate a 16-bit pseudorandom number.
u16 Math_GetRandom16() __FASTCALL;

// Generate a 16-bit pseudorandom number in [0, max-1].
inline u16 Math_GetRandomMax16(u16 max) { return Math_GetRandom16() % max; }

// Generate a 16-bit pseudorandom number in [min, max-1].
inline u16 Math_GetRandomRange16(u16 min, u16 max) { return min + Math_GetRandom16() % (max - min); }
#endif

#endif // MSXMVL_MATH_H
