// msxmvl EXTENSION module: unzx0 -- runtime ZX0 decompressor.
//
// Decompressor for the ZX0 format by Einar Saukas (format only; independent
// implementation). Clean-room: the RAM decoder below was written from the ZX0 v2
// format (interlaced Elias-gamma lengths, LZ literals/copies, last-offset reuse),
// with the reference decoder used only as a behavioural oracle. See
// docs/Compression.md for the measured size/speed comparison.
//
// Toolchain: SDCC --sdcccall 1. arg1 -> HL, arg2 -> DE, so UnZX0's naked body is
// the decoder itself -- no argument shuffling and no wrapper, which is why the
// C-callable routine is the same 69 bytes as the bare reference core.
#include "unzx0.h"

//=============================================================================
// RAM DECODER (hand-written, the measured hot path)
//=============================================================================
// Register use across the loop: HL = compressed source, DE = output cursor,
// BC = run length / LDIR counter, A = an 8-bit MSB-first bit reservoir (the
// 0x80 seed doubles as an "empty, reload" sentinel), and the last match offset
// kept on the stack as a negative 16-bit value (so `add hl,de` yields dst-off).
void UnZX0(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;   // src in HL, dst in DE (sdcccall 1)
	__asm
		ld   a, #0x80          ; reservoir empty -> first bit forces a reload
		ld   bc, #0xffff       ; default last offset = 1  (stored as -1)
		push bc
		inc  bc                ; BC = 0 for the first Elias-gamma
	zx0_literals:
		call zx0_gamma         ; BC = literal run length
		ldir                   ; emit literals straight from the stream
		add  a, a              ; control bit: 0 = reuse offset, 1 = new offset
		jr   C, zx0_newoffset
		call zx0_gamma         ; BC = match length (offset reused)
	zx0_match:
		ex   (sp), hl          ; HL = last offset (neg); park source on the stack
		push hl
		add  hl, de            ; HL = dst - offset
		ldir                   ; copy the match
		pop  hl
		ex   (sp), hl          ; restore source; offset stays on the stack
		add  a, a              ; 0 = back to literals, 1 = another new offset
		jr   NC, zx0_literals
	zx0_newoffset:
		call zx0_gamma         ; ((offset-1)>>7)+1 ; wraps to 256 at the end marker
		.db  0x08              ; ex af,af-shadow : stash reservoir + carry
		pop  af                ; drop the previous offset
		xor  a, a
		sub  a, c              ; A = -gamma  (0 => gamma was 256 => end of stream)
		ret  Z
		ld   b, a              ; B = high byte of the negative offset
		.db  0x08              ; ex af,af-shadow : recover reservoir + carry
		ld   c, (hl)           ; C = offset low byte from the stream
		inc  hl
		rr   b                 ; fold the first length bit out of the offset
		rr   c
		push bc                ; save the new offset (negative)
		ld   bc, #1
		call NC, zx0_gamma_more ; more length bits when that first bit was 0
		inc  bc                ; length = value + 1
		jr   zx0_match
	;; interlaced Elias-gamma: value >= 1 into BC, MSB-first from the reservoir
	zx0_gamma:
		inc  c
	zx0_gamma_loop:
		add  a, a
		jr   NZ, zx0_gamma_bit
		ld   a, (hl)           ; reservoir spent: pull the next 8 bits
		inc  hl
		rla
	zx0_gamma_bit:
		ret  C
	zx0_gamma_more:
		add  a, a
		rl   c
		rl   b
		jr   zx0_gamma_loop
	__endasm;
}

