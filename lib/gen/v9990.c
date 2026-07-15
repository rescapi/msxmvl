// msxmvl clean-room reimplementation of MSXgl "v9990" module.
// Toolchain: SDCC --sdcccall 1.
//
// Implements every NON-inline (extern) v9990 function. The many small helpers
// are `inline` in the header (identical to MSXgl's interface). Register
// preservation contracts (__PRESERVES) are honoured: those functions are
// written as __naked asm so the exact clobber set is under our control.
//
// SDCC --sdcccall 1 argument passing (verified empirically):
//   arg1: u8 -> A,  u16 -> HL,  u32 -> DEHL (DE = low16, HL = high16)
//   arg2: u8 -> L,  u16 -> DE
//   arg3+: stack.  Returns: u8 -> A, u16 -> HL.
//
// V9990 register/VRAM protocol:
//   OUT (P#4=64h), regnum   ; select register (bit7/6 = WII/RII inhibit; 0 = auto-inc)
//   OUT (P#3=63h), value    ; write to selected register (index auto-increments)
//   IN  A,(P#3)             ; read selected register
//   VRAM write addr -> R#0/1/2, read addr -> R#3/4/5, data via P#0 (60h, auto-inc).
//   Palette pointer -> R#14 (= entry<<2), data via P#1 (61h, auto-inc R,G,B).

#include "v9990.h"

//=============================================================================
// PORTS used by the C-level helpers
//=============================================================================
__sfr __at(0x60) g_V9_VRAMPort;
__sfr __at(0x61) g_V9_PalettePort;

//=============================================================================
// GLOBALS
//=============================================================================
V9_Address g_V9_Address;

//=============================================================================
// Screen-mode config table indexed by V9_SCREEN_MODE: { R#6, R#7, P#7 }.
// R#6 = MODE | CLOCK | WIDH | BPP ; R#7 = scan timing ; P#7 = clock source.
//=============================================================================
static const u8 g_V9_ModeConfig[V9_MODE_MAX][3] =
{
	{ 0x05, 0x00, 0x00 }, // P1 : P1+CLK4+WIDH512+BPP4  , XTAL1
	{ 0x59, 0x00, 0x00 }, // P2 : P2+CLK2+WIDH1024+BPP4 , XTAL1
	{ 0x82, 0x00, 0x01 }, // B0 : BMP+CLK4+WIDH256+BPP8 , MCKIN
	{ 0x82, 0x00, 0x00 }, // B1 : BMP+CLK4+WIDH256+BPP8 , XTAL1
	{ 0x96, 0x00, 0x01 }, // B2 : BMP+CLK2+WIDH512+BPP8 , MCKIN
	{ 0x96, 0x00, 0x00 }, // B3 : BMP+CLK2+WIDH512+BPP8 , XTAL1
	{ 0xA9, 0x00, 0x01 }, // B4 : BMP+CLK1+WIDH1024+BPP4, MCKIN
	{ 0xA9, 0x01, 0x00 }, // B5 : + HSCN               , XTAL1
	{ 0xA9, 0x41, 0x00 }, // B6 : + HSCN + C25(480)     , XTAL1
	{ 0xA9, 0x00, 0x00 }, // B7 :                       , XTAL1
};

//=============================================================================
// Group: Core
//=============================================================================

// V9_SetPort(u8 port /*A*/, u8 value /*L*/) : OUT (port), value
void V9_SetPort(u8 port, u8 value) __naked __PRESERVES(b, d, e, h, iyl, iyh)
{
	port; value;
	__asm
		ld	c, a		; port -> C
		out	(c), l		; OUT (port), value  (drop the redundant ld a,l)
		ret
	__endasm;
}

// V9_GetPort(u8 port /*A*/) -> A : IN A,(port)
u8 V9_GetPort(u8 port) __naked __PRESERVES(b, d, e, h, l, iyl, iyh)
{
	port;
	__asm
		ld	c, a
		in	a, (c)
		ret
	__endasm;
}

