// msxmvl "crypt" module — ORIGINAL implementation.
//
// Clean-room: written from crypt.h's published interface only. No MSXgl implementation
// source was consulted; the default tables below are OUR OWN.
//
// CONSEQUENCE: the DEFAULT map and code table differ from MSXgl's, so a password made by
// MSXgl's crypt with ITS defaults will not decode here. The API is identical, and a
// program supplying its own tables (Crypt_SetMap / Crypt_SetCode) is unaffected.
//
// What it does: each input byte becomes exactly TWO characters (output = size*2 + 1).
//   1. KEY     — XOR the byte with the key, cycling through it.
//   2. SPREAD  — scatter the byte's bits through g_CryptCode: bit i contributes the mask
//                g_CryptCode[i]. The accumulator's low byte picks character 1, its high
//                byte character 2. Every mask is one distinct bit, so it is invertible.
//   3. CASCADE — add the previous byte's indices onto this byte's (mod 32), so a repeated
//                input byte does not produce a repeated output pair.
//
// NOTE: the cross-function jumps are JP, not JR. JR only reaches +/-127 bytes and these
// bodies are longer than that -- the same trap that broke the MSX1 build of the unrolled
// VRAM fill.
//
// WHY ASSEMBLY. The C version was 689 bytes. SDCC reloads the table pointer and redoes the
// index arithmetic on every iteration, and emits the key-walk and the cascade twice (once
// in encode, once in decode). In asm the cursors stay in register pairs across the loop and
// both functions share one character scan. On MSX, ROM is the scarce resource.
//
// Dead ends, measured, DO NOT retry:
//   - caching g_CryptKey/Map/Code in C locals: +34 B AND +8,000 cycles. Too few registers,
//     so the locals spill to the stack, and a stack access costs more than an
//     absolute-addressed global load (ld hl,(nn) = 16 T).
//   - two u8 accumulators to dodge 16-bit ops: +41 B AND +82,000 cycles.
#include "crypt.h"

//=============================================================================
// DEFAULT TABLES (ours)
//=============================================================================

// 32 characters a player can read off a screen and type back without ambiguity:
// no I/1/l, no O/0.
static const c8 s_DefaultMap[33] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

// One distinct bit per input bit: bits 0-4 -> character 1, bits 5-7 -> character 2.
static const u16 s_DefaultCode[8] =
{
	0x0001, 0x0002, 0x0004, 0x0008, 0x0010,
	0x0100, 0x0200, 0x0400,
};

const c8*  g_CryptKey  = (const c8*)0;
const c8*  g_CryptMap  = s_DefaultMap;
const u16* g_CryptCode = s_DefaultCode;

//=============================================================================
// SCRATCH
//=============================================================================
// The Z80 has nowhere near enough register pairs for src + dst + key + code + map + two
// cascade accumulators + a counter. These are the COLD ones -- touched once per byte, not
// once per bit -- so they live in memory and the hot ones stay in registers.

u16 g_Crypt_Src;     // source cursor
u16 g_Crypt_Dst;     // destination cursor
u16 g_Crypt_Kp;      // key cursor
u8  g_Crypt_Prev[2]; // cascade: previous index 1, previous index 2
u8  g_Crypt_Cnt;     // bytes remaining

//=============================================================================
// FUNCTIONS
//=============================================================================

// Crypt_Encode(data=HL, size@SP+2, str@SP+3) -> A = TRUE/FALSE. Callee cleans 3 bytes.
bool Crypt_Encode(const void* data, u8 size, c8* str) __naked
{
	(void)data; (void)size; (void)str;
	__asm
		ld   (_g_Crypt_Src), hl
		ld   hl, #2
		add  hl, sp
		ld   a, (hl)                ; size
		ld   (_g_Crypt_Cnt), a
		inc  hl
		ld   e, (hl)
		inc  hl
		ld   d, (hl)
		ld   (_g_Crypt_Dst), de     ; str

		ld   hl, (_g_CryptKey)      ; a key is mandatory
		ld   a, h
		or   a, l
		jp   Z, enc_fail
		ld   a, (hl)
		or   a, a
		jp   Z, enc_fail
		ld   (_g_Crypt_Kp), hl
		xor  a, a
		ld   (_g_Crypt_Prev), a
		ld   (_g_Crypt_Prev + 1), a

	enc_loop:
		ld   a, (_g_Crypt_Cnt)
		or   a, a
		jp   Z, enc_done
		dec  a
		ld   (_g_Crypt_Cnt), a

		ld   hl, (_g_Crypt_Src)     ; val = *src++ ^ *key++
		ld   a, (hl)
		inc  hl
		ld   (_g_Crypt_Src), hl
		ld   hl, (_g_Crypt_Kp)
		xor  a, (hl)
		inc  hl
		ld   c, a                   ; C = val
		ld   a, (hl)
		or   a, a
		jr   NZ, enc_key
		ld   hl, (_g_CryptKey)      ; wrap the key
	enc_key:
		ld   (_g_Crypt_Kp), hl

		ld   de, #0x0000            ; DE = accumulator
		ld   hl, (_g_CryptCode)
		ld   a, c
	enc_spread:
		or   a, a                   ; bits above the top set bit contribute nothing:
		jr   Z, enc_casc            ; stop early, and no bit counter is needed
		srl  a                      ; carry = bit 0
		jr   NC, enc_skip
		ld   b, a                   ; acc |= *cp   (16-bit; park val in B)
		ld   a, e
		or   a, (hl)
		ld   e, a
		inc  hl
		ld   a, d
		or   a, (hl)
		ld   d, a
		dec  hl
		ld   a, b
	enc_skip:
		inc  hl                     ; ++cp (entries are u16)
		inc  hl
		jr   enc_spread

	enc_casc:
		ld   hl, #_g_Crypt_Prev     ; cascade both indices, mod 32
		ld   a, (hl)
		add  a, e
		and  a, #31
		ld   (hl), a
		ld   c, a                   ; C = index 1
		inc  hl
		ld   a, (hl)
		add  a, d
		and  a, #31
		ld   (hl), a
		ld   b, a                   ; B = index 2

		ld   hl, (_g_CryptMap)      ; map both indices to characters
		ld   e, c
		ld   d, #0
		add  hl, de
		ld   c, (hl)
		ld   hl, (_g_CryptMap)
		ld   e, b
		ld   d, #0
		add  hl, de
		ld   b, (hl)

		ld   hl, (_g_Crypt_Dst)     ; *dst++ = both
		ld   (hl), c
		inc  hl
		ld   (hl), b
		inc  hl
		ld   (_g_Crypt_Dst), hl
		jp   enc_loop

	enc_done:
		ld   hl, (_g_Crypt_Dst)
		ld   (hl), #0x00            ; terminate
		ld   a, #1                  ; TRUE
		jr   enc_ret
	enc_fail:
		xor  a, a                   ; FALSE
	enc_ret:
		pop  hl                     ; return address
		pop  bc                     ; discard size + str low
		inc  sp                     ; discard str high  (callee-clean: 3 bytes)
		jp   (hl)
	__endasm;
}