//=============================================================================
// FAST RAM DECODER (bigger, ~19% faster; still ROM-safe)
//=============================================================================
// Same ZX0 format and byte-identical output as UnZX0, tuned for speed: the
// per-token Elias reads are inlined (no call/ret) and the last match offset lives
// in a fixed RAM word instead of on the stack, so a match copy is a plain
// `ld hl,(offset)` + `add hl,de` with no `ex (sp),hl` juggling. This is the
// structure of Einar Saukas's "turbo" decoder, but where turbo self-modifies its
// own `ld hl,imm` operand (which forces the decoder to run from RAM), we keep the
// offset in RAM DATA (`ld hl,(g_ZX0_FastOff)`) -- 1 T/byte slower than turbo but
// FULLY ROM-SAFE, at the same size. Measured (4 KB corpus, openMSX): 128 bytes,
// 72.5 T/byte vs UnZX0's 89.8 and turbo's 71.4 (turbo being RAM-only).
//
// Like UnZX0 it uses the alternate AF register; mask interrupts around the call
// if an interrupt handler clobbers AF'.

u16 g_ZX0_FastOff;   // last match offset (negative); the "turbo" self-mod slot, in RAM

// UnZX0_fast(src=HL, dst=DE): decompress a ZX0 stream into RAM, the fast path.
void UnZX0_fast(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;
	__asm
		ld   bc, #0xffff
		ld   (_g_ZX0_FastOff), bc   ; default last offset = 1  (stored as -1)
		inc  bc
		ld   a, #0x80
		jr   zf_literals
	zf_newoffset:
		inc  c                      ; new offset: high part via inlined Elias
		add  a, a
		jp   NZ, zf_newoffset_skip
		ld   a, (hl)
		inc  hl
		rla
	zf_newoffset_skip:
		call NC, zf_elias
		.db  0x08                   ; ex af,af-shadow : stash reservoir
		xor  a, a
		sub  a, c
		ret  Z                      ; end marker
		ld   b, a
		.db  0x08                   ; ex af,af-shadow : recover reservoir
		ld   c, (hl)
		inc  hl
		rr   b
		rr   c
		ld   (_g_ZX0_FastOff), bc   ; store the new offset (the turbo self-mod slot)
		ld   bc, #1
		call NC, zf_elias
		inc  bc
	zf_copy:
		push hl                     ; park source
		ld   hl, (_g_ZX0_FastOff)   ; offset (negative) straight from RAM
		add  hl, de                 ; HL = dst - offset
		ldir
		pop  hl
		add  a, a
		jr   C, zf_newoffset
	zf_literals:
		inc  c                      ; literal run length via inlined Elias
		add  a, a
		jp   NZ, zf_literals_skip
		ld   a, (hl)
		inc  hl
		rla
	zf_literals_skip:
		call NC, zf_elias
		ldir
		add  a, a
		jr   C, zf_newoffset
		inc  c                      ; reused-offset match length via inlined Elias
		add  a, a
		jp   NZ, zf_reuse_skip
		ld   a, (hl)
		inc  hl
		rla
	zf_reuse_skip:
		call NC, zf_elias
		jp   zf_copy
	;; interlaced Elias-gamma continuation (unrolled first groups, then a loop)
	zf_elias:
		add  a, a
		rl   c
		add  a, a
		jr   NC, zf_elias
		ret  NZ
		ld   a, (hl)
		inc  hl
		rla
		ret  C
		add  a, a
		rl   c
		add  a, a
		ret  C
		add  a, a
		rl   c
		add  a, a
		ret  C
		add  a, a
		rl   c
		add  a, a
		ret  C
	zf_elias_loop:
		add  a, a
		rl   c
		rl   b
		add  a, a
		jr   NC, zf_elias_loop
		ret  NZ
		ld   a, (hl)
		inc  hl
		rla
		jr   NC, zf_elias_loop
		ret
	__endasm;
}

//=============================================================================
// VRAM DECODER (streams to the VDP; correctness over raw speed)
//=============================================================================
// The reference format has no VRAM variant: LZ back-references need the already
// produced output, which here lives in VRAM, so matches are resolved by reading
// VRAM back a byte at a time (this also makes overlapping RLE-style runs fall out
// for free). Written in C for clarity -- it is a one-shot asset load, not a hot
// loop, and is not part of the measured comparison.

__sfr __at(0x98) g_ZX0_P98;   // VRAM data (auto-increment)
__sfr __at(0x99) g_ZX0_P99;   // register / address setup

// Bit reader state (module-local; UnZX0_VRAM is not re-entrant).
static const u8* s_ZX0_Src;
static u8 s_ZX0_Buf;
static u8 s_ZX0_Cnt;