// V9_SetRegister(u8 reg /*A*/, u8 val /*L*/)
void V9_SetRegister(u8 reg, u8 val) __naked __PRESERVES(b, c, d, e, h, iyl, iyh)
{
	reg; val;
	__asm
		di			; protect the select+data pair (ISR selecting a V9990 reg would corrupt it)
		out	(0x64), a	; select register (auto-increment enabled)
		ld	a, l
		out	(0x63), a	; write value
		ei			; correct placement: ei AFTER the data write (MSXgl re-enables before it -> race)
		ret
	__endasm;
}

// V9_SetRegister16(u8 reg /*A*/, u16 val /*DE*/) : reg<-E(low), reg+1<-D(high)
void V9_SetRegister16(u8 reg, u16 val) __naked __PRESERVES(b, h, l, iyl, iyh)
{
	reg; val;
	__asm
		di			; protect the select + 2-write auto-inc burst
		out	(0x64), a	; select register, auto-increment ON
		ld	a, e
		out	(0x63), a	; reg   = LSB
		ld	a, d
		out	(0x63), a	; reg+1 = MSB
		ei
		ret			; 11 bytes / 62T vs MSXgl 12 bytes / 69T (MSXgl has dead ld a,l + slower out(c) burst)
	__endasm;
}

// V9_GetRegister(u8 reg /*A*/) -> A
u8 V9_GetRegister(u8 reg) __naked __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	reg;
	__asm
		di			; protect the select+read pair
		out	(0x64), a	; select register
		in	a, (0x63)	; read value
		ei			; ei AFTER the read (does not touch A)
		ret
	__endasm;
}

//=============================================================================
// Group: VRAM access
//=============================================================================

// V9_SetWriteAddress(u32 addr /*DEHL*/) : E=A7-0, D=A15-8, L=A18-16 -> R#0,1,2
void V9_SetWriteAddress(u32 addr) __naked __PRESERVES(b, iyl, iyh)
{
	addr;
	__asm
		xor	a, a
		di			; protect the select + 3-write auto-inc address burst
		out	(0x64), a	; select R#0, auto-increment ON
		ld	c, #0x63
		out	(c), e		; R#0 = A7-0
		out	(c), d		; R#1 = A15-8
		out	(c), l		; R#2 = A18-16
		ei			; self-contained (does not leak DI across the call like MSXgl)
		ret
	__endasm;
}

// V9_SetReadAddress(u32 addr /*DEHL*/) -> R#3,4,5
void V9_SetReadAddress(u32 addr) __naked __PRESERVES(b, iyl, iyh)
{
	addr;
	__asm
		ld	a, #3
		di			; protect the select + 3-write auto-inc address burst
		out	(0x64), a	; select R#3, auto-increment ON
		ld	c, #0x63
		out	(c), e		; R#3 = A7-0
		out	(c), d		; R#4 = A15-8
		out	(c), l		; R#5 = A18-16
		ei			; self-contained
		ret
	__endasm;
}

// Fill 'count' bytes of 'value' from current write address.
// value=A, count=DE. Block djnz loop (V9990 VRAM port is not display-slot-contended,
// so out+djnz streams at full speed). No di/ei: port 0x60 does not touch the reg index.
// Convention (matches MSXgl and V9_FillVRAM16_CurrentAddr): count 0 => 65536.
void V9_FillVRAM_CurrentAddr(u8 value, u16 count) __naked
{
	value;	// A
	count;	// DE
	__asm
		ld	b, e			// low byte -> inner djnz count
		dec	de
		inc	d			// D = number of 256-byte blocks
	v9_fill_loop:
		out	(0x60), a		// fill VRAM (A preserved)
		djnz	v9_fill_loop		// inner 8-bit loop
		dec	d
		jp	nz, v9_fill_loop	// outer 8-bit loop
		ret
	__endasm;
}

