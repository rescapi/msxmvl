// msxmvl clean-room reimplementation of MSXgl "v9990" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/v9990.h (interface/contract only) +
// vendor v9990_reg.h constant names + V9990 Application Manual knowledge.
// NO MSXgl implementation body (.c/.asm/.lst) was read.
//
// In MSXgl the small helpers are `inline` (living in this header). msxmvl
// keeps those inline bodies here (they ARE the interface) and implements the
// non-inline (extern) functions as real, linkable symbols in v9990.c so a
// single difftest driver can link against real MSXgl OR msxmvl.
#ifndef MSXMVL_V9990_H
#define MSXMVL_V9990_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif
#ifndef __NAKED
#define __NAKED			__naked
#endif

//=============================================================================
// CONFIG (defaults as in MSXgl config_option.h)
//=============================================================================
#define V9_USE_MODE_P1			TRUE
#define V9_USE_MODE_P2			TRUE
#define V9_USE_MODE_B0			TRUE
#define V9_USE_MODE_B1			TRUE
#define V9_USE_MODE_B2			TRUE
#define V9_USE_MODE_B3			TRUE
#define V9_USE_MODE_B4			TRUE
#define V9_USE_MODE_B5			TRUE
#define V9_USE_MODE_B6			TRUE
#define V9_USE_MODE_B7			TRUE

#define V9_PALETTE_YSGBR_16		0
#define V9_PALETTE_GBR_16		1
#define V9_PALETTE_RGB_24		2
#ifndef V9_PALETTE_MODE
#define V9_PALETTE_MODE			V9_PALETTE_RGB_24
#endif

//=============================================================================
// PORTS (V9990 mapped to 60h~6Fh)
//=============================================================================
#define V9_P00			0x60	// VRAM data
#define V9_P01			0x61	// Palette data
#define V9_P02			0x62	// Command data
#define V9_P03			0x63	// Register data
#define V9_P04			0x64	// Register select
#define V9_P05			0x65	// Status
#define V9_P06			0x66	// Interrupt flag
#define V9_P07			0x67	// System control

// P#4 register-select flags
#define V9_P04_RII		0b01000000
#define V9_P04_WII		0b10000000

// P#5 status flags
#define V9_P05_CE		0b00000001
#define V9_P05_E0		0b00000010
#define V9_P05_MCS		0b00000100
#define V9_P05_BD		0b00010000
#define V9_P05_HR		0b00100000
#define V9_P05_VR		0b01000000
#define V9_P05_TR		0b10000000

// P#6 interrupt flags
#define V9_P06_NONE		0b00000000
#define V9_P06_VI		0b00000001
#define V9_P06_HI		0b00000010
#define V9_P06_CE		0b00000100

//=============================================================================
// REGISTER CONSTANTS (subset used by the API)
//=============================================================================
// R#6 Screen mode 0
#define V9_R06_BPP_2		0b00000000
#define V9_R06_BPP_4		0b00000001
#define V9_R06_BPP_8		0b00000010
#define V9_R06_BPP_16		0b00000011
#define V9_R06_BPP_MASK		0b00000011
#define V9_R06_WIDH_256		0b00000000
#define V9_R06_WIDH_512		0b00000100
#define V9_R06_WIDH_1024	0b00001000
#define V9_R06_WIDH_2048	0b00001100
#define V9_R06_WIDH_MASK	0b00001100
#define V9_R06_CLOCK_4		0b00000000
#define V9_R06_CLOCK_2		0b00010000
#define V9_R06_CLOCK_1		0b00100000
#define V9_R06_MODE_P1		0b00000000
#define V9_R06_MODE_P2		0b01000000
#define V9_R06_MODE_BITMAP	0b10000000
#define V9_R06_MODE_STANDBY	0b11000000

// R#8 control
#define V9_R08_SPD_ON		0
#define V9_R08_SPD_OFF		0b01000000
#define V9_R08_DISP_ON		0b10000000
#define V9_R08_DISP_OFF		0
// R#9 interrupt enables (kept under MSXgl's R08_ prefix naming)
#define V9_R08_IEV_ON		0b00000001
#define V9_R08_IEH_ON		0b00000010
#define V9_R08_IECE_ON		0b00000100
#define V9_R09_EVERYLINE	0x80