static u8 ZX0_Bit(void)
{
	if (s_ZX0_Cnt == 0) { s_ZX0_Buf = *s_ZX0_Src++; s_ZX0_Cnt = 8; }
	s_ZX0_Cnt--;
	u8 b = s_ZX0_Buf >> 7;
	s_ZX0_Buf <<= 1;
	return b;
}

// interlaced Elias-gamma; returns a value >= 1 (256 marks end of stream).
static u16 ZX0_Gamma(void)
{
	u16 v = 1;
	while (ZX0_Bit() == 0)
		v = (u16)((v << 1) | ZX0_Bit());
	return v;
}

// Point the VDP at a 14-bit VRAM address for writing (wr=1) or reading (wr=0).
// On a TMS9918 (MSX1) a freshly-set read address needs a short settle before the
// data byte is valid (same as VDP_Peek_16K); the whole routine runs under DI, so
// the 0x99 write pair itself is already ISR-safe.
static void ZX0_VAddr(u16 addr, u8 wr)
{
	g_ZX0_P99 = (u8)addr;
	g_ZX0_P99 = (wr ? 0x40 : 0x00) | ((u8)(addr >> 8) & 0x3F);
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
	if (!wr) { __asm nop __endasm; __asm nop __endasm; __asm nop __endasm; __asm nop __endasm; }
#endif
}

void UnZX0_VRAM(const u8* src, u16 vramDst, u16 len)
{
	u16 pos = 0;         // bytes produced so far (offset from vramDst)
	u16 lastOff = 1;
	u8  newOffset;

	s_ZX0_Src = src;
	s_ZX0_Cnt = 0;

	__asm di __endasm;   // keep the VDP address latch coherent against the ISR
#if (!defined(MSX_VERSION) || (MSX_VERSION != MSX_1))
	// R#14 = the 16 KB page of the destination (region stays inside one page).
	// A TMS9918 (MSX1) has no R#14 and only 16 KB of VRAM, so skip it there
	// (writing "register 14" would alias onto R#6).
	g_ZX0_P99 = (u8)((vramDst >> 14) & 0x07);
	g_ZX0_P99 = 0x80 | 14;
#endif

	// The stream always begins with a (possibly empty) literal run.
	for (;;) {
		u16 n = ZX0_Gamma();               // literal run length
		ZX0_VAddr(vramDst + pos, 1);
		while (n--) { g_ZX0_P98 = *s_ZX0_Src++; pos++; }

		newOffset = ZX0_Bit();             // 0 = reuse offset, 1 = new offset
		if (!newOffset) {
			n = ZX0_Gamma();               // match length (offset reused)
			goto copy;
		}

	new_offset:
		{
			u16 g = ZX0_Gamma();           // offset high part (256 => done)
			if (g == 256) break;
			u8 lsb = *s_ZX0_Src++;
			u8 hi  = (u8)(0u - g);         // negative-offset high byte
			u8 nb  = (u8)(0x80 | (hi >> 1));
			u8 nc  = (u8)(((hi & 1) << 7) | (lsb >> 1));
			u8 cout = lsb & 1;             // first length bit
			u16 negOff = (u16)((nb << 8) | nc);
			lastOff = (u16)(0x10000u - negOff);
			if (cout) {
				n = 2;
			} else {
				u16 v = 1;
				do { v = (u16)((v << 1) | ZX0_Bit()); } while (ZX0_Bit() == 0);
				n = (u16)(v + 1);
			}
		}

	copy:
		{
			u16 rp = vramDst + pos - lastOff;   // match source in VRAM
			while (n--) {
				u8 v;
				ZX0_VAddr(rp, 0); v = g_ZX0_P98;      // read one byte back
				ZX0_VAddr(vramDst + pos, 1); g_ZX0_P98 = v;
				rp++; pos++;
			}
		}

		if (ZX0_Bit()) goto new_offset;    // else fall back to the literal run
	}

	(void)len;                             // output size is implicit in the stream
	__asm ei __endasm;
}
