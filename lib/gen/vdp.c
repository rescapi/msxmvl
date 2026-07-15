// msxmvl clean-room reimplementation of MSXgl "vdp" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/L(8b when arg1 8b), arg3+ -> stack.
//   return 8b->A, 16b->HL. __FASTCALL single arg -> HL(16b)/A(8b).
//
// Register-preservation contracts (__PRESERVES) are honoured by hand-written
// __naked asm for the constrained routines. Everything else is plain C so the
// observable VRAM/RAM/return results match a real-MSXgl build byte-for-byte.
//
// VDP hardware ports (MSX wiring):
//   0x98 VRAM data (auto-increment)   0x99 register/address setup + status read
//   0x9A palette (R#16 pointer)       0x9B register indirect (R#17 pointer)

#include "vdp.h"

//=============================================================================
// PORT DECLARATIONS
//=============================================================================
__sfr __at(0x98) g_P98;   // VRAM data
__sfr __at(0x99) g_P99;   // register/address write ; status read
__sfr __at(0x9A) g_P9A;   // palette
__sfr __at(0x9B) g_P9B;   // register indirect

//=============================================================================
// GLOBALS
//=============================================================================
u8 g_VDP_REGSAV[28];
VDP_Data g_VDP_Data;
VDP_Command g_VDP_Command;
VDP_Sprite g_VDP_Sprite;

u16 g_ScreenLayoutLow;
u16 g_ScreenColorLow;
u16 g_ScreenPatternLow;
u16 g_SpriteAttributeLow;
u16 g_SpritePatternLow;
u16 g_SpriteColorLow;
u8 g_ScreenLayoutHigh;
u8 g_ScreenColorHigh;
u8 g_ScreenPatternHigh;
u8 g_SpriteAttributeHigh;
u8 g_SpritePatternHigh;
u8 g_SpriteColorHigh;

// BIOS register-mirror source addresses (used by VDP_Initialize)
#define M_RG0SAV	0xF3DF	// RG0SAV..RG7SAV : 8 bytes
#define M_RG08SAV	0xFFE7	// RG08SAV..RG23SAV : 16 bytes

//=============================================================================
// DIRECT REGISTER ACCESS
//=============================================================================

// VDP_RegWrite(reg=A, value=L). Preserve BC,DE,IY. (H free)
void VDP_RegWrite(u8 reg, u8 value) __PRESERVES(b, c, d, e, iyl, iyh) __naked
{
	(void)reg; (void)value;
	__asm
		ld   h, a          ; H = reg
		ld   a, l          ; A = value
		out  (0x99), a     ; write data byte first
		ld   a, h
		or   #0x80         ; reg | 0x80
		out  (0x99), a
		ret
	__endasm;
}

// VDP_RegWriteBak(reg=A, value=L). Write register AND store new value in the
// shadow g_VDP_REGSAV[reg]. Preserve DE,IY. (A,BC,HL free)
void VDP_RegWriteBak(u8 reg, u8 value) __PRESERVES(d, e, iyl, iyh) __naked
{
	(void)reg; (void)value;
	__asm
		ld   c, a          ; C = reg
		ld   b, #0
		ld   a, l          ; A = value
		ld   hl, #_g_VDP_REGSAV
		add  hl, bc        ; HL = &g_VDP_REGSAV[reg]
		ld   (hl), a       ; shadow[reg] = value  (store BEFORE outs -> no push/pop)
		di                 ; protect the 2-write 0x99 sequence from ISR corruption (matches MSXgl safe default)
		out  (0x99), a     ; write value (data)
		ld   a, c
		or   #0x80
		out  (0x99), a     ; reg | 0x80
		ei
		ret
	__endasm;
}

// VDP_RegWriteBakMask: newval = (shadow[idx] & mask) | value, then RegWriteBak.
// idx=A, mask=L, value@(SP+2). __naked to skip SDCC's ix frame: computes the
// masked value in registers and tail-routes through VDP_RegWriteBak (which does
// the di/ei-protected 2-write 0x99 sequence). Callee-cleans the 1-byte stack arg.
void VDP_RegWriteBakMask(u8 idx, u8 mask, u8 value) __naked
{
	(void)idx; (void)mask; (void)value;
	__asm
		ld   c, a                 ; C = idx
		ld   a, l                 ; A = mask
		ld   b, #0
		ld   hl, #_g_VDP_REGSAV
		add  hl, bc               ; HL = &g_VDP_REGSAV[idx]
		and  (hl)                 ; A = shadow[idx] & mask
		ld   hl, #2
		add  hl, sp               ; HL -> value (stack arg)
		or   (hl)                 ; A |= value
		ld   l, a                 ; L = newval
		ld   a, c                 ; A = idx
		call _VDP_RegWriteBak     ; write + shadow (preserves DE,IY)
		pop  hl                   ; HL = return address
		inc  sp                   ; drop 1-byte stack arg (callee cleanup)
		jp   (hl)
	__endasm;
}

// VDP_ReadDefaultStatus: read currently-selected status (default S#0).
// Preserve everything except A,F.
u8 VDP_ReadDefaultStatus() __PRESERVES(b, c, d, e, h, l, iyl, iyh) __naked
{
	__asm
		in   a, (0x99)
		ret
	__endasm;
}

// VDP_ReadStatus(stat=A): select S#stat via R#15, read it, reset R#15 to 0.
// Preserve BC,DE,H,IY. (A,L free)
u8 VDP_ReadStatus(u8 stat) __PRESERVES(b, c, d, e, h, iyl, iyh) __naked
{
	(void)stat;
	__asm
		out  (0x99), a        ; value = stat
		ld   a, #(0x80 | 15)
		out  (0x99), a        ; R#15 = stat
		in   a, (0x99)        ; read S#stat
		ld   l, a             ; save result
		xor  a
		out  (0x99), a        ; value = 0
		ld   a, #(0x80 | 15)
		out  (0x99), a        ; R#15 = 0
		ld   a, l             ; restore result
		ret
	__endasm;
}

//=============================================================================
// HELPER
//=============================================================================

// Copy BIOS register mirrors into the shadow register buffer.
u8 g_VDPInitialized = 0;  // one-time init guard (matches MSXgl VDP_AUTO_INIT behavior)

void VDP_Initialize()
{
	// Copy the BIOS register-save areas via LDIR (matches MSXgl; far smaller/faster than byte loops).
	__asm
		ld   hl, #0xF3DF        ; M_RG0SAV  (RG0SAV..RG7SAV, 8 bytes)
		ld   de, #_g_VDP_REGSAV
		ld   bc, #8
		ldir
		ld   hl, #0xFFE7        ; M_RG08SAV (RG08SAV..RG23SAV, 16 bytes)
		ld   de, #_g_VDP_REGSAV+8
		ld   bc, #16
		ldir
	__endasm;
	g_VDP_Data.Mode = 0;   // BPC/Width/Height left zero (BSS), matching MSXgl (no explicit zeroing)
}