// Fill 'count' words of 'value' (LSB then MSB) from current write address.
void V9_FillVRAM16_CurrentAddr(u16 value, u16 count) __naked
{
	value;	// HL
	count;	// DE
	__asm
		ld	c, #V9_P00		// port in C so we can OUT straight from L/H (no ld a reload)
		ld	b, e			// low byte -> inner djnz count
		dec	de
		inc	d			// D = number of 256-word blocks
	v9_fill16_loop:
		out	(c), l			// low byte  (14T vs ld a,l+out = 16T)
		out	(c), h			// high byte
		djnz	v9_fill16_loop		// inner 8-bit loop
		dec	d
		jp	nz, v9_fill16_loop	// outer 8-bit loop
		ret				// no ei: data port 0x60 needs no reg-index protection
	__endasm;
}

// Copy 'count' bytes from RAM 'src' to current write address.
// src=HL, count=DE. OTIR block loop (no display-slot padding needed on the V9990).
// No di/ei: port 0x60 does not touch the reg index. Convention: count 0 => 65536.
void V9_WriteVRAM_CurrentAddr(const u8* src, u16 count) __naked
{
	src;	// HL
	count;	// DE
	__asm
		ld	b, e			// low byte -> inner OTIR count
		dec	de
		inc	d			// D = number of 256-byte blocks
		ld	c, #0x60
	v9_write_loop:
		otir				// write to VRAM (inner 8-bit loop)
		dec	d
		jp	nz, v9_write_loop	// outer 8-bit loop
		ret
	__endasm;
}

// Copy 'count' bytes from current read address to RAM 'dest'.
void V9_ReadVRAM_CurrentAddr(const u8* dest, u16 count) __naked
{
	dest;	// HL
	count;	// DE
	__asm
		ld	b, e			// Number of loops is in DE
		dec	de				// Calculate DB value (destroys B, D and E)
		inc	d
		ld	c, #V9_P00
	v9_read_loop:
		inir				// Read from VRAM (inner 8-bits loop)
		dec	d
		jp	nz, v9_read_loop	// Outer 8-bits loop
		ret				// no ei: data port 0x60 needs no reg-index protection
	__endasm;
}

// V9_Poke_CurrentAddr(u8 val /*A*/)
void V9_Poke_CurrentAddr(u8 val) __naked __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	val;
	__asm
		out	(0x60), a
		ret
	__endasm;
}

// V9_Poke16_CurrentAddr(u16 val /*HL*/)
void V9_Poke16_CurrentAddr(u16 val) __naked __PRESERVES(b, c, d, e, iyl, iyh)
{
	val;
	__asm
		ld	a, l
		out	(0x60), a
		ld	a, h
		out	(0x60), a
		ret
	__endasm;
}

// V9_Peek_CurrentAddr() -> A
u8 V9_Peek_CurrentAddr() __naked __PRESERVES(b, c, d, e, h, l, iyl, iyh)
{
	__asm
		in	a, (0x60)
		ret
	__endasm;
}

// V9_Peek16_CurrentAddr() -> DE (LSB first). sdcccall(1) returns a u16 in DE,
// so the two port bytes must be placed in E (low) and D (high); h,l stay
// untouched (hence preserved).
u16 V9_Peek16_CurrentAddr() __naked __PRESERVES(b, c, h, l, iyl, iyh)
{
	__asm
		in	a, (0x60)
		ld	e, a
		in	a, (0x60)
		ld	d, a
		ret
	__endasm;
}

//=============================================================================
// Group: Setting
//=============================================================================

// Set complete screen mode (R#6 mode/clock/width, R#7 scan timing, P#7 clock),
// then enable the display (R#8) and reset the color mode (R#13).
void V9_SetScreenMode(u8 mode)
{
	const u8* ptr = (const u8*)&g_V9_ModeConfig[mode];
	V9_SetRegister(6, *ptr++);
	V9_SetRegister(7, *ptr++);
	V9_SetPort(V9_P07, *ptr);

	V9_SetRegister(8, V9_R08_DISP_ON | 0x02); // Enable display
	V9_SetRegister(13, 0x00); // Reset color mode
}