// Crypt_Decode(str=HL, data=DE) -> A = TRUE/FALSE.
bool Crypt_Decode(const c8* str, void* data) __naked
{
	(void)str; (void)data;
	__asm
		ld   (_g_Crypt_Src), hl
		ld   (_g_Crypt_Dst), de

		ld   hl, (_g_CryptKey)
		ld   a, h
		or   a, l
		jp   Z, dec_fail
		ld   a, (hl)
		or   a, a
		jp   Z, dec_fail
		ld   (_g_Crypt_Kp), hl
		xor  a, a
		ld   (_g_Crypt_Prev), a
		ld   (_g_Crypt_Prev + 1), a

	dec_loop:
		ld   hl, (_g_Crypt_Src)     ; a PAIR of characters, or we are done
		ld   a, (hl)
		or   a, a
		jp   Z, dec_ok
		inc  hl
		ld   c, a                   ; C = first character
		ld   a, (hl)
		or   a, a
		jp   Z, dec_ok              ; odd trailing character: stop
		inc  hl
		ld   (_g_Crypt_Src), hl
		ld   b, a                   ; B = second character

		ld   a, c                   ; index each in the map
		call ck_index
		cp   a, #32
		jp   Z, dec_fail            ; not a code character: a typo'd password
		ld   c, a
		ld   a, b
		call ck_index
		cp   a, #32
		jp   Z, dec_fail
		ld   b, a

		ld   hl, #_g_Crypt_Prev     ; undo the cascade
		ld   a, c
		sub  a, (hl)
		and  a, #31
		ld   e, a                   ; E = accumulator low
		ld   (hl), c                ; prev1 = index 1
		inc  hl
		ld   a, b
		sub  a, (hl)
		and  a, #31
		ld   d, a                   ; D = accumulator high
		ld   (hl), b                ; prev2 = index 2

		ld   hl, (_g_CryptCode)     ; un-scatter, MSB first, shifting val along --
		ld   bc, #16                ; no 1<<bit mask needed
		add  hl, bc
		ld   b, #8
		ld   c, #0                  ; C = val
	dec_bits:
		dec  hl                     ; --cp (entries are u16)
		dec  hl
		sla  c
		ld   a, e
		and  a, (hl)
		jr   NZ, dec_set
		inc  hl
		ld   a, d
		and  a, (hl)
		dec  hl
		jr   Z, dec_next
	dec_set:
		set  0, c
	dec_next:
		djnz dec_bits

		ld   a, c                   ; *dst++ = val ^ *key++
		ld   hl, (_g_Crypt_Kp)
		xor  a, (hl)
		inc  hl
		ld   c, a
		ld   a, (hl)
		or   a, a
		jr   NZ, dec_key
		ld   hl, (_g_CryptKey)      ; wrap the key
	dec_key:
		ld   (_g_Crypt_Kp), hl
		ld   hl, (_g_Crypt_Dst)
		ld   (hl), c
		inc  hl
		ld   (_g_Crypt_Dst), hl
		jp   dec_loop

	dec_ok:
		ld   a, #1
		ret
	dec_fail:
		xor  a, a
		ret

	; ck_index: A = character -> A = its index, or 32 if the map does not contain it.
	; Shared by both characters -- the C version emitted this scan twice.
	; Clobbers HL, DE, A. Preserves B and C.
	ck_index:
		ld   hl, (_g_CryptMap)
		ld   d, a                   ; D = wanted character
		ld   e, #0                  ; E = index
	ck_scan:
		ld   a, (hl)
		cp   a, d
		jr   Z, ck_hit
		inc  hl
		inc  e
		ld   a, e
		cp   a, #32
		jr   NZ, ck_scan
	ck_hit:
		ld   a, e
		ret
	__endasm;
}
