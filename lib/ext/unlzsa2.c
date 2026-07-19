// msxmvl EXTENSION module: unlzsa2 -- runtime LZSA2 decompressor.
//
// NOT clean-room. This is the size-optimized LZSA2 Z80 decompressor by spke &
// uniabis (from Emmanuel Marty's LZSA, https://github.com/emmanuel-marty/lzsa),
// transliterated to sdasz80 syntax and wrapped as an SDCC --sdcccall 1 naked
// function. It is an ALTERED version per clause 2 of the zlib licence below.
// The ROM-safe forward configuration is used (no self-modifying code) so the
// decoder runs from ROM. One adaptation from the reference: the match offset is
// kept in a RAM global (g_LZSA2_Off) rather than IX, because SDCC-z80 uses IX as
// the callee-saved frame pointer -- clobbering it corrupts the C caller. The
// nibble reservoir lives in AF', so this routine is NOT AF'-safe -- do not call
// it from a path that must preserve the shadow accumulator.
//
// -------------------------------------------------------------------------
//  Size-optimized LZSA2 decompressor by spke & uniabis (134 bytes)
//  Z80 decompressor written by introspec (spke), optimizations by uniabis.
//  LZSA2 compression algorithms are (c) 2019 Emmanuel Marty.
//  Data must be packed with:  lzsa -f2 -r <infile> <outfile>
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from the
//  use of this software.
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//  1. The origin of this software must not be misrepresented; you must not claim
//     that you wrote the original software.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software. (Altered: transliterated to
//     sdasz80 and wrapped for SDCC by the msxmvl project.)
//  3. This notice may not be removed or altered from any source distribution.
// -------------------------------------------------------------------------
//
// Toolchain: SDCC --sdcccall 1. arg1 (src) -> HL, arg2 (dst) -> DE, matching the
// reference contract (HL=src, DE=dst), so the naked body is the decoder itself.
#include "unlzsa2.h"

u16 g_LZSA2_Off;   // last match offset (negative); RAM-held so IX is left intact

void UnLZSA2(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;            // src in HL, dst in DE (sdcccall 1)
	__asm
		xor  a
		ld   b, a
		ex   af, af'
		jr   l2_readtoken

	l2_case00x:
		push af
		call l2_rn_skip
		ld   c, a
		pop  af
		cp   #0x20
		rl   c
		jr   l2_saveoff
	l2_case0xx:
		dec  b
		cp   #0x40
		jr   C, l2_case00x
	l2_case01x:
		cp   #0x60
	l2_dorlb:
		rl   b
	l2_offreadc:
		ld   c, (hl)
		inc  hl
	l2_saveoff:
		ld   (_g_LZSA2_Off), bc
		ld   b, #0
	l2_matchlen:
		and  #0x07
		add  a, #2
		cp   #9
		call Z, l2_extcode
	l2_copymatch:
		ld   c, a
		push hl
		ld   hl, (_g_LZSA2_Off)
		add  hl, de
		ldir
		pop  hl
	l2_readtoken:
		ld   a, (hl)
		inc  hl
		push af
		and  #0x18
		jr   Z, l2_nolit
		rrca
		rrca
		rrca
		call PE, l2_extcode
		ld   c, a
		ldir
	l2_nolit:
		pop  af
		or   a
		jp   P, l2_case0xx
	l2_case1xx:
		cp   #0xC0
		jr   C, l2_case10x
		cp   #0xE0
		jr   NC, l2_matchlen
	l2_case110:
		ld   b, (hl)
		inc  hl
		jr   l2_offreadc
	l2_case10x:
		call l2_readnibble
		ld   b, a
		ld   a, c
		cp   #0xA0
		dec  b
		jr   l2_dorlb

	l2_extcode:
		call l2_readnibble
		inc  a
		jr   Z, l2_extrabyte
		sub  #0xF1
		add  a, c
		ret
	l2_extrabyte:
		ld   a, #15
		add  a, c
		add  a, (hl)
		inc  hl
		ret  NC
		ld   a, (hl)
		inc  hl
		ld   b, (hl)
		inc  hl
		ret  NZ
		pop  bc

	l2_readnibble:
		ld   c, a
	l2_rn_skip:
		xor  a
		ex   af, af'
		ret  M
		ld   a, (hl)
		or   #0xF0
		ex   af, af'
		ld   a, (hl)
		inc  hl
		or   #0x0F
		rrca
		rrca
		rrca
		rrca
		ret
	__endasm;
}