// Bitmap color mode configuration (R#6 CLRM/bpp, R#13 PLTM|YAE) per color mode.
static const u8 g_V9_ColorConfig[V9_COLOR_BMP_MAX][2] =
{
	{ V9_R06_BPP_2, V9_R13_PLTM_PAL },					// BP2
	{ V9_R06_BPP_4, V9_R13_PLTM_PAL },					// BP4
	{ V9_R06_BPP_8, V9_R13_PLTM_PAL },					// BP6
	{ V9_R06_BPP_8, V9_R13_PLTM_256 },					// BD8
	{ V9_R06_BPP_8, V9_R13_PLTM_YJK },					// BYJK
	{ V9_R06_BPP_8, (u8)(V9_R13_PLTM_YJK + V9_R13_YAE)},	// BYJKP
	{ V9_R06_BPP_8, V9_R13_PLTM_YUV },					// BYUV
	{ V9_R06_BPP_8, (u8)(V9_R13_PLTM_YUV + V9_R13_YAE)},	// BYUVP
	{ V9_R06_BPP_16, V9_R13_PLTM_PAL },					// BD16
};

// Set bitmap color mode: R#6 CLRM (bpp) + R#13 PLTM/YAE.
void V9_SetColorMode(u8 mode)
{
	const u8* ptr = (const u8*)&g_V9_ColorConfig[mode];
	V9_SetFlag(6, V9_R06_BPP_MASK, *ptr++);
	V9_SetFlag(13, (u8)(V9_R13_PLTM_MASK + V9_R13_YAE), *ptr);
}

//=============================================================================
// Group: Scrolling
//=============================================================================

// Layer A horizontal scroll: R#19 = fine (low 3 bits), R#20 = coarse (x>>3).
// R#19/R#20 are consecutive -> one select + auto-inc burst (one di/ei), beating
// two separate SetRegister calls (2 selects + 2 di/ei). x=HL.
void V9_SetScrollingX(u16 x) __naked
{
	x;	// HL
	__asm
		di
		ld	a, #19
		out	(0x64), a	; select R#19, auto-inc
		ld	a, l
		and	#0x07
		out	(0x63), a	; R#19 = x & 7 (fine)
		srl	h
		rr	l
		srl	h
		rr	l
		srl	h
		rr	l		; HL = x >> 3
		ld	a, l
		out	(0x63), a	; R#20 = (x>>3)&0xFF (coarse)
		ei
		ret
	__endasm;
}

// Layer A vertical scroll: R#17 = low 8 bits, R#18 = high 5 bits (preserve top 3).
void V9_SetScrollingY(u16 y)
{
	V9_SetRegister(17, (u8)(y & 0xFF));
	V9_SetFlag(18, 0x1F, (u8)((y >> 8) & 0x1F));
}

// Layer B horizontal scroll (P1): R#23 = fine (low 3 bits), R#24 = coarse (x>>3).
// R#23/R#24 consecutive -> single auto-inc burst. x=HL.
void V9_SetScrollingBX(u16 x) __naked
{
	x;	// HL
	__asm
		di
		ld	a, #23
		out	(0x64), a	; select R#23, auto-inc
		ld	a, l
		and	#0x07
		out	(0x63), a	; R#23 = x & 7 (fine)
		srl	h
		rr	l
		srl	h
		rr	l
		srl	h
		rr	l		; HL = x >> 3
		ld	a, l
		out	(0x63), a	; R#24 = (x>>3)&0xFF (coarse)
		ei
		ret
	__endasm;
}

// Layer B vertical scroll (P1): R#21 = low 8 bits, R#22 bit0 = high bit (preserve rest).
void V9_SetScrollingBY(u16 y)
{
	V9_SetRegister(21, (u8)(y & 0xFF));
	V9_SetFlag(22, 0x01, (u8)((y >> 8) & 0x01));
}

//=============================================================================
// Group: Cursor
//=============================================================================
// Cursor attribute table lives in the cursor area (V9_BMP_CUR = 0x7FE00),
// 8 bytes per cursor:
//   +0 : Y bits 7-0
//   +1 : [disable(bit4) | Y bits 9-8]
//   +2 : X bits 7-0
//   +3 : [color(bits7-6) | X bits 9-8]