// R#13 palette/color
#define V9_R13_PLTM_PAL		0b00000000
#define V9_R13_PLTM_256		0b01000000
#define V9_R13_PLTM_YJK		0b10000000
#define V9_R13_PLTM_YUV		0b11000000
#define V9_R13_PLTM_MASK	0b11000000
#define V9_R13_YAE			0b00100000
#define V9_R13_PLTO_MASK	0b00001111

// R#22 layer B scroll high flags
#define V9_R22_SDB			0b01000000
#define V9_R22_SDA			0b10000000

// Command opcodes (high nibble of R#52)
#define V9_CMD_STOP			(0  << 4)
#define V9_CMD_LMMC			(1  << 4)
#define V9_CMD_LMMV			(2  << 4)
#define V9_CMD_LMCM			(3  << 4)
#define V9_CMD_LMMM			(4  << 4)
#define V9_CMD_CMMC			(5  << 4)
#define V9_CMD_CMMM			(7  << 4)
#define V9_CMD_BMXL			(8  << 4)
#define V9_CMD_BMLX			(9  << 4)
#define V9_CMD_BMLL			(10 << 4)
#define V9_CMD_LINE			(11 << 4)
#define V9_CMD_SEARCH		(12 << 4)
#define V9_CMD_POINT		(13 << 4)
#define V9_CMD_PSET			(14 << 4)
#define V9_CMD_ADVANCE		(15 << 4)

//=============================================================================
// DEFINES (from v9990.h)
//=============================================================================
enum V9_SCREEN_MODE
{
	V9_MODE_P1, V9_MODE_P2,
	V9_MODE_B0, V9_MODE_B1, V9_MODE_B2, V9_MODE_B3,
	V9_MODE_B4, V9_MODE_B5, V9_MODE_B6, V9_MODE_B7,
	V9_MODE_MAX,
};

enum V9_COLOR_MODE
{
	V9_COLOR_BP2, V9_COLOR_BP4, V9_COLOR_BP6,
	V9_COLOR_BD8, V9_COLOR_BYJK, V9_COLOR_BYJKP,
	V9_COLOR_BYUV, V9_COLOR_BYUVP, V9_COLOR_BD16,
	V9_COLOR_BMP_MAX,
	V9_COLOR_PP,
	V9_COLOR_MAX,
};

#define V9_CURSOR_DISABLE			0b00010000

#define V9_RGB(r, g, b)				(u16)(((u16)((g) & 0x1F) << 10) + (((r) & 0x1F) << 5) + ((b) & 0x1F))
#define V9_RGBYs(r, g, b, ys)		(u16)((((ys) & 0x01) << 15) + ((u16)((g) & 0x1F) << 10) + (((r) & 0x1F) << 5) + ((b) & 0x1F))

#define V9_REG(n)					n
#define V9_RII						0b01000000
#define V9_WII						0b10000000

#define V9_R06_COLOR_MASK			V9_R06_BPP_MASK
#define V9_R13_COLOR_MASK			(V9_R13_PLTM_MASK + V9_R13_YAE)

// P1 mode VRAM layout
#define V9_P1_PGT_A					0x00000
#define V9_P1_SGT					0x00000
#define V9_P1_SPAT					0x3FE00
#define V9_P1_PGT_B					0x40000
#define V9_P1_PNT_A					0x7C000
#define V9_P1_PNT_B					0x7E000
// P2 mode VRAM layout
#define V9_P2_PGT					0x00000
#define V9_P2_SGT					0x00000
#define V9_P2_SPAT					0x7BE00
#define V9_P2_PNT					0x7C000
// Bitmap mode VRAM layout
#define V9_BMP_PGT					0x00000
#define V9_BMP_CUR					0x7FE00