// Detect VDP version. Return value matches MSXgl on every VDP:
//   TMS9918A (MSX1) -> 0, V9938 -> 1, V9958 -> 2, newer -> raw ID.
// The TMS9918A has no S#1 ID register, so reading S#1 alone (as this used to) can
// never yield 0 and mis-reports an MSX1 as a V9938; it also returned id+1 for every
// VDP, giving 3 for a V9958 instead of 2. TMS9918A is instead detected by a VBlank
// timing trick (it exposes only S#0; a V99x8 exposes S#2's VR flag). That wait costs
// up to a frame -- the price of correct MSX1 detection, matching MSXgl.
u8 VDP_GetVersion() __NAKED
{
	__asm
		call  vgv_is_tms9918a          ; Z => TMS9918A (A already 0)
		ret   z
		ld    a, #1
		di
		out   (0x99), a                ; select S#1
		ld    a, #(0x80 | 15)
		out   (0x99), a
		in    a, (0x99)                ; read S#1
		and   #0x3E                    ; ID field (bits 1..5)
		rrca                           ; >> 1 => raw VDP ID
		ex    af, af'                  ; '
		xor   a
		out   (0x99), a                ; select S#0 (BIOS ISR expects R#15 = 0)
		ld    a, #(0x80 | 15)
		ei
		out   (0x99), a
		ex    af, af'                  ; '
		ret   nz                       ; V9958+ => raw ID (2 for V9958)
		inc   a                        ; V9938 (ID 0) => 1
		ret

	; Wait for the VBlank flag on S#0, then sample S#2's VR (bit 6): 1 on a V99x8,
	; 0 on a TMS9918A (which has no S#2 and mirrors S#0's 5S flag). Z => TMS9918A, A=0.
	vgv_is_tms9918a:
		xor   a
		di
		out   (0x99), a                ; select S#0
		ld    a, #(0x80 | 15)
		out   (0x99), a
		in    a, (0x99)                ; read S#0 to clear the interrupt flag
	vgv_wait:
		in    a, (0x99)                ; read S#0
		and   a
		jp    p, vgv_wait              ; spin until interrupt flag (bit 7) sets
		ld    a, #2
		out   (0x99), a                ; select S#2
		ld    a, #(0x80 | 15)
		out   (0x99), a
		in    a, (0x99)                ; read S#2 (or S#0 mirror on a TMS9918A)
		ex    af, af'                  ; '
		xor   a
		out   (0x99), a                ; select S#0
		ld    a, #(0x80 | 15)
		out   (0x99), a
		ld    a, (0xF3E6)              ; RG7SAV: restore R#7 if S#2 select mirrored to it
		out   (0x99), a
		ld    a, #(0x80 | 7)
		ei
		out   (0x99), a
		ex    af, af'                  ; '
		and   #0x40                    ; VR bit: 0 => TMS9918A (Z), 1 => V99x8 (NZ)
		ret
	__endasm;
}

//=============================================================================
// SHARED BULK TRANSFER ENGINES
//=============================================================================
// The four bulk VRAM routines (Write/Read x 16K/128K) differ ONLY in how they program the
// VDP address and how they clean their stack args. By the time the transfer starts, all four
// hold the identical state:
//
//     HL = buffer   C = 0x98 (data port)   D = whole 256-byte blocks   E = count low byte
//
// so the transfer itself is factored out here and CALLed. Previously each of the four
// open-coded it, and each carried TWO copies of the unrolled block (one for the remainder,
// one for the 256-blocks) -- eight copies of the same 32 bytes across the file.
//
// The call costs 27 T (call 17 + ret 10) ONCE per transfer: 0.6% on a 256-byte write, 0.04%
// on a screen-sized one. The per-byte rate is unchanged, which is the number that matters.
//
// ONE unrolled block, not two. The old split existed because OUTI's counter is B, so a single
// pass can move at most 256 bytes -- forcing a separate loop for the sub-16 remainder. Doing
// the 0..15 tail FIRST removes that: everything left is then a multiple of 16, so one pass
// loop covers the remainder-16s and the 256-blocks alike. The tail may lead because VRAM
// auto-increments and we still emit the bytes in source order -- tail + rem16 + 256*blocks
// == count, read straight up from HL.
//
// Do not "simplify" the jp nz below into a djnz: B is OUTI's own counter and each unrolled
// pass drops it by 16, which is exactly what makes the loop back-edge cost 10 T per 16 bytes.

// Bulk VRAM WRITE. In: HL=src, C=0x98, D=blocks, E=count low. Clobbers AF, BC, HL.
void VDP_BulkOut(void) __naked
{
	__asm
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		; TMS9918 needs ~29 T between VRAM accesses; OUTI alone (16 T) is too fast and the
		; VDP silently DROPS bytes. Pad to 30 T/byte (outi 16 + nop 4 + jp 10). No unroll:
		; the padding, not the loop overhead, sets the pace, so unrolling would buy nothing.
		ld   a, e
		or   a
		jr   z, 2$
		ld   b, e
	4$:
		outi
		nop
		jp   nz, 4$            ; E remainder bytes
	2$:
		ld   a, d
		or   a
		ret  z
	1$:
		ld   b, #0             ; B=0 => 256 iterations
	5$:
		outi
		nop
		jp   nz, 5$
		dec  a
		jr   nz, 1$
		ret
#else
		; V9938+: OUTI is 16 T, which clears the 15 T minimum VRAM gap, so it is display-safe.
		; 16 x OUTI (16 T) + JP (10 T) = 266 T / 16 bytes = 16.6 T/byte, against OTIR's 21.
		ld   a, e
		and  #0x0F
		jr   z, 1$
		ld   b, a
		otir                   ; the 0..15 tail, FIRST. <=15 bytes: OTIR (21 T/byte) beats
		                       ; OUTI+JP NZ (26 T/byte) -- unrolling only pays from 16 bytes up.
	1$:
		ld   a, e
		and  #0xF0
		ld   b, a              ; B = the whole 16s of the remainder (0..240)
		ld   a, d              ; A = number of 256-byte blocks   (neither LD touches flags)
		jr   z, 2$             ; Z is still from the AND: the remainder had no whole 16s
		inc  a                 ; ...it did, so run one extra pass, with B as set above
		jr   3$
	2$:
		or   a
		ret  z                 ; nothing beyond the tail
		ld   b, #0             ; B=0 => a full 256-byte pass
	3$:
		.rept 16
		outi
		.endm
		jp   nz, 3$            ; B != 0: stay in this pass
		dec  a
		ret  z
		ld   b, #0             ; next pass is a full 256 bytes
		jr   3$
#endif
	__endasm;
}

// Bulk VRAM READ. In: HL=dest, C=0x98, D=blocks, E=count low. Clobbers AF, BC, HL.
// Mirror of VDP_BulkOut with INI/INIR; see there for the reasoning.
void VDP_BulkIn(void) __naked
{
	__asm
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		ld   a, e
		or   a
		jr   z, 2$
		ld   b, e
	4$:
		ini
		nop
		jp   nz, 4$
	2$:
		ld   a, d
		or   a
		ret  z
	1$:
		ld   b, #0
	5$:
		ini
		nop
		jp   nz, 5$
		dec  a
		jr   nz, 1$
		ret
#else
		ld   a, e
		and  #0x0F
		jr   z, 1$
		ld   b, a
		inir                   ; the 0..15 tail, first
	1$:
		ld   a, e
		and  #0xF0
		ld   b, a
		ld   a, d
		jr   z, 2$
		inc  a
		jr   3$
	2$:
		or   a
		ret  z
		ld   b, #0
	3$:
		.rept 16
		ini
		.endm
		jp   nz, 3$
		dec  a
		ret  z
		ld   b, #0
		jr   3$
#endif
	__endasm;
}

//=============================================================================
// 16K (14-bit) VRAM ACCESS
//=============================================================================

// VDP_WriteVRAM_16K(src=HL, dest=DE, count@stack). count 0 => 65536. OUTI loop.
void VDP_WriteVRAM_16K(const u8* src, u16 dest, u16 count) __naked
{
	(void)src; (void)dest; (void)count;
	__asm
		push hl                ; save src
		ld   hl, #4            ; saved_src(2)+ret(2) -> count
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)           ; BC = count
		push bc                ; save count
		ld   a, e
		out  (0x99), a         ; addr low
		ld   a, d
		and  #0x3F
		or   #0x40             ; write-enable
		out  (0x99), a         ; addr high
		pop  de                ; DE = count (D=full 256-blocks, E=remainder)
		pop  hl                ; HL = src
		ld   c, #0x98          ; C = data port for OTIR
		call _VDP_BulkOut
		pop  hl                ; ret addr
		pop  bc                ; discard the 2-byte count stack arg (callee-clean)
		jp   (hl)
	__endasm;
}

// VDP_FillVRAM_16K(value=A, dest=DE, count@stack). count 0 => 65536.
void VDP_FillVRAM_16K(u8 value, u16 dest, u16 count) __naked
{
	(void)value; (void)dest; (void)count;
	__asm
		ld   c, a              ; C = value (saved)
		ld   a, e
		out  (0x99), a         ; addr low
		ld   a, d
		and  #0x3F
		or   #0x40
		out  (0x99), a         ; addr high (write)
		ld   hl, #2            ; ret(2) -> count
		add  hl, sp
		ld   e, (hl)           ; E = count low (remainder)
		inc  hl
		ld   d, (hl)           ; D = count high (256-blocks)
		; The old loop was OUT(12 T)+DJNZ(13 T) = 25 T/byte -- the DJNZ was doing double duty
		; as loop control AND as the inter-access gap. Unroll x16 and let NOPs be the gap
		; instead; the DJNZ cost is then amortised over 16 bytes:
		;
		;     16 x [OUT (12 T) + NOP + NOP (8 T)] + DJNZ (13 T) = 333 T / 16 = 20.8 T/byte
		;
		; 20 T, not 16: unlike the 128K variant, a 16K fill CAN be used in SCREEN 0, whose
		; minimum VRAM gap is 20 T (bitmap modes need only 15 T). Staying >=20 keeps this
		; safe in every mode, and still beats 25 T/byte by ~17%.
		; MSX1's TMS9918 wants ~29 T, so it pads further -- see the #if.
		; count == 0 means 65536 (the documented contract). The unrolled path computes
		; count>>4, and 0>>4 == 0 -- which would write NOTHING. Handle it before anything else.
		ld   a, d
		or   e
		jr   nz, 9$
		ld   l, #0x00          ; no tail
		ld   de, #0x1000       ; 65536 bytes = 4096 blocks of 16
		jr   10$
	9$:
		; SHORT-FILL GUARD. Unrolling costs ~94 T of setup (the count>>4 below). That is a
		; bargain for a screen fill and a disaster for an 8-byte one -- and 8-byte fills are
		; common: VDP_FillLayout_GM2 does one per tile row. Measured: without this guard,
		; FillLayout_GM2 went from 115,659 to 130,767. Take the plain loop when the count
		; cannot amortise the setup. (count != 0 here, so E < 16 means 1..15.)
		ld   a, d
		or   a
		jr   nz, 7$            ; >= 256 bytes: definitely worth unrolling
		ld   a, e
		cp   #16
		jp   c, 8$             ; 1..15 bytes: plain loop, no setup at all.
		                       ; JP, not JR: this jumps over the unrolled block, which on
		                       ; MSX1 (extra NOPs for the 29 T gap) exceeds JR's 127-byte reach.
	7$:
		ld   a, e
		and  #0x0F
		ld   l, a              ; L = tail bytes (count & 15)
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e                 ; DE = number of 16-byte blocks
		ld   a, d
		or   e
		jp   z, 4$             ; fewer than 16 bytes (JP: over the unrolled block)
	10$:
		ld   b, e
		dec  de
		inc  d
		ld   a, c              ; A = value; OUT/NOP/DJNZ never touch it
	1$:
		.rept 16
		out  (0x98), a
		nop
		nop
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		nop
		nop
		nop
#endif
		.endm
		djnz 1$
		dec  d
		jr   nz, 1$
	4$:
		ld   a, l
		or   a
		jr   z, 3$             ; no tail
	8$:
		ld   b, a
		ld   a, c
	5$:
		out  (0x98), a         ; <=15 bytes: OUT+DJNZ (25 T) is fine
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		nop
		nop
#endif
		djnz 5$
	3$:
		pop  hl               ; return address
		inc  sp               ; discard stack arg 'count' (callee-clean, --sdcccall 1)
		inc  sp
		jp   (hl)
	__endasm;
}