// Set cursor position/color (leaves cursor enabled). Attribute bytes are stored
// at a 2-byte stride: +0 Y7-0, +2 Y9-8, +4 X7-0, +6 [color|X9-8|disable].
void V9_SetCursorAttribute(u8 id, u16 x, u16 y, u8 color)
{
	u32 addr = 0x7FE00;
	if (id)
		addr += 8;
	V9_Poke(addr, y & 0xFF);
	addr += 2;
	V9_Poke(addr, y >> 8);
	addr += 2;
	V9_Poke(addr, x & 0xFF);
	addr += 2;
	V9_Poke(addr, ((x >> 8) & 0x3) + ((color & 0x3) << 6));
}

// Show/hide a cursor by toggling the disable bit in attribute byte +6.
void V9_SetCursorDisplay(u8 id, bool enable)
{
	u32 addr = 0x7FE06;
	if (id)
		addr += 8;
	u8 val = V9_Peek(addr);
	if (enable)
		val &= ~V9_CURSOR_DISABLE;
	else
		val |= V9_CURSOR_DISABLE;
	V9_Poke(addr, val);
}

//=============================================================================
// Group: Palette (RGB_24 mode - the MSXgl default)
//=============================================================================

// Set one palette entry. index=A, color=DE -> 3 bytes: R, G, B (each [x:3|5-bit]).
// index<<2 via add a,a (4T/1B each) instead of MSXgl's sla a (8T/2B each);
// select R#14 with out(0x64),a instead of MSXgl's ld c/ld b/out(c) preamble.
void V9_SetPaletteEntry(u8 index, const u8* color) __naked
{
	index;	// A
	color;	// DE
	__asm
		add	a, a		; index<<1
		add	a, a		; index<<2 = palette pointer (entry*4, sub-index 0)
		ld	b, a		; save pointer value
		ld	a, #14
		di			; protect select R#14 + write, and the 3-byte palette burst
		out	(0x64), a	; select R#14 (palette pointer register)
		ld	a, b
		out	(0x63), a	; R#14 = index<<2
		ld	a, (de)		; red
		out	(0x61), a
		inc	de
		ld	a, (de)		; green
		out	(0x61), a
		inc	de
		ld	a, (de)		; blue
		out	(0x61), a
		ei
		ret			; 24 bytes / 134T vs MSXgl 26 bytes / 143T
	__endasm;
}

//=============================================================================
// Group: Helper
//=============================================================================

// Detect a V9990 by round-tripping register R#15.
bool V9_Detect() __naked
{
	__asm
		ld	a, #V9_REG(15) + V9_RII + V9_WII
		di
		out	(V9_P04), a			// Select R#15
		in	a, (V9_P03)			// Read value
		ld	b, a
		cpl						// Invert value
		out	(V9_P03), a			// Write value
		in	a, (V9_P03)			// Read value again
		xor	b
		jr	z, v9_notfound
		ld	c, #V9_P03
		out	(c), b				// Restore value (Return A=0xFF)
	v9_notfound:					// Return A=0x00
		ei
		ret						// Return value in A
	__endasm;
}