// Sprite generator table base address values (R#25)
#define V9_P1_SGT_00000				0x00
#define V9_P1_SGT_08000				0x02
#define V9_P1_SGT_10000				0x04
#define V9_P1_SGT_18000				0x06
#define V9_P1_SGT_20000				0x08
#define V9_P1_SGT_28000				0x0A
#define V9_P1_SGT_30000				0x0C
#define V9_P1_SGT_38000				0x0E
#define V9_P2_SGT_00000				0x00
#define V9_P2_SGT_08000				0x01
#define V9_P2_SGT_10000				0x02
#define V9_P2_SGT_18000				0x03
#define V9_P2_SGT_20000				0x04
#define V9_P2_SGT_28000				0x05
#define V9_P2_SGT_30000				0x06
#define V9_P2_SGT_38000				0x07
#define V9_P2_SGT_40000				0x08
#define V9_P2_SGT_48000				0x09
#define V9_P2_SGT_50000				0x0A
#define V9_P2_SGT_58000				0x0B
#define V9_P2_SGT_60000				0x0C
#define V9_P2_SGT_68000				0x0D
#define V9_P2_SGT_70000				0x0E
#define V9_P2_SGT_78000				0x0F

// V9990 interruption flag
#define V9_INT_NONE					V9_P06_NONE
#define V9_INT_VBLANK				V9_P06_VI
#define V9_INT_HBLANK				V9_P06_HI
#define V9_INT_CMDEND				V9_P06_CE

//=============================================================================
// STRUCTURES
//=============================================================================
typedef struct V9_Sprite
{
	u8		Y;
	u8		Pattern;
	u16		X  : 10;
	u16		_1 : 2;
	u16		D  : 1;
	u16		P  : 1;
	u16		SC : 2;
} V9_Sprite;

typedef struct V9_Address
{
	u8 Lower;
	u8 Center;
	u8 Upper;
} V9_Address;

extern V9_Address g_V9_Address;

//=============================================================================
// Group: Core
//=============================================================================
void V9_SetPort(u8 port, u8 value) __PRESERVES(b, d, e, h, iyl, iyh);
u8 V9_GetPort(u8 port) __PRESERVES(b, d, e, h, l, iyl, iyh);
void V9_SetRegister(u8 reg, u8 val) __PRESERVES(b, c, d, e, h, iyl, iyh);
void V9_SetRegister16(u8 reg, u16 val) __PRESERVES(b, h, l, iyl, iyh);
u8 V9_GetRegister(u8 reg) __PRESERVES(b, c, d, e, h, l, iyl, iyh);
inline void V9_SetFlag(u8 reg, u8 mask, u8 flag) { V9_SetRegister(reg, V9_GetRegister(reg) & (~mask) | flag); }

//=============================================================================
// Group: VRAM access
//=============================================================================
void V9_SetWriteAddress(u32 addr) __PRESERVES(b, iyl, iyh);
void V9_SetReadAddress(u32 addr) __PRESERVES(b, iyl, iyh);
void V9_FillVRAM_CurrentAddr(u8 value, u16 count);
void V9_FillVRAM16_CurrentAddr(u16 value, u16 count);
void V9_WriteVRAM_CurrentAddr(const u8* src, u16 count);
void V9_ReadVRAM_CurrentAddr(const u8* dest, u16 count);
void V9_Poke_CurrentAddr(u8 val) __PRESERVES(b, c, d, e, h, l, iyl, iyh);
void V9_Poke16_CurrentAddr(u16 val) __PRESERVES(b, c, d, e, iyl, iyh);
u8 V9_Peek_CurrentAddr() __PRESERVES(b, c, d, e, h, l, iyl, iyh);
u16 V9_Peek16_CurrentAddr() __PRESERVES(b, c, h, l, iyl, iyh);

inline void V9_FillVRAM(u32 addr, u8 value, u16 count) { V9_SetWriteAddress(addr); V9_FillVRAM_CurrentAddr(value, count); }
inline void V9_FillVRAM16(u32 addr, u16 value, u16 count) { V9_SetWriteAddress(addr); V9_FillVRAM16_CurrentAddr(value, count); }
inline void V9_WriteVRAM(u32 addr, const u8* src, u16 count) { V9_SetWriteAddress(addr); V9_WriteVRAM_CurrentAddr(src, count); }
inline void V9_ReadVRAM(u32 addr, const u8* dest, u16 count) { V9_SetReadAddress(addr); V9_ReadVRAM_CurrentAddr(dest, count); }
inline void V9_Poke(u32 addr, u8 val) { V9_SetWriteAddress(addr); V9_Poke_CurrentAddr(val); }
inline void V9_Poke16(u32 addr, u16 val) { V9_SetWriteAddress(addr); V9_Poke16_CurrentAddr(val); }
inline u8 V9_Peek(u32 addr) { V9_SetReadAddress(addr); return V9_Peek_CurrentAddr(); }
inline u16 V9_Peek16(u32 addr) { V9_SetReadAddress(addr); return V9_Peek16_CurrentAddr(); }