// Fast fill: same observable result as VDP_FillVRAM_16K.
void VDP_FastFillVRAM_16K(u8 value, u16 dest, u16 count) __naked
{
	(void)value; (void)dest; (void)count;
	__asm
		; FAST fill = BLANKED-ONLY. Creative win vs MSXgl: drive the block loop with DJNZ (never touches A),
		; so the fill value stays in A across every block -> pure 12cc OUTs, no per-block reload.
		; 16 OUTs/block + amortized djnz/outer ~= 12.8 cc/byte (beats MSXgl's ~14.5).
		ld   c, a              ; C = value
		ld   a, e
		out  (0x99), a         ; addr low
		ld   a, d
		and  #0x3F
		or   #0x40
		out  (0x99), a         ; addr high (write)
		ld   hl, #2            ; ret(2) -> count
		add  hl, sp
		ld   e, (hl)
		inc  hl
		ld   d, (hl)           ; DE = count
		; The tail is computed ONCE and stashed in L. The previous version shuffled the value
		; between A, B and C about ten times before every fill just to mask off the low
		; nibble -- pure setup cost, paid even when count is a multiple of 16 (which it
		; usually is). That preamble, not the inner loop, is why this measured SLOWER than
		; MSXgl despite a faster loop.
		ld   a, e
		and  #0x0F
		ld   l, a              ; L = tail bytes (count & 15)
		; --- blocks of 16: DE = count >> 4 ---
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e                 ; DE = count >> 4 (number of 16-byte blocks)
		ld   a, d
		or   e
		jr   z, 6$             ; no full blocks -- straight to the tail
		ld   a, c              ; A = value (stays across the whole block loop)
		ld   b, e              ; B = blocks LSB  (16-bit loop via B inner + D outer)
		dec  de
		inc  d                 ; D = blocks MSB adjusted
	1$:
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a
		out  (0x98), a         ; 16 OUTs, A never touched
		djnz 1$                ; inner: B blocks (12cc out + 0.8cc amortized djnz)
		dec  d
		jp   nz, 1$            ; outer 256-block chunks
	6$:
		; --- tail: count & 15 ---
		ld   a, l
		or   a
		jr   z, 5$
		ld   b, a
		ld   a, c              ; A = value
	3$:
		out  (0x98), a
		djnz 3$
	5$:
		pop  hl                ; ret addr
		pop  bc                ; discard the 2-byte count stack arg (callee-clean)
		jp   (hl)
	__endasm;
}

// VDP_ReadVRAM_16K(src=HL, dest=DE, count@stack). count 0 => 65536.
void VDP_ReadVRAM_16K(u16 src, u8* dest, u16 count) __naked
{
	(void)src; (void)dest; (void)count;
	__asm
		ld   a, l
		out  (0x99), a         ; addr low
		ld   a, h
		and  #0x3F             ; read (bit6 = 0)
		out  (0x99), a
		ex   de, hl            ; HL = dest (INIR target)
		push hl                ; save dest
		ld   hl, #4            ; saved_dest(2)+ret(2) -> count
		add  hl, sp
		ld   e, (hl)           ; E = count low (remainder)
		inc  hl
		ld   d, (hl)           ; D = count high (256-blocks)
		pop  hl                ; HL = dest
		ld   c, #0x98          ; C = data port for INIR
		call _VDP_BulkIn
		pop  hl                ; ret addr
		pop  bc                ; discard the 2-byte count stack arg (callee-clean)
		jp   (hl)
	__endasm;
}

// VDP_Poke_16K(val=A, dest=DE). Preserve C,HL,IY. (A,B,DE free)
void VDP_Poke_16K(u8 val, u16 dest) __PRESERVES(c, h, l, iyl, iyh) __naked
{
	(void)val; (void)dest;
	__asm
		ld   b, a              ; B = val
		ld   a, e
		di                     ; protect the 2-write 0x99 flip-flop from an ISR that reads
		out  (0x99), a         ; status (which resets it); matches MSXgl's ISR-safe default
		ld   a, d
		and  #0x3F
		or   #0x40
		out  (0x99), a
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		inc  de               ; ~7 T settle for MSX1 G1/G2/MC before the data write
#endif
		ld   a, b
		ei                     ; data write (0x98) does not touch the flip-flop
		out  (0x98), a
		ret
	__endasm;
}

// VDP_Peek_16K(src=HL). Preserve BC,DE,IY. (A,HL free) -> return A
u8 VDP_Peek_16K(u16 src) __PRESERVES(b, c, d, e, iyl, iyh) __naked
{
	(void)src;
	__asm
		ld   a, l
		di                     ; protect the 2-write 0x99 flip-flop from an ISR status read
		out  (0x99), a         ; (resets it); matches MSXgl's ISR-safe default
		ld   a, h
		and  #0x3F
		out  (0x99), a
		; VDP read-settle wait so the data byte is valid before it is read. Only slow
		; modes need it; MSX2 bitmap (the default, incl. undefined MSX_VERSION which the
		; standalone build treats as MSX2) reads immediately, as in MSXgl.
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		add  hl, hl            ; ~12 T settle for MSX1 G1/G2/MC + TEXT1 (HL is dead here)
#elif (VDP_USE_MODE_T2)
		nop                    ; shorter settle for TEXT2 (80-column)
#endif
		ei                     ; data read (0x98) does not touch the flip-flop
		in   a, (0x98)
		ret
	__endasm;
}

//=============================================================================
// 128K (17-bit) VRAM ACCESS
//=============================================================================
// R#14 holds A16..A14; the address port carries A13..A0. The full 17-bit
// address is (destHigh<<16) | destLow, so R#14 = ((destHigh<<2)|(destLow>>14)).
//
// vs the 16K variants these add ONE thing: an R#14 write (2 extra 0x99 writes,
// so 4 consecutive 0x99 writes total). Those 4 writes are the address-flip-flop
// setup and MUST be di/ei-wrapped (an ISR touching 0x99 corrupts the flip-flop).
// The bulk transfer (0x98 via OTIR/INIR/OUT) is the speed win and runs with
// interrupts enabled, exactly as the 16K variants and MSXgl. R#14 is computed
// with plain HL-relative / pop stack reads (no slow IY(disp) accesses like MSXgl)
// -> leaner fixed overhead, which is where we beat MSXgl on the OTIR/INIR paths.

// VDP_WriteVRAM_128K(src=HL, destLow=DE, destHigh@SP+2, count@SP+3). count 0 => 65536.
void VDP_WriteVRAM_128K(const u8* src, u16 destLow, u8 destHigh, u16 count) __naked
{
	(void)src; (void)destLow; (void)destHigh; (void)count;
	__asm
		push hl                ; save src           [src][ret][dH][cnt]
		ld   hl, #4            ; SP+4 = destHigh (orig SP+2 + pushed src)
		add  hl, sp
		ld   a, (hl)           ; A = destHigh
		add  a, a
		add  a, a              ; destHigh << 2
		ld   c, a
		ld   a, d              ; destLow high byte
		rlca
		rlca
		and  #0x03             ; destLow >> 14
		or   c                 ; R#14 = (destHigh<<2)|(destLow>>14)
		di                     ; protect the 4-write 0x99 flip-flop setup from ISR
		out  (0x99), a         ; R#14 value
		ld   a, #0x8E          ; 0x80 | 14
		out  (0x99), a         ; select R#14 (completes R#14 pair)
		ld   a, e
		out  (0x99), a         ; A7..A0
		ld   a, d
		and  #0x3F
		or   #0x40             ; write-enable
		out  (0x99), a         ; A13..A8 + write (completes addr pair)
		ei
		ld   hl, #5            ; SP+5 = count (orig SP+3 + pushed src)
		add  hl, sp
		ld   e, (hl)           ; E = count low  (remainder)
		inc  hl
		ld   d, (hl)           ; D = count high (256-blocks)
		pop  hl                ; HL = src        [ret][dH][cnt]
		ld   c, #0x98          ; data port
		call _VDP_BulkOut
		pop  hl                ; ret addr
		pop  bc                ; discard destHigh + count LSB
		inc  sp               ; discard count MSB (callee-clean, 3 stack bytes)
		jp   (hl)
	__endasm;
}

// VDP_FillVRAM_128K(value=A, destLow=DE, destHigh@SP+2, count@SP+3). count 0 => 65536.
void VDP_FillVRAM_128K(u8 value, u16 destLow, u8 destHigh, u16 count) __naked
{
	(void)value; (void)destLow; (void)destHigh; (void)count;
	__asm
		ld   c, a              ; C = value (saved across setup)
		ld   hl, #2            ; SP+2 = destHigh
		add  hl, sp
		ld   a, (hl)           ; A = destHigh
		add  a, a
		add  a, a              ; destHigh << 2
		ld   b, a
		ld   a, d
		rlca
		rlca
		and  #0x03             ; destLow >> 14
		or   b                 ; R#14 value
		di                     ; protect the 4-write 0x99 flip-flop setup
		out  (0x99), a         ; R#14 value
		ld   a, #0x8E
		out  (0x99), a         ; select R#14
		ld   a, e
		out  (0x99), a         ; A7..A0
		ld   a, d
		and  #0x3F
		or   #0x40
		out  (0x99), a         ; A13..A8 + write
		ei
		ld   hl, #3            ; SP+3 = count
		add  hl, sp
		ld   e, (hl)
		inc  hl
		ld   d, (hl)           ; DE = count
		; NOT a wall. The old loop was OUT(12 T)+DJNZ(13 T) = 25 T/byte, chosen so it also
		; cleared SCREEN 0's 20 T gap -- but 128K addressing only exists in the BITMAP modes
		; (SCREEN 5-8), where the floor is 15 T. So pay 15, not 25:
		;
		;     16 x [OUT (12 T) + NOP (4 T)] + DJNZ (13 T) = 269 T / 16 bytes = 16.8 T/byte
		;
		; The NOP is not waste -- it IS the required inter-access gap, and it replaces a
		; DJNZ that was doing the same padding while pretending to be loop control.
		; ~33% faster. (VDP_FillVRAM_16K keeps a 20 T spacing: it may be used in SCREEN 0.)
		; SIZE DISPATCH. Unrolling costs ~94 T of count>>4 setup below -- a bargain for a
		; screen fill, a disaster for an 8-byte one (VDP_FillLayout_GM2 fills ONE TILE ROW
		; per call). So short fills take a plain loop with no setup at all.
		;
		; ACCEPTED TRADE (deliberate, measured -- do not "fix" this):
		;   bulk fill  +27% vs MSXgl  (114,587 -> 83,079 cycles)
		;   8-byte fill -3.6%         (the ~33 T this dispatch costs)
		; MSXgl pays no dispatch because it has no fast bulk path to dispatch TO. At n=8
		; the setup IS the cost, and every alternative is worse -- all measured:
		;   computed jump into an OUT/NOP chain: ~80 T of address math  -> ~208 T (loses)
		;   jump table:                          ~60 T                  -> ~221 T (loses)
		;   plain loop, as here:                  33 T dispatch         -> ~233 T
		;   MSXgl (no bulk path at all):           0 T                  -> ~200 T
		; Bulk fills dominate real frame budgets; a tile row does not.
		;
		; The dispatch itself is the only cost small fills pay, so it is arranged so the
		; SHORT case falls through with every branch UNTAKEN (7 T each, vs 12 T taken):
		;   D != 0  -> >= 256 bytes          -> unroll
		;   E == 0  -> count == 0 == 65536   -> unroll (0>>4 would write NOTHING)
		;   E >= 16 -> worth amortising      -> unroll
		;   else 1..15 bytes                 -> fall through to the plain loop
		ld   a, d
		or   a
		jr   nz, 7$
		ld   a, e
		or   a
		jr   z, 9$             ; count == 0 -> 65536
		cp   #16
		jr   nc, 7$
		jp   8$                ; 1..15 bytes: plain loop (JP: JR cannot clear the unrolled
		                       ; block on MSX1, where it grows for the 29 T gap)
	9$:
		ld   l, #0x00          ; count == 0 -> 65536 bytes = 4096 blocks of 16, no tail
		ld   de, #0x1000
		jr   10$
	7$:
		ld   a, e
		and  #0x0F
		ld   l, a              ; L = tail bytes (count & 15)
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e
		srl  d
		rr   e                 ; DE = count >> 4 = number of 16-byte blocks
		ld   a, d
		or   e
		jp   z, 4$             ; fewer than 16 bytes: tail only (JP: over the unrolled block)
	10$:
		ld   b, e              ; B = blocks LSB
		dec  de
		inc  d                 ; D = outer block count (same B/D trick as before)
		ld   a, c              ; A = value; OUT/NOP/DJNZ never touch it
	1$:
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		out  (0x98), a
		nop
		djnz 1$
		dec  d
		jr   nz, 1$
	4$:
		ld   a, l
		or   a
		jr   z, 5$             ; no tail
	8$:
		ld   b, a
		ld   a, c              ; A = value
	6$:
		out  (0x98), a         ; <=15 bytes; OUT+DJNZ (25 T) is fine here
		djnz 6$
	5$:
		pop  hl               ; ret addr
		pop  bc               ; discard destHigh + count LSB
		inc  sp               ; discard count MSB (callee-clean, 3 stack bytes)
		jp   (hl)
	__endasm;
}

// VDP_ReadVRAM_128K(srcLow=HL, srcHigh@SP+2, dest@SP+3, count@SP+5). count 0 => 65536.
void VDP_ReadVRAM_128K(u16 srcLow, u8 srcHigh, u8* dest, u16 count) __naked
{
	(void)srcLow; (void)srcHigh; (void)dest; (void)count;
	__asm
		ex   de, hl            ; DE = srcLow (DE was free); HL now scratch for stack reads
		ld   hl, #2            ; SP+2 = srcHigh
		add  hl, sp
		ld   a, (hl)           ; A = srcHigh
		add  a, a
		add  a, a              ; srcHigh << 2
		ld   c, a
		ld   a, d              ; srcLow high byte
		rlca
		rlca
		and  #0x03             ; srcLow >> 14
		or   c                 ; R#14 value
		di                     ; protect the 4-write 0x99 flip-flop setup
		out  (0x99), a         ; R#14 value
		ld   a, #0x8E
		out  (0x99), a         ; select R#14
		ld   a, e              ; srcLow low  -> A7..A0
		out  (0x99), a
		ld   a, d              ; srcLow high
		and  #0x3F             ; read (bit6 = 0)
		out  (0x99), a         ; A13..A8
		ei
		ld   hl, #3            ; SP+3 = dest (single stack walk: dest then count)
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)           ; BC = dest
		inc  hl
		ld   e, (hl)
		inc  hl
		ld   d, (hl)           ; DE = count
		ld   l, c
		ld   h, b              ; HL = dest (INIR target)
		ld   c, #0x98          ; data port for INIR
		call _VDP_BulkIn
		pop  hl                ; ret addr
		pop  bc                ; discard srcHigh + dest LSB
		pop  bc                ; discard dest MSB + count LSB
		inc  sp               ; discard count MSB (callee-clean, 5 stack bytes)
		jp   (hl)
	__endasm;
}