// Clear all 512 KB of VRAM with zero.
//
// Uses the V9990 hardware COMMAND ENGINE (byte-unit BMLL, cmd 0xA0) as an
// overlapping cascade fill instead of a CPU out-loop: seed byte 0 = 0, then
// BMLL-copy SA=0 -> DA=1 for 0x7FFFF bytes. Because BMLL processes bytes
// strictly sequentially, each byte reads the (already-zeroed) previous byte,
// so the zero cascades across all 512 KB while the VDP runs the fill
// AUTONOMOUSLY at hardware bandwidth. The Z80 only polls CE (P#5 bit0).
//
// Measured on openMSX C-BIOS_MSX2 -ext gfx9000 (issue->CE=0):
//   ~786 904 T   vs   MSXgl CPU out-loop ~7 212 394 T   => ~9.17x FASTER.
// Correctness: BMLL addresses VRAM via the Bx bijection (transformBx), and the
// CPU port uses the same map in bitmap modes, so the whole 512 KB physical
// space is covered exactly once. A zero fill is transform-independent (0 maps
// to 0 under any bijection), so the result is byte-identical to MSXgl's linear
// CPU clear in ANY screen mode. Full-coverage verified via gfx9000 physical
// readback at 0x00000/0x1FFFF/0x3FFFF/0x40000/0x5FFFF/0x7FFF8.
//
// BMLL command block (R#32..R#52), uploaded as one P#4 select + P#3 auto-inc
// burst (wrapped in di/ei for ISR reg-index safety; poll needs no di):
//   SA=0 (R#32-35=0)  DA=1 (R#36=1, R#37-39=0)
//   NA=0x7FFFF (R#40=FF R#41=00 R#42=FF R#43=07)   ; (NX&FF)+((NY&7FF)<<8)
//   ARG=0 (R#44)  LOG=IMP 0x0C (R#45)  WM=0xFFFF (R#46-47)  R#48-51=0
//   CMD=0xA0 BMLL (R#52) -> triggers execution
// b,c,d,e,h,l all untouched (only A + ports), honouring __PRESERVES.
void V9_ClearVRAM() __naked __PRESERVES(d, e, h, l, iyl, iyh)
{
	__asm
		di			; protect select+burst (ISR reg-index safety)
		xor	a, a
		out	(0x64), a	; select R#0, auto-inc
		out	(0x63), a	; R#0 = 0
		out	(0x63), a	; R#1 = 0
		out	(0x63), a	; R#2 = 0  (write pointer -> linear 0)
		out	(0x60), a	; seed VRAM byte 0 = 0 (source of the cascade)

		ld	a, #32
		out	(0x64), a	; select R#32, auto-inc (command block)
		xor	a, a
		out	(0x63), a	; R#32 SX  low  = 0  \
		out	(0x63), a	; R#33 SX  high = 0   } SA = 0
		out	(0x63), a	; R#34 SY  low  = 0   /
		out	(0x63), a	; R#35 SY  high = 0  /
		inc	a		; a = 1
		out	(0x63), a	; R#36 DX  low  = 1  -> DA = 1
		dec	a		; a = 0
		out	(0x63), a	; R#37 DX  high = 0
		out	(0x63), a	; R#38 DY  low  = 0
		out	(0x63), a	; R#39 DY  high = 0
		dec	a		; a = 0xFF
		out	(0x63), a	; R#40 NX  low  = 0xFF \
		ld	a, #0		; a = 0                  \  NA = 0x7FFFF
		out	(0x63), a	; R#41 NX  high = 0      /  = (0xFF)+(0x07FF<<8)
		dec	a		; a = 0xFF               /
		out	(0x63), a	; R#42 NY  low  = 0xFF  /
		ld	a, #0x07
		out	(0x63), a	; R#43 NY  high = 0x07
		xor	a, a
		out	(0x63), a	; R#44 ARG = 0
		ld	a, #0x0C
		out	(0x63), a	; R#45 LOG = IMP (copy)
		ld	a, #0xFF
		out	(0x63), a	; R#46 WM low  = 0xFF \  WM = 0xFFFF
		out	(0x63), a	; R#47 WM high = 0xFF /
		xor	a, a
		out	(0x63), a	; R#48 FC low  = 0
		out	(0x63), a	; R#49 FC high = 0
		out	(0x63), a	; R#50 BC low  = 0
		out	(0x63), a	; R#51 BC high = 0
		ld	a, #0xA0
		out	(0x63), a	; R#52 CMD = BMLL -> TRIGGER
		ei			; command runs autonomously; index no longer at risk
	v9_clr_poll:
		in	a, (0x65)	; P#5 status
		rrca			; CE (bit0) -> carry
		jr	c, v9_clr_poll	; wait while command executing
		ret
	__endasm;
}