//=============================================================================
// Group: Setting
//=============================================================================
void V9_SetScreenMode(u8 mode);
inline void V9_SetBPP(u8 bpp) { V9_SetFlag(6, V9_R06_BPP_MASK, bpp); }
void V9_SetColorMode(u8 mode);
inline u8 V9_GetBPP() { return V9_GetRegister(6) & (~V9_R06_BPP_MASK); }
inline void V9_SetImageSpaceWidth(u8 width) { V9_SetFlag(6, V9_R06_WIDH_MASK, width); }
inline u8 V9_GetImageSpaceWidth() { return V9_GetRegister(6) & (~V9_R06_WIDH_MASK); }
inline void V9_SetDisplayEnable(bool enable) { V9_SetFlag(8, V9_R08_DISP_ON, enable ? V9_R08_DISP_ON : 0); }
inline void V9_SetBackgroundColor(u8 color) { V9_SetRegister(15, color); }
inline u8 V9_GetBackgroundColor() { return V9_GetRegister(15); }
inline void V9_SetAdjustOffset(u8 offset) { V9_SetRegister(16, offset); }
inline void V9_SetAdjustOffsetXY(i8 x, i8 y) { V9_SetAdjustOffset(((-x) & 0x0F) | (((-y) & 0x0F) << 4)); }
inline void V9_SetLayerPriority(u8 x, u8 y) { V9_SetRegister(27, (y << 2) + x); }

//=============================================================================
// Group: Status
//=============================================================================
inline u8 V9_GetStatus() { return V9_GetPort(V9_P05); }
inline bool V9_IsVBlank() { return V9_GetStatus() & V9_P05_VR; }
inline bool V9_IsHBlank() { return V9_GetStatus() & V9_P05_HR; }
inline bool V9_IsCmdDataReady() { return V9_GetStatus() & V9_P05_TR; }
inline bool V9_IsCmdRunning() { return V9_GetStatus() & V9_P05_CE; }
inline bool V9_IsCmdComplete() { return !V9_IsCmdRunning(); }
inline bool V9_IsSecondField() { return V9_GetStatus() & V9_P05_E0; }

//=============================================================================
// Group: Interrupt
//=============================================================================
inline void V9_SetInterrupt(u8 flags) { V9_SetRegister(9, flags); }
inline void V9_DisableInterrupt() { V9_SetRegister(9, 0); }
inline void V9_SetVBlankInterrupt(bool enable) { V9_SetFlag(9, V9_R08_IEV_ON, enable ? V9_R08_IEV_ON : 0); }
inline void V9_SetHBlankInterrupt(bool enable) { V9_SetFlag(9, V9_R08_IEH_ON, enable ? V9_R08_IEH_ON : 0); }
inline void V9_SetCmdEndInterrupt(bool enable) { V9_SetFlag(9, V9_R08_IECE_ON, enable ? V9_R08_IECE_ON : 0); }
inline void V9_SetInterruptLine(u16 line) { V9_SetRegister16(10, line); }
inline void V9_SetInterruptEveryLine() { V9_SetRegister(11, V9_R09_EVERYLINE); }
inline void V9_SetInterruptX(u8 x) { V9_SetRegister(12, x); }

//=============================================================================
// Group: Scrolling
//=============================================================================
void V9_SetScrollingX(u16 x);
void V9_SetScrollingY(u16 y);
inline void V9_SetScrolling(u16 x, u16 y) { V9_SetScrollingX(x);  V9_SetScrollingY(y); }
void V9_SetScrollingBX(u16 x);
void V9_SetScrollingBY(u16 y);
inline void V9_SetScrollingB(u16 x, u16 y) { V9_SetScrollingBX(x);  V9_SetScrollingBY(y); }