// VDP_Poke_128K(val=A, destLow=DE, destHigh@SP+2).
void VDP_Poke_128K(u8 val, u16 destLow, u8 destHigh) __naked
{
	(void)val; (void)destLow; (void)destHigh;
	__asm
		ld   c, a              ; C = val
		ld   hl, #2            ; SP+2 = destHigh
		add  hl, sp
		ld   a, (hl)
		add  a, a
		add  a, a              ; destHigh << 2
		ld   b, a
		ld   a, d
		rlca
		rlca
		and  #0x03
		or   b                 ; R#14 value
		di                     ; protect the 4-write 0x99 flip-flop setup
		out  (0x99), a         ; R#14 value
		ld   a, #0x8E
		out  (0x99), a         ; select R#14
		ld   a, e
		out  (0x99), a         ; A7..A0
		ld   a, d
		and  #0x3F
		or   #0x40
		out  (0x99), a         ; A13..A8 + write
		ei
		ld   a, c              ; val
		out  (0x98), a
		pop  hl                ; ret addr
		inc  sp               ; discard destHigh (callee-clean, 1 stack byte)
		jp   (hl)
	__endasm;
}

// VDP_Peek_128K(srcLow=HL, srcHigh@SP+2) -> return A.
u8 VDP_Peek_128K(u16 srcLow, u8 srcHigh) __naked
{
	(void)srcLow; (void)srcHigh;
	__asm
		pop  iy                ; IY = return address
		dec  sp
		pop  de                ; D = srcHigh (E = garbage); SP now callee-clean
		ld   a, d
		add  a, a
		add  a, a              ; srcHigh << 2
		ld   c, a
		ld   a, h
		rlca
		rlca
		and  #0x03             ; srcLow >> 14
		or   c                 ; R#14 value
		di                     ; protect the 4-write 0x99 flip-flop setup
		out  (0x99), a         ; R#14 value
		ld   a, #0x8E
		out  (0x99), a         ; select R#14
		ld   a, l
		out  (0x99), a         ; A7..A0
		ld   a, h
		and  #0x3F             ; read (bit6 = 0)
		out  (0x99), a         ; A13..A8
		ei
		in   a, (0x98)         ; A = VRAM byte (return value)
		jp   (iy)
	__endasm;
}

// Clear the whole 128K VRAM. Two 64K fills (fewer R#14 setups than MSXgl's 8x16K).
void VDP_ClearVRAM()
{
	VDP_FillVRAM_128K(0, 0x0000, 0, 0);   // low 64K (count 0 => 65536)
	VDP_FillVRAM_128K(0, 0x0000, 1, 0);   // high 64K
}

//=============================================================================
// SCREEN MODE
//=============================================================================

