// msxmvl clean-room reimplementation of MSXgl "compress" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL, arg2 -> DE, arg3+ -> stack. return 16b -> HL.
//
// RLEp is a simple run-length format made of chunks. Each chunk starts with a
// 1-byte header:
//   bits 7-6 = Type (0-3), bits 5-0 = Count (stored value, real count = +1)
//     Type 0: emit the default value, (Count+1) times.
//     Type 1: read 1 literal byte, emit it (Count+1) times.
//     Type 2: read 2 literal bytes, emit that pair (Count+1) times.
//     Type 3: read (Count+1) literal bytes and copy them verbatim.
//
// COMPRESS_USE_RLEP_DEFAULT: when set, the first source byte is the "default"
// value used by Type-0 runs; otherwise Type-0 runs emit zeros.
// COMPRESS_USE_RLEP_FIXSIZE: when set, the caller passes the exact output size
// and unpacking stops once that many bytes are produced; otherwise a header
// byte of 0x00 terminates the stream.
//
// The header declares no __PRESERVES() contract, so this follows the standard
// SDCC convention (AF/BC/DE/HL/IX caller-clobberable; IY untouched).

#include "compress.h"
#include "memory.h"	// Mem_Set / Mem_Copy (LDIR-based) for the RLE fills/copies

#if (COMPRESS_USE_RLEP)

// HAND-WRITTEN ASM for the default configuration (RLEP_DEFAULT + no FIXSIZE). The other
// configurations still use the C body below -- there is no point hand-coding four variants of
// a routine only one of which anybody builds.
//
// The C version was 2% slower than MSXgl despite being the better C: we read the chunk header
// ONCE where MSXgl reads *src twice. Three attempts to win it back in C all lost (see the
// rejected-experiment note in the type-2 branch). The cost is not in the C at all -- it is that
// BOTH versions call Mem_Set / Mem_Copy once per chunk, and every call pays SDCC's stack-arg
// setup and the callee's stack teardown for a run that is often only a handful of bytes.
//
// In asm the fills and copies are inline: LDIR for a literal run, an overlapping store loop for
// a byte run. No call, no stack traffic, no per-chunk prologue.
//
//     HL = src   DE = dst   C = count (1..64)   B = scratch/counter   A = value
//
// Contract pinned by test/gen/rlep_check.c: every chunk type, count==1 (the minimum -- the
// header stores count-1), count==64 (the maximum a 6-bit field holds), type 2 writing count*2
// bytes rather than count, no overrun past the output, and the returned length. It passed
// against the C version before this replaced it.
#if ((COMPRESS_USE_RLEP_DEFAULT) && !(COMPRESS_USE_RLEP_FIXSIZE))

static u8  g_RLEp_Def;      // the stream's default byte (type-0 runs)
static u16 g_RLEp_Start;    // where the output began, so the length can be returned