//=============================================================================
// Group: Cursor
//=============================================================================
inline void V9_SetCursorEnable(bool enable) { V9_SetFlag(8, V9_R08_SPD_OFF, enable ? 0 : V9_R08_SPD_OFF); }
void V9_SetCursorAttribute(u8 id, u16 x, u16 y, u8 color);
void V9_SetCursorDisplay(u8 id, bool enable);
inline void V9_SetCursorPattern(u8 id, const u8* data) { V9_WriteVRAM(0x7FF00 + (id * 0x80), data, 128); }
inline void V9_SetCursorPalette(u8 offset) { V9_SetRegister(28, offset); }

//=============================================================================
// Group: Sprite
//=============================================================================
inline void V9_SetSpriteEnable(bool enable) { V9_SetFlag(8, V9_R08_SPD_OFF, enable ? 0 : V9_R08_SPD_OFF); }
inline void V9_SetSpritePatternAddr(u8 addr) { V9_SetRegister(25, addr); }
inline void V9_SetSpritePaletteOffset(u8 offset) { V9_SetRegister(28, offset); }

#define V9_SPAT_X_MASK				0b00000011
#define V9_SPAT_DISABLE_MASK		0b00010000
#define V9_SPAT_PRIORITY_MASK		0b00100000
#define V9_SPAT_PALETTE_MASK		0b11000000
#define V9_SPAT_INFO_ENABLE			0
#define V9_SPAT_INFO_DISABLE		V9_SPAT_DISABLE_MASK
#define V9_SPAT_INFO_PRIO_FRONT		0
#define V9_SPAT_INFO_PRIO_BETWEEN	V9_SPAT_PRIORITY_MASK
#define V9_SPAT_INFO_PALETTE(p)		((p) << 6)

// P1 sprites
inline void V9_SetSpriteP1(u8 id, const V9_Sprite* attr) { V9_WriteVRAM(V9_P1_SPAT + (id * 4), (const u8*)attr, 4); }
inline void V9_SetSpritePatternP1(u8 id, u8 pat) { V9_Poke(V9_P1_SPAT + 1 + (id * 4), pat); }
inline void V9_SetSpritePositionP1(u8 id, u8 x, u8 y) { V9_Poke(V9_P1_SPAT + 0 + (id * 4), y); V9_Poke(V9_P1_SPAT + 2 + (id * 4), x); }
inline void V9_SetSpriteInfoP1(u8 id, u8 info) { V9_Poke(V9_P1_SPAT + 3 + (id * 4), info); }
inline void V9_SetSpriteEnableP1(u8 id, bool enable)
{
	u8 val = V9_Peek(V9_P1_SPAT + 3 + (id * 4));
	if (enable) val &= ~V9_SPAT_DISABLE_MASK; else val |= V9_SPAT_DISABLE_MASK;
	V9_Poke(V9_P1_SPAT + 3 + (id * 4), val);
}
inline void V9_SetSpriteDisableP1(u8 id, bool disable) { V9_SetSpriteEnableP1(id, !disable); }
inline void V9_SetSpritePriorityP1(u8 id, u8 prio)
{
	u8 val = V9_Peek(V9_P1_SPAT + 3 + (id * 4));
	if (prio) val &= ~V9_SPAT_PRIORITY_MASK; else val |= V9_SPAT_PRIORITY_MASK;
	V9_Poke(V9_P1_SPAT + 3 + (id * 4), val);
}
inline void V9_SetSpritePaletteP1(u8 id, u8 pal)
{
	u8 val = V9_Peek(V9_P1_SPAT + 3 + (id * 4));
	val &= ~V9_SPAT_PALETTE_MASK;
	val |= pal << 6;
	V9_Poke(V9_P1_SPAT + 3 + (id * 4), val);
}