// Distribute the 5-bit mode flag (M5 M4 M3 M2 M1) into R#0 and R#1.
//   M3,M4,M5 -> R#0 (bits 1..3) = (flag>>1) & 0x0E
//   M1 -> R#1 bit4 (0x10), M2 -> R#1 bit3 (0x08)
// __naked: inline the shadow mask/merge and call VDP_RegWriteBak directly.
// This drops the two VDP_RegWriteBakMask call layers (each with a stack-arg push
// and a runtime g_VDP_REGSAV[idx] index) in favour of constant-address shadow
// loads and register-only merges. VDP_RegWriteBak preserves DE, so 'flag' is
// stashed in E across the first call. Same register writes, same order as before.
//   R#0: (REGSAV[0] & 0xF1) | ((flag>>1) & 0x0E)   (M3,M4,M5)
//   R#1: (REGSAV[1] & 0xE7) | (flag.0?0x10) | (flag.1?0x08)   (M2,M1)
void VDP_SetModeFlag(u8 flag) __naked
{
	(void)flag;
	__asm
		ld   e, a                        ; E = flag (survives VDP_RegWriteBak)
		; ---- R#0 ----
		srl  a
		and  #0x0E                       ; (flag>>1) & 0x0E
		ld   b, a
		ld   a, (#_g_VDP_REGSAV + 0)
		and  #0xF1                       ; clear M3,M4,M5
		or   b
		ld   l, a                        ; value
		xor  a                           ; reg #0
		call _VDP_RegWriteBak
		; ---- R#1 ----
		ld   a, (#_g_VDP_REGSAV + 1)
		and  #0xE7                       ; clear M1,M2
		bit  0, e
		jr   z, 1$
		or   #0x10
	1$:
		bit  1, e
		jr   z, 2$
		or   #0x08
	2$:
		ld   l, a                        ; value
		ld   a, #1                       ; reg #1
		jp   _VDP_RegWriteBak            ; tail call
	__endasm;
}

// Per-mode table config for VDP_SetMode. 0xFFFF = "this table unused in this mode" (no real mode's
// NT/CT/PT/SAT/SPT constant is 0xFFFF). Indexed directly by VDP_MODE (contiguous 0..9). Deep-optimized
// from an 11-case switch (498 B) to a const data table + single apply path: smaller AND faster, with
// the exact same per-mode register writes, in the same order.
typedef struct { u16 nt, ct, pt, sat, spt; u8 flag; } VDP_ModeCfg;
static const VDP_ModeCfg g_VDP_ModeCfg[] = {
	/* TEXT1      */ { VDP_T1_ADDR_NT, 0xFFFF,         VDP_T1_ADDR_PT, 0xFFFF,          0xFFFF,          VDP_T1_MODE },
	/* MULTICOLOR */ { VDP_MC_ADDR_NT, 0xFFFF,         VDP_MC_ADDR_PT, VDP_MC_ADDR_SAT, VDP_MC_ADDR_SPT, VDP_MC_MODE },
	/* GRAPHIC1   */ { VDP_G1_ADDR_NT, VDP_G1_ADDR_CT, VDP_G1_ADDR_PT, VDP_G1_ADDR_SAT, VDP_G1_ADDR_SPT, VDP_G1_MODE },
	/* GRAPHIC2   */ { VDP_G2_ADDR_NT, VDP_G2_ADDR_CT, VDP_G2_ADDR_PT, VDP_G2_ADDR_SAT, VDP_G2_ADDR_SPT, VDP_G2_MODE },
	/* TEXT2      */ { VDP_T2_ADDR_NT, VDP_T2_ADDR_CT, VDP_T2_ADDR_PT, 0xFFFF,          0xFFFF,          VDP_T2_MODE },
	/* GRAPHIC3   */ { VDP_G3_ADDR_NT, VDP_G3_ADDR_CT, VDP_G3_ADDR_PT, VDP_G3_ADDR_SAT, VDP_G3_ADDR_SPT, VDP_G3_MODE },
	/* GRAPHIC4   */ { VDP_G4_ADDR_NT, 0xFFFF,         0xFFFF,         VDP_G4_ADDR_SAT, VDP_G4_ADDR_SPT, VDP_G4_MODE },
	/* GRAPHIC5   */ { VDP_G5_ADDR_NT, 0xFFFF,         0xFFFF,         VDP_G5_ADDR_SAT, VDP_G5_ADDR_SPT, VDP_G5_MODE },
	/* GRAPHIC6   */ { VDP_G6_ADDR_NT, 0xFFFF,         0xFFFF,         VDP_G6_ADDR_SAT, VDP_G6_ADDR_SPT, VDP_G6_MODE },
	/* GRAPHIC7   */ { VDP_G7_ADDR_NT, 0xFFFF,         0xFFFF,         VDP_G7_ADDR_SAT, VDP_G7_ADDR_SPT, VDP_G7_MODE },
};

// Highest valid mode index for the target machine. The bitmap modes G4-G7 only
// exist under MSX2+ (vdp.h gates the enumerators on MSX_VERSION); on MSX1 the
// last valid mode is GRAPHIC2 (screen 2).
#if (MSX_VERSION >= MSX_2)
	#define MVC_VDP_MODE_MAX	VDP_MODE_GRAPHIC7
#else
	#define MVC_VDP_MODE_MAX	VDP_MODE_GRAPHIC2
#endif

void VDP_SetMode(u8 mode)
{
	if (!g_VDPInitialized) { VDP_Initialize(); g_VDPInitialized = 1; }
	g_VDP_Data.Mode = mode;   // set early: table setters force bitmap-mode regs (R#2/R#5) for G4-G7

	if (mode > MVC_VDP_MODE_MAX)   // invalid mode -> original default: only the G1 mode flag
	{
		VDP_SetModeFlag(VDP_G1_MODE);
		return;
	}
	const VDP_ModeCfg* c = &g_VDP_ModeCfg[mode];
	VDP_SetLayoutTable(c->nt);
	if (c->ct != 0xFFFF) VDP_SetColorTable(c->ct);
	if (c->pt != 0xFFFF) VDP_SetPatternTable(c->pt);
	if (c->sat != 0xFFFF) VDP_SetSpriteAttributeTable(c->sat);
	if (c->spt != 0xFFFF) VDP_SetSpritePatternTable(c->spt);
	VDP_SetModeFlag(c->flag);
}

//=============================================================================
// DISPLAY SETUP (non-inline)
//=============================================================================

void VDP_SetAdjustOffset(u8 offset)
{
	VDP_RegWrite(18, offset);   // MSXgl writes R#18 without backing up REGSAV[18]
}

// R#26 = byte scroll ((offset>>3)+1), R#27 = fine scroll (offset & 7).
// The +1 on the byte-column count matches MSXgl (V9958 scroll convention).
void VDP_SetHorizontalOffset(u16 offset)
{
	VDP_RegWrite(27, (u8)(offset & 0x07));
	VDP_RegWrite(26, (u8)(((offset >> 3) + 1) & 0x3F));
}

// Palette entry: R#16 = index, then RB byte (0RRR0BBB) + G byte (00000GGG).
void VDP_SetPaletteEntry(u8 index, u16 color)
{
	g_P99 = index;
	g_P99 = 0x80 | 16;
	g_P9A = (u8)color;          // low byte  = R/B
	g_P9A = (u8)(color >> 8);   // high byte = G
}

// VDP_SetPalette(pal=HL, FASTCALL). Load 16 entries (index 0..15).
// Preserve DE,IY. (A,B,HL free)
void VDP_SetPalette(const u8* pal) __FASTCALL __PRESERVES(d, e, iyl, iyh) __naked
{
	(void)pal;
	__asm
		xor  a
		di                      ; protect R#16 setup (2 writes to 0x99) AND the
		                        ; palette auto-increment run from ISR corruption
		out  (0x99), a          ; palette index 0
		ld   a, #(0x80 | 16)
		out  (0x99), a          ; R#16
		ld   c, #0x9A           ; palette data port
		ld   b, #32             ; 16 entries x 2 bytes
		otir                    ; block-out 32 palette bytes from (HL)
		ei
		ret
	__endasm;
}

// Default MSX2 16-colour palette. Each entry: { (R<<4)|B , G }.
static const u8 g_DefaultPalette[32] = {
	0x00,0x00, 0x00,0x00, 0x11,0x06, 0x33,0x07,
	0x17,0x01, 0x27,0x03, 0x51,0x01, 0x27,0x06,
	0x71,0x01, 0x73,0x03, 0x61,0x06, 0x64,0x06,
	0x11,0x04, 0x65,0x02, 0x55,0x05, 0x77,0x07,
};

// Approximate TMS9918 (MSX1) palette expressed in V9938 RGB333.
static const u8 g_MSX1Palette[32] = {
	0x00,0x00, 0x00,0x00, 0x11,0x05, 0x33,0x06,
	0x26,0x02, 0x37,0x03, 0x52,0x02, 0x27,0x06,
	0x62,0x02, 0x63,0x03, 0x52,0x05, 0x63,0x06,
	0x11,0x04, 0x55,0x02, 0x55,0x05, 0x77,0x07,
};

void VDP_SetDefaultPalette()
{
	VDP_SetPalette(g_DefaultPalette);
}

void VDP_SetMSX1Palette()
{
	VDP_SetPalette(g_MSX1Palette);
}

//=============================================================================
// VRAM TABLE ADDRESS (non-inline)
//=============================================================================

void VDP_SetLayoutTable(VADDR addr)
{
	// R#2's encoding is mode-dependent in TWO ways, not one:
	//   - TEXT2 forces the low 2 bits;
	//   - G4/G5 force the low 5 bits;
	//   - G6/G7 (and SCREEN 10..12) force the low 5 bits AND use one shift more,
	//     because their layout table is addressed in 32K rather than 16K units.
	// Only masking the bitmap modes (and never shifting) put the G6/G7 layout base
	// off by a factor of two and left TEXT2 unmasked. Mirrors MSXgl exactly.
	u8 r2 = (u8)(addr >> 10);
#if (MSX_VERSION >= MSX_2)
	{
		u8 mode = g_VDP_Data.Mode;
		if (mode == VDP_MODE_TEXT2)
		{
			r2 |= 0x03;
		}
		else if (mode >= VDP_MODE_GRAPHIC4)   // G4..G7 (and SCREEN 10..12 beyond)
		{
			if (mode >= VDP_MODE_GRAPHIC6)
				r2 >>= 1;                     // 32K addressing units
			r2 |= 0x1F;
		}
	}
#endif
	VDP_SetLayoutTableEx(addr, r2);
}

// In GRAPHIC 2/3 (and TEXT 2) the low bits of R#3 / R#4 are not address bits but
// ADDRESS MASKS: they must be set for the VDP to address all three pattern/colour
// banks. Leaving them clear confines the screen to one bank, so most of the tilemap
// and its colours never appear. Same mode-dependent OR that MSXgl applies.
void VDP_SetColorTable(VADDR addr)
{
	// (u8)(addr >> 6) makes SDCC emit a 6-iteration "srl d / rr e / djnz" loop (~138 T).
	// The same 8 bits (addr bits 6..13) are the HIGH BYTE of (low << 2), and "low <<= 2"
	// is two ADD HL,HL (22 T). Assigning g_ScreenColorLow first also keeps the value in
	// HL and stops SDCC building an IX frame to spill the locals into.
	u16 low = (u16)addr;
	u8 reg;
	g_ScreenColorLow = low;
	low <<= 2;
	reg = (u8)(low >> 8);

	u8 mode = g_VDP_Data.Mode;
	if (mode == VDP_MODE_GRAPHIC2
#if (MSX_VERSION >= MSX_2)
	    || mode == VDP_MODE_GRAPHIC3
#endif
	   )
		reg |= 0x7F;
#if (MSX_VERSION >= MSX_2)
	else if (mode == VDP_MODE_TEXT2)
		reg |= 0x07;
#endif
	VDP_RegWrite(3, reg);

#if (VDP_VRAM_ADDR == VDP_VRAM_ADDR_17)
	VDP_RegWrite(10, (u8)(addr >> 14));
	g_ScreenColorHigh = addr >> 16;
#endif
}

void VDP_SetPatternTable(VADDR addr)
{
	u8 reg = (u8)(addr >> 11);
	switch (g_VDP_Data.Mode)   // measured faster than the equivalent if-chain here
	{
#if (MSX_VERSION >= MSX_2)
	case VDP_MODE_GRAPHIC3:
#endif
	case VDP_MODE_GRAPHIC2:
		reg |= 0x03;
		break;
	default:
		break;
	}
	VDP_SetPatternTableEx(addr, reg);
}

void VDP_SetSpriteAttributeTable(VADDR addr)
{
	u8 r5 = (u8)((addr >> 7) & 0xFF);
	// The low A9..A7 bits of R#5 are forced to 1 in every SPRITE-MODE-2 screen, which
	// is GRAPHIC3 onwards -- not just the bitmap modes G4-G7. Missing GRAPHIC3 here put
	// the sprite attribute table 0x180 bytes off (matches MSXgl's mode list).
#if (MSX_VERSION >= MSX_2)
	if (g_VDP_Data.Mode >= VDP_MODE_GRAPHIC3)
		r5 |= 0x07;
#endif
	VDP_SetSpriteAttributeTableEx(addr, r5, (u8)((addr >> 15) & 0x03));
}

// VADDR (u32) arrives as DE=addr[0:16], L=addr[16:24], H=addr[24:32].
// R#6 = addr>>11 = (addr>>8)>>3. For any valid table (multiple of 0x800, <128K)
// addr>>11 <= 0x3F, so the &0x3F HEAD applied is a no-op here -> identical result.
void VDP_SetSpritePatternTable(VADDR addr) __naked
{
	(void)addr;
	__asm
		ld   (_g_SpritePatternLow), de
		ld   a, l                  ; addr[16:24] = A16..A23
		ld   (_g_SpritePatternHigh), a
		; build EHL = addr[8:32] then >>3 -> L = addr>>11
		ld   e, h                  ; E = addr[24:32]
		ld   h, l                  ; H = addr[16:24]
		ld   l, d                  ; L = addr[8:16]
		ld   b, #3
	1$:
		srl  e
		rr   h
		rr   l
		djnz 1$                    ; EHL >>= 3  => L = (addr>>11)&0xFF
		ld   a, #6
		jp   _VDP_RegWrite         ; RegWrite(reg=A=6, value=L)
	__endasm;
}

// Set the current VRAM page for the active mode by moving the layout base.
// NOTE (documented wall vs MSXgl): MSXgl's VDP_SetPage is a 14-byte R#2 poke
// (reg = (page<<5)|0x1F; VDP_RegWriteBak(2,reg)) that only fits G4/G5 32K pages
// and does NOT update the g_ScreenLayout* tracking globals. msxmvl's contract is
// richer: it moves the full 17-bit layout base (32K pages for G4/G5, 64K pages
// for G6/G7) through VDP_SetLayoutTable, which also refreshes g_ScreenLayoutLow/
// High. Matching MSXgl's size would change observable behaviour, so this stays
// larger by design.
// Select the displayed bitmap page by writing R#2 only, exactly as MSXgl: page in the
// high bits, standard layout-base mask in the low bits. The previous version recomputed
// a full VADDR by mode and routed through VDP_SetLayoutTable -- 3x slower/4x larger AND
// observably different (it also rewrote g_ScreenLayout, which MSXgl's VDP_SetPage does
// not touch, so subsequent draws retargeted).
void VDP_SetPage(u8 page)
{
	u8 reg = (u8)(page << 5);
	reg |= 0x1F;
	VDP_RegWriteBak(2, reg);
}

//=============================================================================
// SPRITE
//=============================================================================

// ---------------------------------------------------------------------------
// Internal sprite VRAM helpers (shared by the naked setters below; not part of
// the public API). Mirror MSXgl's structure (wrappers share a VRAM helper) but
// with a leaner ABI: no stack arg for the high byte, no IY frame.
// ---------------------------------------------------------------------------

// SprSetWriteAddr(DE=destLow, A=destHigh): program the 17-bit VRAM *write*
// address (R#14 + address port). di/ei wraps the 4 consecutive 0x99 writes so
// an ISR cannot corrupt the address flip-flop (matches MSXgl ISR_SAFE_DEFAULT).
// Preserves B, DE, HL; clobbers A, C, F.
void VDP_SprSetWriteAddr(u16 destLow, u8 destHigh) __naked
{
	(void)destLow; (void)destHigh;
	__asm
		add  a, a
		add  a, a              ; A = destHigh << 2
		ld   c, a
		ld   a, d
		rlca
		rlca
		and  #0x03             ; destLow >> 14
		add  a, c              ; R#14 value = (high<<2)|(low>>14)
		di
		out  (0x99), a
		ld   a, #0x8E          ; 0x80|14 -> select R#14
		out  (0x99), a
		ld   a, e
		out  (0x99), a         ; address A7..A0
		ld   a, d
		and  #0x3F
		or   #0x40             ; write-enable
		out  (0x99), a         ; address A13..A8
		ei
		ret
	__endasm;
}

// SprPokeAttr(A=value, HL=destLow): single-byte write to the sprite ATTRIBUTE
// table (uses g_SpriteAttributeHigh). Tail-called by the single-byte setters.
void VDP_SprPokeAttr(u8 value, u16 destLow) __naked
{
	(void)value; (void)destLow;
	__asm
		ld   b, a              ; B = value (SprSetWriteAddr preserves B)
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteAttributeHigh)
		call _VDP_SprSetWriteAddr
		ld   a, b
		out  (0x98), a         ; single VRAM write
		ret
	__endasm;
}

void VDP_LoadSpritePattern(const u8* addr, u8 index, u8 count) __naked
{
	(void)addr; (void)index; (void)count;
	__asm
		push hl                ; save src (SP: src, ret, index, count)
		ld   hl, #4
		add  hl, sp            ; -> index
		ld   a, (hl)
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		add  hl, hl            ; HL = index*8
		ld   de, (_g_SpritePatternLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpritePatternHigh)
		call _VDP_SprSetWriteAddr
		ld   hl, #5
		add  hl, sp            ; -> count
		ld   a, (hl)           ; A = count
		pop  hl                ; HL = src ; SP -> ret
		or   a
		jr   z, 2$             ; count 0 => nothing (HEAD's *8=0 path is pathological)
		ld   c, #0x98
	1$:
		ld   b, #8
		otir                   ; write 8 bytes/sprite (23 T/byte, display-safe)
		dec  a
		jr   nz, 1$
	2$:
		pop  hl                ; ret addr
		inc  sp                ; discard index, count (callee-clean)
		inc  sp
		jp   (hl)
	__endasm;
}

void VDP_SetSpritePositionY(u8 index, u8 y) __naked
{
	(void)index; (void)y;
	__asm
		ld   c, l              ; C = y (value)
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl            ; HL = index*4
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de            ; HL = destLow (+0)
		ld   a, c
		jp   _VDP_SprPokeAttr
	__endasm;
}

void VDP_SetSpritePositionX(u8 index, u8 x) __naked
{
	(void)index; (void)x;
	__asm
		ld   c, l              ; C = x
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		inc  hl                ; +1
		ld   a, c
		jp   _VDP_SprPokeAttr
	__endasm;
}

void VDP_SetSpritePosition(u8 index, u8 x, u8 y) __naked
{
	(void)index; (void)x; (void)y;
	__asm
		ld   b, l              ; B = x (SprSetWriteAddr preserves B)
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteAttributeHigh)
		call _VDP_SprSetWriteAddr
		ld   hl, #2
		add  hl, sp            ; -> y
		ld   a, (hl)
		out  (0x98), a         ; Y
		ld   a, b
		out  (0x98), a         ; X  (12+4 T spacing)
		pop  hl                ; ret
		inc  sp                ; discard y
		jp   (hl)
	__endasm;
}

void VDP_SetSpritePattern(u8 index, u8 shape) __naked
{
	(void)index; (void)shape;
	__asm
		ld   c, l              ; C = shape
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		inc  hl
		inc  hl                ; +2
		ld   a, c
		jp   _VDP_SprPokeAttr
	__endasm;
}

void VDP_SetSpriteColorSM1(u8 index, u8 color) __naked
{
	(void)index; (void)color;
	__asm
		ld   c, l              ; C = color
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		inc  hl
		inc  hl
		inc  hl                ; +3
		ld   a, c
		jp   _VDP_SprPokeAttr
	__endasm;
}

void VDP_SetSpriteSM1(u8 index, u8 x, u8 y, u8 shape, u8 color) __naked
{
	(void)index; (void)x; (void)y; (void)shape; (void)color;
	__asm
		ld   b, l              ; B = x
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteAttributeHigh)
		call _VDP_SprSetWriteAddr
		ld   hl, #2
		add  hl, sp            ; -> y
		ld   a, (hl)
		out  (0x98), a         ; Y
		ld   a, b
		out  (0x98), a         ; X
		inc  hl                ; -> shape
		ld   a, (hl)
		out  (0x98), a         ; shape
		inc  hl                ; -> color
		ld   a, (hl)
		out  (0x98), a         ; color
		pop  hl                ; ret
		inc  sp                ; discard y, shape, color
		inc  sp
		inc  sp
		jp   (hl)
	__endasm;
}