u16 RLEp_UnpackToRAM(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;
	__asm
		; HL = src, DE = dst  (--sdcccall 1: arg1 -> HL, arg2 -> DE)
		ld   (_g_RLEp_Start), de
		ld   a, (hl)               ; the default value for this stream
		inc  hl
		ld   (_g_RLEp_Def), a

	1$:                            ; --- next chunk ---
		ld   a, (hl)               ; header: type in bits 7-6, count-1 in bits 5-0
		or   a
		jr   z, 9$                 ; header 0x00 terminates the stream
		inc  hl

		ld   b, a
		and  #0x3F
		inc  a
		ld   c, a                  ; C = count, 1..64
		ld   a, b
		rlca
		rlca
		and  #0x03                 ; A = type

		jr   z, 2$                 ; 0: run of the default byte
		dec  a
		jr   z, 3$                 ; 1: run of one repeated byte
		dec  a
		jr   z, 4$                 ; 2: run of a repeated 2-byte pattern

		ld   b, #0                 ; 3: literal run -- BC = count, straight LDIR
		ldir                       ; advances HL and DE by count for free
		jr   1$

	2$:                            ; type 0
		ld   a, (_g_RLEp_Def)
		jr   5$

	3$:                            ; type 1
		ld   a, (hl)               ; the byte to repeat
		inc  hl

	5$:                            ; fill C bytes of A at (DE)
		ld   b, c                  ; C is 1..64, so B is never 0 -- djnz is safe
	6$:
		ld   (de), a
		inc  de
		djnz 6$
		jr   1$

	4$:                            ; type 2: C PAIRS = C*2 bytes
		; Read the pattern back through (HL) each pass rather than caching it in a register.
		; The Z80 can only store the accumulator to (DE) -- there is no `ld (de),b` -- so a
		; cached second byte would have to be shuffled through A anyway, and the alternatives
		; are worse: push/pop AF costs 21 T a pair, and EX AF,AF-shadow is unsafe because the
		; MSX interrupt handler uses the shadow set.
		ld   b, c                  ; B = pair count (1..64, so djnz is safe)
	7$:
		ld   a, (hl)               ; first byte of the pattern
		ld   (de), a
		inc  de
		inc  hl
		ld   a, (hl)               ; second byte
		ld   (de), a
		inc  de
		dec  hl                    ; back to the first byte for the next pass
		djnz 7$
		inc  hl                    ; step over the 2 pattern bytes
		inc  hl
		jr   1$

	9$:                            ; --- done: return dst - start ---
		ld   hl, (_g_RLEp_Start)
		ex   de, hl                ; HL = dst (end), DE = start
		or   a                     ; clear carry
		sbc  hl, de                ; HL = bytes written
		ex   de, hl                ; a u16 return goes in DE under --sdcccall 1
		ret
	__endasm;
}

#else

u16 RLEp_UnpackToRAM(const u8* src, u8* dst RLEP_FIXSIZE_PARAM)
{
	u8* start = dst;

#if (COMPRESS_USE_RLEP_DEFAULT)
	u8 def = *src++;
#else
	const u8 def = 0;
#endif

#if (COMPRESS_USE_RLEP_FIXSIZE)
	while (((u16)dst - (u16)start) < size)
#else
	while (*src != 0)
#endif
	{
		u8 header = *src++;
		u8 type   = header >> 6;
		u8 count  = (header & 0x3F) + 1;

		// Fills/copies via the LDIR-based Mem_Set/Mem_Copy (matches MSXgl) — far faster than
		// per-byte loops for the common medium/large runs; dst is advanced by `count` at the end.
		if (type == 0)			// Sequence of the default value
		{
			Mem_Set(def, dst, count);
		}
		else if (type == 1)		// Sequence of a repeated single byte
		{
			Mem_Set(*src, dst, count);
			src++;
		}
		else if (type == 2)		// Sequence of a repeated 2-byte pattern
		{
			// TRIED AND REJECTED: hoisting the pair into locals and writing it two bytes at
			// a time (`u8 a=src[0], b=src[1]; do { *d++=a; *d++=b; } while (--n);`) on the
			// theory that `dst[i] = src[i & 1]` re-reads the same two values every byte.
			// Measured: 125,219 -> 133,507 cycles AND 316 -> 331 bytes. WORSE ON BOTH. SDCC
			// generates a tighter loop from the indexed form than from the pointer-walk one.
			// The remaining -2% vs MSXgl is real, but it is not here.
			u8 i;
			count <<= 1;		// now the total byte count; shared `dst += count` below covers it
			for (i = 0; i < count; ++i)
				dst[i] = src[i & 1];
			src += 2;
		}
		else					// type == 3: uncompressed literal run
		{
			Mem_Copy(src, dst, count);
			src += count;
		}
		dst += count;
	}

	return (u16)dst - (u16)start;
}

#endif

#endif // (COMPRESS_USE_RLEP)