// P2 sprites
inline void V9_SetSpriteP2(u8 id, const V9_Sprite* attr) { V9_WriteVRAM(V9_P2_SPAT + 4 * id, (const u8*)attr, 4); }
inline void V9_SetSpritePatternP2(u8 id, u8 pat) { V9_Poke(V9_P2_SPAT + 4 * id + 1, pat); }
inline void V9_SetSpritePositionP2(u8 id, u8 x, u8 y) { V9_Poke(V9_P2_SPAT + (4 * id), y); V9_Poke(V9_P2_SPAT + (4 * id) + 2, x); }
inline void V9_SetSpriteInfoP2(u8 id, u8 info) { V9_Poke(V9_P2_SPAT + 3 + (id * 4), info); }
inline void V9_SetSpriteEnableP2(u8 id, bool enable)
{
	u8 val = V9_Peek(V9_P2_SPAT + 3 + (id * 4));
	if (enable) val &= ~V9_SPAT_DISABLE_MASK; else val |= V9_SPAT_DISABLE_MASK;
	V9_Poke(V9_P2_SPAT + 3 + (id * 4), val);
}
inline void V9_SetSpriteDisableP2(u8 id, bool disable) { V9_SetSpriteEnableP2(id, !disable); }
inline void V9_SetSpritePriorityP2(u8 id, u8 prio)
{
	u8 val = V9_Peek(V9_P2_SPAT + 3 + (id * 4));
	if (prio) val &= ~V9_SPAT_PRIORITY_MASK; else val |= V9_SPAT_PRIORITY_MASK;
	V9_Poke(V9_P2_SPAT + 3 + (id * 4), val);
}
inline void V9_SetSpritePaletteP2(u8 id, u8 pal)
{
	u8 val = V9_Peek(V9_P2_SPAT + 3 + (id * 4));
	val &= ~V9_SPAT_PALETTE_MASK;
	val |= pal << 6;
	V9_Poke(V9_P2_SPAT + 3 + (id * 4), val);
}

//=============================================================================
// Group: Palette
//=============================================================================
#if (V9_PALETTE_MODE == V9_PALETTE_RGB_24)
void V9_SetPaletteEntry(u8 index, const u8* color);
inline void V9_SetPalette(u8 first, u8 num, const u8* table) { for (u8 i = 0; i < num; ++i) { V9_SetPaletteEntry(first++, table); table += 3; } }
inline void V9_SetPaletteAll(const u8* table) { V9_SetPalette(0, 64, table); }
#else
void V9_SetPaletteEntry(u8 index, u16 color) __PRESERVES(h, l, iyl, iyh);
inline void V9_SetPalette(u8 first, u8 num, const u16* table) { for (u8 i = 0; i < num; ++i) V9_SetPaletteEntry(first++, *(table++)); }
inline void V9_SetPaletteAll(const u16* table) { V9_SetPalette(0, 64, table); }
#endif

inline void V9_SelectPaletteP1(u8 a, u8 b) { V9_SetRegister(13, ((b & 0x3) << 2) + (a & 0x3)); }
inline void V9_SelectPaletteP2(u8 a) { V9_SetRegister(13, ((a & 0x3) << 2) + (a & 0x3)); }
inline void V9_SelectPaletteBP2(u8 a) { V9_SetRegister(13, a & 0x7); }
inline void V9_SelectPaletteBP4(u8 a) { V9_SetRegister(13, (a & 0x3) << 2); }

//=============================================================================
// Group: Command helper
//=============================================================================
inline void V9_SetCommandSX(u16 sx) { V9_SetRegister16(32, sx); }
inline void V9_SetCommandSY(u16 sy) { V9_SetRegister16(34, sy); }
inline void V9_SetCommandSA(u32 sa) { V9_SetRegister(32, sa & 0xFF); V9_SetRegister(34, (sa >> 8) & 0xFF); V9_SetRegister(35, (sa >> 16) & 0xFF); }
inline void V9_SetCommandDX(u16 dx) { V9_SetRegister16(36, dx); }
inline void V9_SetCommandDY(u16 dy) { V9_SetRegister16(38, dy); }
inline void V9_SetCommandDA(u32 da) { V9_SetRegister(36, da & 0xFF); V9_SetRegister(38, (da >> 8) & 0xFF); V9_SetRegister(39, (da >> 16) & 0xFF); }
inline void V9_SetCommandNX(u16 nx) { V9_SetRegister16(40, nx); }
inline void V9_SetCommandNY(u16 ny) { V9_SetRegister16(42, ny); }
inline void V9_SetCommandMJ(u16 mj) { V9_SetRegister16(40, mj); }
inline void V9_SetCommandMI(u16 mi) { V9_SetRegister16(42, mi); }
inline void V9_SetCommandNA(u32 na) { V9_SetRegister(40, na & 0xFF); V9_SetRegister(42, (na >> 8) & 0xFF); V9_SetRegister(43, (na >> 16) & 0xFF); }
inline void V9_SetCommandArgument(u8 arg) { V9_SetRegister(44, arg); }
inline void V9_SetCommandLogicalOp(u8 lop) { V9_SetRegister(45, lop); }
inline void V9_SetCommandWriteMask(u16 wm) { V9_SetRegister16(46, wm); }
inline void V9_SetCommandFC(u16 fc) { V9_SetRegister16(48, fc); }
inline void V9_SetCommandBC(u16 bc) { V9_SetRegister16(50, bc); }
inline void V9_ExecCommand(u8 op) { V9_SetRegister(52, op); }
inline u16 GetCommandBX() { return V9_GetRegister(53) + (V9_GetRegister(54) << 8); }