// Sprite Mode 2 attribute is 3 bytes (Y,X,pattern); colour lives in the colour
// table.
void VDP_SetSprite(u8 index, u8 x, u8 y, u8 shape) __naked
{
	(void)index; (void)x; (void)y; (void)shape;
	__asm
		ld   b, l              ; B = x
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteAttributeHigh)
		call _VDP_SprSetWriteAddr
		ld   hl, #2
		add  hl, sp            ; -> y
		ld   a, (hl)
		out  (0x98), a         ; Y
		ld   a, b
		out  (0x98), a         ; X
		inc  hl                ; -> shape
		ld   a, (hl)
		out  (0x98), a         ; shape
		pop  hl                ; ret
		inc  sp                ; discard y, shape
		inc  sp
		jp   (hl)
	__endasm;
}

// Sprite Mode 2 attribute/colour setters. vdp.h only declares these under MSX2+
// (on MSX1 VDP_SetSpriteData is a different inline taking VDP_Sprite*), so gate
// the whole block to match and avoid a redefinition clash on MSX1 builds.
#if (MSX_VERSION >= MSX_2)
void VDP_SetSpriteData(u8 index, const u8* data) __naked
{
	(void)index; (void)data;
	__asm
		push de                ; save data ptr
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteAttributeHigh)
		call _VDP_SprSetWriteAddr
		pop  hl                ; HL = data ptr
		ld   c, #0x98
		outi
		outi
		outi                   ; 3 bytes, 18 T each (display-safe)
		ret
	__endasm;
}

void VDP_SetSpriteUniColor(u8 index, u8 color) __naked
{
	(void)index; (void)color;
	__asm
		ld   b, l              ; B = color value
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl            ; HL = index*16
		ld   de, (_g_SpriteColorLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteColorHigh)
		call _VDP_SprSetWriteAddr
		ld   a, b              ; value (kept in A across the loop)
		ld   b, #16
	1$:
		out  (0x98), a         ; 12 T out + 13 T djnz = 25 T/byte
		djnz 1$
		ret
	__endasm;
}

void VDP_SetSpriteMultiColor(u8 index, const u8* ram) __naked
{
	(void)index; (void)ram;
	__asm
		push de                ; save ram ptr
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl            ; HL = index*16
		ld   de, (_g_SpriteColorLow)
		add  hl, de
		ex   de, hl            ; DE = destLow
		ld   a, (_g_SpriteColorHigh)
		call _VDP_SprSetWriteAddr
		pop  hl                ; HL = ram ptr
		ld   c, #0x98
		ld   b, #16
		otir
		ret
	__endasm;
}

void VDP_SetSpriteExUniColor(u8 index, u8 x, u8 y, u8 shape, u8 color)
{
	VDP_SetSprite(index, x, y, shape);
	VDP_SetSpriteUniColor(index, color);
}

void VDP_SetSpriteExMultiColor(u8 index, u8 x, u8 y, u8 shape, const u8* ram)
{
	VDP_SetSprite(index, x, y, shape);
	VDP_SetSpriteMultiColor(index, ram);
}
#endif // (MSX_VERSION >= MSX_2)

// Disable this sprite and all lower-priority ones via the magic Y coordinate.
// msxmvl (like HEAD) always uses the SM1 disable value 0xD0.
void VDP_DisableSpritesFrom(u8 index) __naked
{
	(void)index;
	__asm
		ld   l, a
		ld   h, #0
		add  hl, hl
		add  hl, hl
		ld   de, (_g_SpriteAttributeLow)
		add  hl, de            ; HL = destLow (+0)
		ld   a, #0xD0          ; VDP_SPRITE_DISABLE_SM1
		jp   _VDP_SprPokeAttr
	__endasm;
}

//=============================================================================
// BLINK (Text 2)
//=============================================================================

// Bit within the attribute byte for column x: bit 7 = leftmost pixel-column.
// A table lookup matches MSXgl's technique and avoids SDCC emitting a runtime
// variable-shift loop for 0x80>>(x&7).
static const u8 g_BlinkBit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

// Set the blink attribute bit for a single tile at column x, row y.
void VDP_SetBlinkTile(u8 x, u8 y)
{
	u16 addr = g_ScreenColorLow + (u16)y * 10 + (x >> 3);
	u8 v = VDP_Peek(addr, g_ScreenColorHigh);
	v |= g_BlinkBit[x & 7];
	VDP_Poke(v, addr, g_ScreenColorHigh);
}

//=============================================================================
// GRAPHIC MODE 2/3 TILE HELPERS
//=============================================================================

// GRAPHIC2/3 mirror the pattern/colour generators into three 0x800-byte banks.
// MSXgl unrolls the three VDP_WriteVRAM calls; a 3-iteration loop (dst += 0x800)
// is markedly smaller (one call-site marshalling instead of three) at a trivial
// speed cost (2 extra loop iterations' overhead vs thousands of OTIR bytes). src
// is re-sent to every bank (not advanced), matching the original per-bank calls.
void VDP_LoadPattern_GM2(const u8* src, u8 count, u8 offset)
{
	u16 n = count ? (u16)count * 8 : 2048;
	u16 dst = g_ScreenPatternLow + (u16)offset * 8;
	u8 high = g_ScreenPatternHigh;
	u8 i = 3;
	do
	{
		VDP_WriteVRAM(src, dst, high, n);
		dst += 0x800;
	} while (--i);
}

void VDP_LoadColor_GM2(const u8* src, u8 count, u8 offset)
{
	u16 n = count ? (u16)count * 8 : 2048;
	u16 dst = g_ScreenColorLow + (u16)offset * 8;
	u8 high = g_ScreenColorHigh;
	u8 i = 3;
	do
	{
		VDP_WriteVRAM(src, dst, high, n);
		dst += 0x800;
	} while (--i);
}

// Both layout helpers step dst by 32 per row instead of recomputing (dy+row)*32
// (that per-row multiply was the original size cost). SDCC keeps count/globals in
// an IX frame here -- unlike the old recompute form, IY is never spilled, so the
// prior IY-clobber "bogus 64K fill" hazard does not arise (verified by the VRAM
// byte-compare vs the HEAD original).
// Loop state. Held in memory, not registers: the row loop needs HL for OUTI, C for the data
// port and DE for the byte count, which leaves nothing to keep dst/nx/ny in. (Same reason
// crypt.c keeps its scratch in globals.)
static u16 s_Lay_Src;
static u16 s_Lay_Dst;
static u8  s_Lay_Nx;
static u8  s_Lay_Ny;
static u8  s_Lay_Val;   // fill value (VDP_FillLayout_GM2)
static u8  s_Lay_R14;   // last R#14 written; 0xFF = "none yet"

// Written in asm because the C version called VDP_WriteVRAM once per ROW, and a row is
// typically 8..32 bytes -- so the per-call overhead, not the copying, was the cost:
//
//   * the wrapper's prologue re-read dx/dy/nx/ny off the stack every row (~60 T),
//   * and it reprogrammed R#14 every row, though g_ScreenLayoutHigh never changes and dst
//     only crosses a 16K boundary in the rare case where the layout table straddles one.
//
// Both are hoisted: the args are read once, and R#14 is written only when its value actually
// changes (still correct across a 16K crossing -- we compare rather than assume).
void VDP_WriteLayout_GM2(const u8* src, u8 dx, u8 dy, u8 nx, u8 ny) __naked
{
	(void)src; (void)dx; (void)dy; (void)nx; (void)ny;
	__asm
		ld   (_s_Lay_Src), hl  ; HL = src
		ld   hl, #2
		add  hl, sp            ; -> dx  [ret][dx][dy][nx][ny]
		ld   c, (hl)           ; C = dx
		inc  hl
		ld   b, (hl)           ; B = dy
		inc  hl
		ld   a, (hl)
		ld   (_s_Lay_Nx), a    ; nx
		inc  hl
		ld   a, (hl)
		ld   (_s_Lay_Ny), a    ; ny
		or   a
		jr   z, 9$             ; ny == 0: nothing to do

		; dst = g_ScreenLayoutLow + dy*32 + dx
		ld   l, b
		ld   h, #0             ; HL = dy
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl            ; HL = dy * 32
		ld   b, #0             ; BC = dx
		add  hl, bc
		ld   bc, (_g_ScreenLayoutLow)
		add  hl, bc
		ld   (_s_Lay_Dst), hl

		ld   a, #0xFF
		ld   (_s_Lay_R14), a   ; force the first row to program R#14

	1$:
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		; NO R#14 ON A TMS9918. It decodes 3 register bits, so a write to R#14 would land on
		; R#6 -- the sprite pattern base -- and silently move the sprite patterns. MSX1 has
		; 16K of VRAM and needs no high bits at all, so skip the register entirely.
		di
#else
		; --- R#14 = (g_ScreenLayoutHigh << 2) | (dst >> 14), written only when it changes
		ld   a, (_g_ScreenLayoutHigh)
		add  a, a
		add  a, a
		ld   b, a
		ld   a, (_s_Lay_Dst + 1)   ; dst high byte
		rlca
		rlca
		and  #0x03
		or   b                 ; A = R#14 value
		ld   b, a
		ld   a, (_s_Lay_R14)
		cp   b
		ld   a, b
		jr   z, 2$             ; unchanged -- skip the register pair (2 OUTs + the DI window)
		ld   (_s_Lay_R14), a
		di
		out  (0x99), a
		ld   a, #0x8E          ; 0x80 | 14
		out  (0x99), a         ; select R#14
		jr   3$
	2$:
		di
	3$:
#endif
		; --- address pair (inside the same DI window: the 0x99 flip-flop must not be split)
		ld   hl, (_s_Lay_Dst)
		ld   a, l
		out  (0x99), a         ; A7..A0
		ld   a, h
		and  #0x3F
		or   #0x40             ; write-enable
		out  (0x99), a         ; A13..A8
		ei

		; --- copy one row
		ld   hl, (_s_Lay_Src)
		ld   a, (_s_Lay_Nx)
		ld   e, a
		ld   d, #0             ; DE = count (a row is < 256 bytes)
		ld   c, #0x98
		call _VDP_BulkOut      ; OUTI advances HL past the row
		ld   (_s_Lay_Src), hl

		ld   hl, (_s_Lay_Dst)
		ld   bc, #32
		add  hl, bc
		ld   (_s_Lay_Dst), hl  ; next row, one layout line down

		ld   a, (_s_Lay_Ny)
		dec  a
		ld   (_s_Lay_Ny), a
		jr   nz, 1$
	9$:
		pop  hl                ; ret addr
		pop  bc                ; discard dx+dy
		pop  bc                ; discard nx+ny  (callee-clean, 4 stack bytes)
		jp   (hl)
	__endasm;
}