//=============================================================================
// Group: Command
//=============================================================================
inline void V9_CommandSTOP() { V9_ExecCommand(V9_CMD_STOP); }
inline void V9_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc) { V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_SetCommandFC(fc); V9_ExecCommand(V9_CMD_LMMV); }
inline void V9_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg) { V9_SetCommandSX(sx); V9_SetCommandSY(sy); V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_ExecCommand(V9_CMD_LMMM); }
inline void V9_CommandCMMC(u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc, u16 bc) { V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_SetCommandFC(fc); V9_SetCommandBC(bc); V9_ExecCommand(V9_CMD_CMMC); }
inline void V9_CommandCMMM(u32 sa, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc, u16 bc) { V9_SetCommandSA(sa); V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_SetCommandFC(fc); V9_SetCommandBC(bc); V9_ExecCommand(V9_CMD_CMMM); }
inline void V9_CommandBMXL(u32 sa, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg) { V9_SetCommandSA(sa); V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_ExecCommand(V9_CMD_BMXL); }
inline void V9_CommandBMLX(u16 sx, u16 sy, u32 da, u16 nx, u16 ny, u8 arg) { V9_SetCommandSX(sx); V9_SetCommandSY(sy); V9_SetCommandDA(da); V9_SetCommandNX(nx); V9_SetCommandNY(ny); V9_SetCommandArgument(arg); V9_ExecCommand(V9_CMD_BMLX); }
inline void V9_CommandBMLL(u32 sa, u32 da, u32 na, u8 arg) { V9_SetCommandSA(sa); V9_SetCommandDA(da); V9_SetCommandNA(na); V9_SetCommandArgument(arg); V9_ExecCommand(V9_CMD_BMLL); }
inline void V9_CommandLINE(u16 dx, u16 dy, u16 mj, u16 mi, u8 arg, u16 fc) { V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandMJ(mj); V9_SetCommandMI(mi); V9_SetCommandArgument(arg); V9_SetCommandFC(fc); V9_ExecCommand(V9_CMD_LINE); }
inline void V9_CommandSEARCH(u16 sx, u16 sy, u8 arg, u16 fc) { V9_SetCommandSX(sx); V9_SetCommandSY(sy); V9_SetCommandArgument(arg); V9_SetCommandFC(fc); V9_ExecCommand(V9_CMD_SEARCH); }
inline void V9_CommandPOINT(u16 sx, u16 sy) { V9_SetCommandSX(sx); V9_SetCommandSY(sy); V9_ExecCommand(V9_CMD_POINT); }
inline void V9_CommandPSET(u16 dx, u16 dy, u16 fc, u8 shift) { V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_SetCommandFC(fc); V9_ExecCommand(V9_CMD_PSET | shift); }
inline void V9_CommandADVANCE(u16 dx, u16 dy, u8 shift) { V9_SetCommandDX(dx); V9_SetCommandDY(dy); V9_ExecCommand(V9_CMD_ADVANCE | shift); }

//=============================================================================
// Group: Helper
//=============================================================================
bool V9_Detect();
void V9_ClearVRAM() __PRESERVES(d, e, h, l, iyl, iyh);
inline void V9_WaitCmdEnd() { while (V9_IsCmdRunning()) {} }

inline u32 V9_TileAddrP1A(u8 x, u8 y) { return V9_P1_PNT_A + (((64 * y) + x) * 2); }
inline u32 V9_TileAddrP1B(u8 x, u8 y) { return V9_P1_PNT_B + (((64 * y) + x) * 2); }
inline u32 V9_TileAddrP2(u8 x, u8 y) { return V9_P2_PNT + (((128 * y) + x) * 2); }

#endif // MSXMVL_V9990_H