// Twin of VDP_WriteLayout_GM2, and the same fix: the C version called VDP_FillVRAM once per
// ROW, paying that wrapper's prologue -- and a fresh R#14 -- for a run of 4..32 cells.
//
// The inner loop is OUT + DJNZ = 25 T/byte, which already clears SCREEN 0's 20 T floor, so it
// needs no padding on MSX2. It is NOT unrolled: a layout row is short, and the unrolled fill's
// count>>4 setup costs ~94 T that a 4-cell row can never earn back. (That is the same
// size-dispatch reasoning VDP_FillVRAM_16K spells out -- here the short case is the ONLY case.)
//
// nx == 0 fills nothing. The old code passed nx straight to VDP_FillVRAM, where count == 0
// means 65536 -- so a zero-width rect would have blown away all of VRAM. Nobody calls it that
// way, but the write twin already treated 0 as 0 and now they agree.
void VDP_FillLayout_GM2(u8 value, u8 dx, u8 dy, u8 nx, u8 ny) __naked
{
	(void)value; (void)dx; (void)dy; (void)nx; (void)ny;
	__asm
		ld   (_s_Lay_Val), a   ; A = value
		ld   c, l              ; L = dx  -> C (HL is about to be the stack walker)
		ld   hl, #2
		add  hl, sp            ; -> dy   [ret][dy][nx][ny]
		ld   b, (hl)           ; B = dy
		inc  hl
		ld   a, (hl)
		ld   (_s_Lay_Nx), a    ; nx
		or   a
		jr   z, 9$             ; nx == 0: fill nothing (see above)
		inc  hl
		ld   a, (hl)
		ld   (_s_Lay_Ny), a    ; ny
		or   a
		jr   z, 9$             ; ny == 0: nothing to do

		; dst = g_ScreenLayoutLow + dy*32 + dx
		ld   l, b
		ld   h, #0             ; HL = dy
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl
		add  hl, hl            ; HL = dy * 32
		ld   b, #0             ; BC = dx
		add  hl, bc
		ld   bc, (_g_ScreenLayoutLow)
		add  hl, bc
		ld   (_s_Lay_Dst), hl

		ld   a, #0xFF
		ld   (_s_Lay_R14), a   ; force the first row to program R#14

	1$:
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		di                     ; no R#14 on a TMS9918 -- see VDP_WriteLayout_GM2
#else
		ld   a, (_g_ScreenLayoutHigh)
		add  a, a
		add  a, a
		ld   b, a
		ld   a, (_s_Lay_Dst + 1)
		rlca
		rlca
		and  #0x03
		or   b                 ; A = R#14 value
		ld   b, a
		ld   a, (_s_Lay_R14)
		cp   b
		ld   a, b
		jr   z, 2$             ; unchanged -- skip the register pair
		ld   (_s_Lay_R14), a
		di
		out  (0x99), a
		ld   a, #0x8E
		out  (0x99), a         ; select R#14
		jr   3$
	2$:
		di
	3$:
#endif
		ld   hl, (_s_Lay_Dst)
		ld   a, l
		out  (0x99), a         ; A7..A0
		ld   a, h
		and  #0x3F
		or   #0x40             ; write-enable
		out  (0x99), a         ; A13..A8
		ei

		ld   a, (_s_Lay_Nx)
		ld   b, a              ; B = cells in this row (non-zero, guarded above)
		ld   a, (_s_Lay_Val)
	5$:
		out  (0x98), a         ; OUT 12 + DJNZ 13 = 25 T >= SCREEN 0's 20 T floor
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
		nop                    ; TMS9918 wants ~29 T: 12 + 4 + 4 + 13 = 33
		nop
#endif
		djnz 5$

		ld   hl, (_s_Lay_Dst)
		ld   bc, #32
		add  hl, bc
		ld   (_s_Lay_Dst), hl  ; one layout line down

		ld   a, (_s_Lay_Ny)
		dec  a
		ld   (_s_Lay_Ny), a
		jr   nz, 1$
	9$:
		pop  hl                ; ret addr
		inc  sp                ; discard dy
		pop  bc                ; discard nx+ny  (callee-clean, 3 stack bytes)
		jp   (hl)
	__endasm;
}

//=============================================================================
// COMMAND ENGINE
//=============================================================================

// Wait for the running command to complete (S#2 bit0 CE == 0).
// Preserve everything except A,F.
void VDP_CommandWait() __PRESERVES(b, c, d, e, h, l, iyl, iyh) __naked
{
	// R#15 (the status-register pointer) is GLOBAL state. The default MSX BIOS ISR
	// acknowledges the V-blank by reading S#0 -- so if we leave R#15 = 2 selected
	// and then enable interrupts, the ISR reads S#2 instead, never clears the F flag,
	// and the interrupt re-fires forever: an interrupt storm that starves the program.
	// So S#0 must be restored BEFORE each EI. This mirrors MSXgl's VDP_CommandWait
	// under its VDP_USE_RESTORE_S0 default (documented "needed only for default BIOS
	// ISR"), including its trick of using `ld a,#0` rather than `xor a` so the CE flag
	// produced by RRA survives the pointer-restore, and placing EI one instruction
	// early so its deferred acceptance still covers the final OUT.
	__asm
	1$:
		ld   a, #2
		di                      ; select S#2 atomically (2-write pair)
		out  (0x99), a
		ld   a, #(0x80 | 15)
		out  (0x99), a
		in   a, (0x99)          ; read S#2
		rra                     ; CE (bit0) -> carry
		ld   a, #0              ; NOT `xor a`: must not disturb the carry from RRA
		out  (0x99), a
		ld   a, #(0x80 | 15)
		ei                      ; EI's one-instruction delay covers the OUT below
		out  (0x99), a          ; reset R#15 = 0 for the BIOS ISR
		jr   c, 1$              ; CE still set => command executing
		ret
	__endasm;
}

// Send registers R#32..R#46 (15 bytes) from g_VDP_Command via R#17 auto-inc.
// Unrolled OUTI upload (mirrors MSXgl's ASM_REG_WRITE_INC) instead of a C loop.
void VDP_CommandSetupR32() __naked
{
	__asm
		call _VDP_CommandWait     ; preserves BC/DE/HL/IY, waits for CE==0
		ld   hl, #_g_VDP_Command
		ld   c, #0x9B             ; indirect data port
		di                       ; protect R#17 setup + the 15-byte auto-inc burst from ISR corruption
		ld   a, #32
		out  (0x99), a
		ld   a, #(0x80 | 17)      ; R#17 = 32, auto-increment enabled
		out  (0x99), a
		.rept 15
			outi                  ; OUT (C),(HL); INC HL
		.endm
		ei
		ret
	__endasm;
}

// Send registers R#36..R#46 (11 bytes) from g_VDP_Command+4 via R#17 auto-inc.
void VDP_CommandSetupR36() __naked
{
	__asm
		call _VDP_CommandWait
		ld   hl, #(_g_VDP_Command + 4)
		ld   c, #0x9B
		di                       ; protect R#17 setup + the 11-byte auto-inc burst from ISR corruption
		ld   a, #36
		out  (0x99), a
		ld   a, #(0x80 | 17)
		out  (0x99), a
		.rept 11
			outi
		.endm
		ei
		ret
	__endasm;
}

// CPU -> VRAM transfer loop (HMMC/LMMC): feed source bytes to R#44 while the
// command engine requests them (TR = S#2 bit7) until it finishes (CE = bit0).
// This is a self-terminating loop, so it needs no NX*NY byte count and touches
// only A + HL, honouring the __PRESERVES(d, e, iyl, iyh) contract.  addr -> HL.
void VDP_CommandWriteLoop(const u8* addr) __FASTCALL __PRESERVES(d, e, iyl, iyh) __naked
{
	(void)addr;
	__asm
		; CORRECTNESS: DI wraps the whole routine. Four consecutive 0x99 register
		; writes (R#17 + R#15 setup) and the closing R#15 reset are 2+-write
		; sequences that an ISR would corrupt via the port flip-flop; and while the
		; transfer runs the default BIOS ISR would clobber R#15/R#17. Matches the
		; MSXgl safe default (VDP_DI ... VDP_EI over the loop). One DI + one EI
		; covers everything (smaller than MSXgl's separate DI/EI pairs, no gap).
		di
		inc  hl                ; skip byte 0: already sent via CLR (R#44) during setup
		ld   a, #0xac          ; R#17 = 44, auto-increment disabled (44|0x80)
		out  (0x99), a
		ld   a, #0x91          ; commit R#17 (17|0x80)
		out  (0x99), a
		ld   a, #2             ; select status register S#2
		out  (0x99), a
		ld   a, #0x8f          ; R#15 = 2 (15|0x80)
		out  (0x99), a
	1$:
		in   a, (0x99)         ; read S#2
		rrca                   ; CE (bit0) -> carry
		jr   nc, 2$            ; CE == 0 -> command finished
		rlca                   ; restore A
		and  #0x80             ; TR (bit7): protocol-correct wait-for-ready before
		jr   z, 1$             ; each byte -> stays correct even on turbo R / fast
		ld   a, (hl)           ; CPUs (MSXgl omits this and relies on loop timing,
		inc  hl                ; which can drop bytes when the CPU outruns the VDP)
		out  (0x9B), a         ; feed next byte to R#44
		jr   1$
	2$:
		xor  a
		out  (0x99), a
		ld   a, #0x8f
		ei                     ; EI delay covers the final 0x99 write below
		out  (0x99), a         ; reset R#15 = 0
		ret
	__endasm;
}

// VRAM -> CPU transfer loop (LMCM): collect bytes from status register S#7 as
// the command engine produces them (TR = S#2 bit7) until it finishes (CE=bit0).
// R#15 is toggled between S#2 (poll) and S#7 (read data) each transferred byte.
void VDP_CommandReadLoop(u8* addr) __FASTCALL
{
	u8 s2;
	for (;;)
	{
		g_P99 = 2; g_P99 = 0x80 | 15;      // select S#2
		s2 = g_P99;                        // read S#2
		if (!(s2 & 0x01))                  // CE == 0 -> finished
			break;
		if (s2 & 0x80)                     // TR ready
		{
			g_P99 = 7; g_P99 = 0x80 | 15;  // select S#7
			*addr++ = g_P99;               // read data byte
		}
	}
	g_P99 = 0; g_P99 = 0x80 | 15;          // reset R#15 = 0
}

// Trigger a custom VDP command straight from the caller's buffer (R#32..R#46).
// The original copied the 15 bytes into g_VDP_Command and then re-streamed them
// via VDP_CommandSetupR32 -- a redundant memcpy. We stream directly from the
// buffer (HL) via the R#17 auto-increment port, exactly like MSXgl's
// ASM_REG_WRITE_INC_HL, but with ONE di/ei spanning both the 0x99 register-set
// pair and the OUTI burst (smaller and gap-free vs MSXgl's two di/ei pairs).
// data -> HL (--sdcccall 1, single 16-bit arg). VDP_CommandWait preserves HL.
void VDP_CommandCustomR32(const VDP_Command* data) __naked
{
	(void)data;
	__asm
		call _VDP_CommandWait     ; wait CE==0; preserves BC/DE/HL/IY
		ld   a, #32
		di                        ; protect the 0x99 pair + OUTI burst from ISR
		out  (0x99), a
		ld   a, #(0x80 | 17)      ; R#17 = 32, auto-increment enabled
		out  (0x99), a
		ld   c, #0x9B             ; indirect data port
		.rept 14
			outi                  ; OUT (C),(HL); INC HL  -> stream 15 regs total
		.endm
		ei                        ; EI delay covers the final OUTI below
		outi
		ret
	__endasm;
}

// As above, streaming 11 bytes to R#36..R#46 straight from the caller's buffer.
void VDP_CommandCustomR36(const VDP_Command36* data) __naked
{
	(void)data;
	__asm
		call _VDP_CommandWait
		ld   a, #36
		di
		out  (0x99), a
		ld   a, #(0x80 | 17)
		out  (0x99), a
		ld   c, #0x9B
		.rept 10
			outi
		.endm
		ei
		outi
		ret
	__endasm;
}
