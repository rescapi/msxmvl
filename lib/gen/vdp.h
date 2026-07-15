// msxmvl clean-room reimplementation of MSXgl "vdp" module.
// Drop-in: exposes MSXgl's exact public names + signatures so a single test
// driver can link against real MSXgl OR msxmvl for byte-comparison.
//
// Target configuration (all features enabled so every public function exists):
//   MSX_VERSION >= MSX_2P (V9958) ; VDP_VRAM_ADDR == 17-bit (128K) ;
//   VDP_USE_VRAM16K, VDP_USE_SPRITE, VDP_USE_16X16_SPRITE, VDP_USE_FASTFILL,
//   VDP_USE_COMMAND, VDP_USE_CUSTOM_CMD, VDP_USE_MODE_G2/G3/T2,
//   VDP_USE_PALETTE16, VDP_USE_DEFAULT_PALETTE, VDP_USE_MSX1_PALETTE.
#ifndef MSXMVL_VDP_H
#define MSXMVL_VDP_H

#include "types.h"

//=============================================================================
// TOOLCHAIN ATTRIBUTE MACROS
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

// Set TRUE when the project prints/pokes VRAM in TEXT2 (SCREEN 0, 80 columns). Adds the
// VDP read-settle wait to VDP_Peek_16K in that mode, matching MSXgl's VDP_USE_MODE_T2.
// MSX1 G1/G2/MC and TEXT1 already get the (longer) wait via MSX_VERSION == MSX_1.
#ifndef VDP_USE_MODE_T2
#define VDP_USE_MODE_T2		FALSE
#endif

//=============================================================================
// VDP REGISTER BIT DEFINES (subset used by this module)
//=============================================================================
#define R00_EV		0x01
#define R00_M3		0x02
#define R00_M4		0x04
#define R00_M5		0x08
#define R00_IE1		0x10
#define R00_IE2		0x20
#define R00_DG		0x40

#define R01_MAG		0x01
#define R01_ST		0x02
#define R01_M1		0x08
#define R01_M2		0x10
#define R01_IE0		0x20
#define R01_BL		0x40
#define R01_K16		0x80

#define R08_BW		0x01
#define R08_SPD		0x02
#define R08_VR		0x08
#define R08_CB		0x10
#define R08_TP		0x20
#define R08_PL		0x40
#define R08_MS		0x80

#define R09_DC		0x01
#define R09_NT		0x02
#define R09_EO		0x04
#define R09_IL		0x08
#define R09_S0		0x10
#define R09_S1		0x20
#define R09_LN		0x80

#define R25_SP2		0x01
#define R25_MAK		0x02
#define R25_WTE		0x04
#define R25_YJK		0x08
#define R25_YAE		0x10
#define R25_VDS		0x20
#define R25_CMD		0x40

//.............................................................................
// Command engine command / operation / argument codes
#define VDP_CMD_HMMC	0xF0
#define VDP_CMD_YMMM	0xE0
#define VDP_CMD_HMMM	0xD0
#define VDP_CMD_HMMV	0xC0
#define VDP_CMD_LMMC	0xB0
#define VDP_CMD_LMCM	0xA0
#define VDP_CMD_LMMM	0x90
#define VDP_CMD_LMMV	0x80
#define VDP_CMD_LINE	0x70
#define VDP_CMD_SRCH	0x60
#define VDP_CMD_PSET	0x50
#define VDP_CMD_POINT	0x40
#define VDP_CMD_STOP	0x00

#define VDP_OP_IMP		0x00
#define VDP_OP_AND		0x01
#define VDP_OP_OR		0x02
#define VDP_OP_XOR		0x03
#define VDP_OP_NOT		0x04
#define VDP_OP_TIMP		0x08
#define VDP_OP_TAND		0x09
#define VDP_OP_TOR		0x0A
#define VDP_OP_TXOR		0x0B
#define VDP_OP_TNOT		0x0C

#define VDP_ARG_DIY_DOWN	0
#define VDP_ARG_DIY_UP		8
#define VDP_ARG_DIX_RIGHT	0
#define VDP_ARG_DIX_LEFT	4

//.............................................................................
// 17-bit VRAM address helpers (VADDR == u32)
#define VADDR_LO(a)			((u16)(a))
#define VADDR_HI(a)			((u8)(((u32)(a)) >> 16))
#define VADDR_GET(lo, hi)	(((u32)(hi)) << 16 | ((u32)(u16)(lo)))

//=============================================================================
// ENUMS
//=============================================================================
enum VDP_MODE
{
	VDP_MODE_MSX1 = 0,
	VDP_MODE_TEXT1 = VDP_MODE_MSX1,
	VDP_MODE_MULTICOLOR,
	VDP_MODE_GRAPHIC1,
	VDP_MODE_GRAPHIC2,
	VDP_MODE_MSX2,
	VDP_MODE_TEXT2 = VDP_MODE_MSX2,
	VDP_MODE_GRAPHIC3,
	VDP_MODE_GRAPHIC4,
	VDP_MODE_GRAPHIC5,
	VDP_MODE_GRAPHIC6,
	VDP_MODE_GRAPHIC7,
	VDP_MODE_SCREEN0 = VDP_MODE_TEXT1,
	VDP_MODE_SCREEN0_W40 = VDP_MODE_SCREEN0,
	VDP_MODE_SCREEN1 = VDP_MODE_GRAPHIC1,
	VDP_MODE_SCREEN2 = VDP_MODE_GRAPHIC2,
	VDP_MODE_SCREEN3 = VDP_MODE_MULTICOLOR,
	VDP_MODE_SCREEN0_W80 = VDP_MODE_TEXT2,
	VDP_MODE_SCREEN4 = VDP_MODE_GRAPHIC3,
	VDP_MODE_SCREEN5 = VDP_MODE_GRAPHIC4,
	VDP_MODE_SCREEN6 = VDP_MODE_GRAPHIC5,
	VDP_MODE_SCREEN7 = VDP_MODE_GRAPHIC6,
	VDP_MODE_SCREEN8 = VDP_MODE_GRAPHIC7,
	VDP_MODE_MAX,
};

enum VDP_VERSION
{
	VDP_VERSION_TMS9918A = 0,
	VDP_VERSION_V9938,
	VDP_VERSION_V9958,
	VDP_VERSION_V9968,
	VDP_VERSION_V9978,
};

enum VRAM_SIZE
{
	VRAM_16K = 0,
	VRAM_64K = 1,
	VRAM_128K = 2,
	VRAM_192K = 3,
};

enum VDP_FREQ
{
	VDP_FREQ_50HZ = R09_NT,
	VDP_FREQ_60HZ = 0,
};
enum VDP_LINE
{
	VDP_LINE_192 = 0,
	VDP_LINE_212 = R09_LN,
};
enum VDP_FRAME
{
	VDP_FRAME_STATIC = 0,
	VDP_FRAME_ALTERNANCE = R09_EO,
	VDP_FRAME_INTERLACE = (R09_EO | R09_IL),
};
enum VDP_YJK
{
	VDP_YJK_OFF = 0,
	VDP_YJK_ON = R25_YJK,
	VDP_YJK_YAE = (R25_YJK | R25_YAE),
};

#define VDP_HSCROLL_SINGLE	0
#define VDP_HSCROLL_DOUBLE	R25_SP2

//.............................................................................
// Default screen-mode table VRAM addresses and mode flags
#define VDP_T1_MODE		0x01
#define VDP_T1_ADDR_NT	0x0000
#define VDP_T1_ADDR_PT	0x0800

#define VDP_G1_MODE		0x00
#define VDP_G1_ADDR_NT	0x1800
#define VDP_G1_ADDR_CT	0x2000
#define VDP_G1_ADDR_PT	0x0000
#define VDP_G1_ADDR_SAT	0x1B00
#define VDP_G1_ADDR_SPT	0x3800

#define VDP_G2_MODE		0x04
#define VDP_G2_ADDR_NT	0x1800
#define VDP_G2_ADDR_CT	0x2000
#define VDP_G2_ADDR_PT	0x0000
#define VDP_G2_ADDR_SAT	0x1B00
#define VDP_G2_ADDR_SPT	0x3800

#define VDP_MC_MODE		0x02
#define VDP_MC_ADDR_NT	0x0800
#define VDP_MC_ADDR_PT	0x0000
#define VDP_MC_ADDR_SAT	0x1B00
#define VDP_MC_ADDR_SPT	0x3800

#define VDP_T2_MODE		0x09
#define VDP_T2_ADDR_NT	0x0000
#define VDP_T2_ADDR_CT	0x3E00
#define VDP_T2_ADDR_PT	0x1000

#define VDP_G3_MODE		0x08
#define VDP_G3_ADDR_NT	0x1800
#define VDP_G3_ADDR_CT	0x2000
#define VDP_G3_ADDR_PT	0x0000
#define VDP_G3_ADDR_SAT	0x1E00
#define VDP_G3_ADDR_SPT	0x3800

#define VDP_G4_MODE		0x0C
#define VDP_G4_ADDR_NT	0x0000
#define VDP_G4_ADDR_SAT	0x7600
#define VDP_G4_ADDR_SPT	0x7800

#define VDP_G5_MODE		0x10
#define VDP_G5_ADDR_NT	0x0000
#define VDP_G5_ADDR_SAT	0x7600
#define VDP_G5_ADDR_SPT	0x7800

#define VDP_G6_MODE		0x14
#define VDP_G6_ADDR_NT	0x0000
#define VDP_G6_ADDR_SAT	0xFA00
#define VDP_G6_ADDR_SPT	0xF000

#define VDP_G7_MODE		0x1C
#define VDP_G7_ADDR_NT	0x0000
#define VDP_G7_ADDR_SAT	0xFA00
#define VDP_G7_ADDR_SPT	0xF000

//=============================================================================
// STRUCTURES
//=============================================================================
typedef struct VDP_Data
{
	u8 Mode;
	u8 BPC    : 4;
	u8 Width  : 1;
	u8 Height : 1;
} VDP_Data;

typedef struct VDP_Command
{
	u16 SX; u16 SY; u16 DX; u16 DY; u16 NX; u16 NY;
	u8 CLR; u8 ARG; u8 CMD;
} VDP_Command;
#define VDP_Command32 VDP_Command

typedef struct VDP_Command36
{
	u16 DX; u16 DY; u16 NX; u16 NY;
	u8 CLR; u8 ARG; u8 CMD;
} VDP_Command36;

typedef struct VDP_Sprite
{
	u8 Y; u8 X; u8 Pattern; u8 Color;
} VDP_Sprite;

//=============================================================================
// EXTERNALS
//=============================================================================
extern u8 g_VDP_REGSAV[28];
extern VDP_Data g_VDP_Data;
extern VDP_Command g_VDP_Command;
extern VDP_Sprite g_VDP_Sprite;

extern u16 g_ScreenLayoutLow;
extern u16 g_ScreenColorLow;
extern u16 g_ScreenPatternLow;
extern u16 g_SpriteAttributeLow;
extern u16 g_SpritePatternLow;
extern u16 g_SpriteColorLow;
extern u8 g_ScreenLayoutHigh;
extern u8 g_ScreenColorHigh;
extern u8 g_ScreenPatternHigh;
extern u8 g_SpriteAttributeHigh;
extern u8 g_SpritePatternHigh;
extern u8 g_SpriteColorHigh;

//=============================================================================
// FUNCTIONS (non-inline; bodies in vdp.c)
//=============================================================================

//----- Helper
void VDP_Initialize();
u8 VDP_GetVersion() __NAKED;
void VDP_ClearVRAM();

//----- Screen mode
void VDP_SetMode(u8 mode);
void VDP_SetModeFlag(u8 flag);

//----- Direct access
void VDP_RegWrite(u8 reg, u8 value) __PRESERVES(b, c, d, e, iyl, iyh);
void VDP_RegWriteBak(u8 reg, u8 value) __PRESERVES(d, e, iyl, iyh);
void VDP_RegWriteBakMask(u8 idx, u8 mask, u8 value);
u8 VDP_ReadDefaultStatus() __PRESERVES(b, c, d, e, h, l, iyl, iyh);
u8 VDP_ReadStatus(u8 stat) __PRESERVES(b, c, d, e, h, iyl, iyh);

//----- 16K VRAM access
void VDP_WriteVRAM_16K(const u8* src, u16 dest, u16 count);
void VDP_FillVRAM_16K(u8 value, u16 dest, u16 count);
void VDP_FastFillVRAM_16K(u8 value, u16 dest, u16 count);
void VDP_ReadVRAM_16K(u16 src, u8* dest, u16 count);
void VDP_Poke_16K(u8 val, u16 dest) __PRESERVES(c, h, l, iyl, iyh);
u8 VDP_Peek_16K(u16 src) __PRESERVES(b, c, d, e, iyl, iyh);

//----- 128K (17-bit) VRAM access
void VDP_WriteVRAM_128K(const u8* src, u16 destLow, u8 destHigh, u16 count);
void VDP_FillVRAM_128K(u8 value, u16 destLow, u8 destHigh, u16 count);
void VDP_ReadVRAM_128K(u16 srcLow, u8 srcHigh, u8* dest, u16 count);
void VDP_Poke_128K(u8 val, u16 destLow, u8 destHigh);
u8 VDP_Peek_128K(u16 srcLow, u8 srcHigh);

// The generic names pick the addressing mode that the TARGET ACTUALLY HAS.
//
// These used to be the 128K variants unconditionally, which is wrong on MSX1. 128K addressing
// works by programming R#14 with the high address bits -- but a TMS9918 has no R#14. It
// decodes only 3 register bits, so a write to R#14 lands on R#6, the SPRITE PATTERN BASE.
// Every VDP_Peek / VDP_FillVRAM / VDP_WriteVRAM on an MSX1 silently moved the sprite pattern
// table, and read back wrong data into the bargain.
//
// The high byte is dropped rather than passed, because a TMS9918 only has 16K of VRAM: there
// is no address for it to name. Compile-time, so MSX1 pays nothing for the check -- it simply
// calls the 16K routine, which is also the smaller and faster one.
#if (defined(MSX_VERSION) && (MSX_VERSION == MSX_1))
	#define VDP_WriteVRAM(src, dst, high, cnt)	VDP_WriteVRAM_16K((src), (dst), (cnt))
	#define VDP_FillVRAM(val, dst, high, cnt)	VDP_FillVRAM_16K((val), (dst), (cnt))
	#define VDP_ReadVRAM(src, high, dst, cnt)	VDP_ReadVRAM_16K((src), (dst), (cnt))
	#define VDP_Poke(val, dst, high)			VDP_Poke_16K((val), (dst))
	#define VDP_Peek(src, high)					VDP_Peek_16K((src))
#else
	#define VDP_WriteVRAM	VDP_WriteVRAM_128K
	#define VDP_FillVRAM	VDP_FillVRAM_128K
	#define VDP_ReadVRAM	VDP_ReadVRAM_128K
	#define VDP_Poke		VDP_Poke_128K
	#define VDP_Peek		VDP_Peek_128K
#endif

//----- Display setup (non-inline)
void VDP_SetAdjustOffset(u8 offset);
void VDP_SetPalette(const u8* pal) __FASTCALL __PRESERVES(d, e, iyl, iyh);
void VDP_SetPaletteEntry(u8 index, u16 color);
void VDP_SetDefaultPalette();
void VDP_SetMSX1Palette();
void VDP_SetHorizontalOffset(u16 offset);

//----- VRAM table address (non-inline)
void VDP_SetLayoutTable(VADDR addr);
void VDP_SetColorTable(VADDR addr);
void VDP_SetPatternTable(VADDR addr);
void VDP_SetSpriteAttributeTable(VADDR addr);
void VDP_SetSpritePatternTable(VADDR addr);
void VDP_SetPage(u8 page);

//----- Sprite (non-inline)
void VDP_LoadSpritePattern(const u8* addr, u8 index, u8 count);
void VDP_SetSpriteSM1(u8 index, u8 x, u8 y, u8 shape, u8 color);
void VDP_SetSprite(u8 index, u8 x, u8 y, u8 shape);
void VDP_SetSpriteExMultiColor(u8 index, u8 x, u8 y, u8 shape, const u8* ram);
void VDP_SetSpriteExUniColor(u8 index, u8 x, u8 y, u8 shape, u8 color);
void VDP_SetSpritePosition(u8 index, u8 x, u8 y);
void VDP_SetSpritePositionX(u8 index, u8 x);
void VDP_SetSpritePositionY(u8 index, u8 y);
void VDP_SetSpritePattern(u8 index, u8 shape);
void VDP_SetSpriteColorSM1(u8 index, u8 color);
void VDP_SetSpriteUniColor(u8 index, u8 color);
void VDP_SetSpriteMultiColor(u8 index, const u8* ram);
void VDP_SetSpriteData(u8 index, const u8* data);
void VDP_DisableSpritesFrom(u8 index);

#define VDP_SPRITE_SIZE_8	0
#define VDP_SPRITE_SIZE_16	R01_ST
#define VDP_SPRITE_SCALE_1	0
#define VDP_SPRITE_SCALE_2	R01_MAG
#define VDP_SPRITE_EC		0x80
#define VDP_SPRITE_CC		0x40
#define VDP_SPRITE_IC		0x20
#define VDP_SPRITE_DISABLE_SM1	208
#define VDP_SPRITE_DISABLE_SM2	216
#define VDP_SPRITE_HIDE		213

//----- Blink (T2)
void VDP_SetBlinkTile(u8 x, u8 y);

//----- GM2 / GM3
void VDP_LoadPattern_GM2(const u8* src, u8 count, u8 offset);
void VDP_LoadColor_GM2(const u8* src, u8 count, u8 offset);
void VDP_WriteLayout_GM2(const u8* src, u8 dx, u8 dy, u8 nx, u8 ny);
void VDP_FillLayout_GM2(u8 value, u8 dx, u8 dy, u8 nx, u8 ny);

//----- Command engine
void VDP_CommandWait() __PRESERVES(b, c, d, e, h, l, iyl, iyh);
void VDP_CommandSetupR32();
void VDP_CommandSetupR36();
void VDP_CommandWriteLoop(const u8* addr) __FASTCALL __PRESERVES(d, e, iyl, iyh);
void VDP_CommandReadLoop(u8* addr) __FASTCALL;
void VDP_CommandCustomR32(const VDP_Command* data);
void VDP_CommandCustomR36(const VDP_Command36* data);

//=============================================================================
// INLINE FUNCTIONS (part of the header interface)
//=============================================================================

//----- Screen mode queries
inline u8 VDP_GetMode() { return g_VDP_Data.Mode; }
inline bool VDP_IsBitmapMode(u8 mode) { return mode >= VDP_MODE_GRAPHIC4; }
inline bool VDP_IsPatternMode(u8 mode) { return !VDP_IsBitmapMode(mode); }

//----- Display setup (R#1 / R#7 / R#8 / R#9 / R#18 / R#23 / R#25)
inline void VDP_EnableDisplay(bool enable) { VDP_RegWriteBakMask(1, (u8)~R01_BL, enable ? R01_BL : 0); }
inline void VDP_EnableVBlank(bool enable) { VDP_RegWriteBakMask(1, (u8)~R01_IE0, enable ? R01_IE0 : 0); }
inline void VDP_SetColor(u8 color) { VDP_RegWrite(7, color); }
inline void VDP_SetBackdropColor(u8 color) { VDP_SetColor(color); }
inline void VDP_SetColor2(u8 bg, u8 text) { VDP_RegWrite(7, bg | (text << 4)); }

inline void VDP_EnableSprite(u8 enable) { VDP_RegWriteBakMask(8, (u8)~R08_SPD, !enable ? R08_SPD : 0); }
inline void VDP_DisableSprite() { VDP_EnableSprite(FALSE); }
inline void VDP_EnableTransparency(u8 enable) { VDP_RegWriteBakMask(8, (u8)~R08_TP, !enable ? R08_TP : 0); }
inline void VDP_EnableHBlank(bool enable) { VDP_RegWriteBakMask(0, (u8)~R00_IE1, enable ? R00_IE1 : 0); }
inline void VDP_SetHBlankLine(u8 line) { VDP_RegWrite(19, line); }
inline void VDP_SetVerticalOffset(u8 offset) { VDP_RegWrite(23, offset); }
inline void VDP_SetAdjustOffsetXY(i8 x, i8 y) { VDP_SetAdjustOffset(((-x) & 0x0F) | (((-y) & 0x0F) << 4)); }
inline void VDP_SetGrayScale(bool enable) { VDP_RegWriteBakMask(8, (u8)~R08_BW, enable ? R08_BW : 0); }
inline void VDP_SetFrequency(u8 freq) { VDP_RegWriteBakMask(9, (u8)~R09_NT, freq); }
inline u8 VDP_GetFrequency() { return g_VDP_REGSAV[9] & R09_NT; }
inline void VDP_SetLineCount(u8 lines) { VDP_RegWriteBakMask(9, (u8)~R09_LN, lines); }
inline void VDP_SetPageAlternance(bool enable) { VDP_RegWriteBakMask(9, (u8)~R09_EO, enable ? R09_EO : 0); }
inline void VDP_SetInterlace(bool enable) { VDP_RegWriteBakMask(9, (u8)~R09_IL, enable ? R09_IL : 0); }
inline void VDP_SetFrameRender(u8 mode) { VDP_RegWriteBakMask(9, (u8)~(R09_EO | R09_IL), mode); }
inline void VDP_ResetVRAMAddrMSB() { VDP_RegWrite(14, 0); }

inline void VDP_SetYJK(u8 mode) { VDP_RegWriteBakMask(25, (u8)~VDP_YJK_YAE, mode); }
inline void VDP_ExpendCommand(u8 enable) { VDP_RegWriteBakMask(25, (u8)~R25_CMD, enable ? R25_CMD : 0); }
inline void VDP_EnableMask(u8 enable) { VDP_RegWriteBakMask(25, (u8)~R25_MAK, enable ? R25_MAK : 0); }
inline void VDP_SetHorizontalMode(u8 mode) { VDP_RegWriteBakMask(25, (u8)~R25_SP2, mode); }

//----- VRAM table address (Ex + getters)
inline void VDP_SetLayoutTableEx(VADDR addr, u8 r2) { g_ScreenLayoutLow = (u16)addr; VDP_RegWrite(2, r2); g_ScreenLayoutHigh = (u8)(addr >> 16); }
inline void VDP_SetColorTableEx(VADDR addr, u8 r3, u8 r10) { g_ScreenColorLow = (u16)addr; VDP_RegWrite(3, r3); VDP_RegWrite(10, r10); g_ScreenColorHigh = (u8)(addr >> 16); }
inline void VDP_SetPatternTableEx(VADDR addr, u8 r4) { g_ScreenPatternLow = (u16)addr; VDP_RegWrite(4, r4); g_ScreenPatternHigh = (u8)(addr >> 16); }
inline void VDP_SetSpriteAttributeTableEx(VADDR addr, u8 r5, u8 r11) { g_SpriteAttributeLow = (u16)addr; VDP_RegWrite(5, r5); VDP_RegWrite(11, r11); g_SpriteAttributeHigh = (u8)(addr >> 16); addr -= 0x200; g_SpriteColorLow = (u16)addr; g_SpriteColorHigh = (u8)(addr >> 16); }
inline void VDP_SetSpritePatternTableEx(VADDR addr, u8 r6) { g_SpritePatternLow = (u16)addr; VDP_RegWrite(6, r6); g_SpritePatternHigh = (u8)(addr >> 16); }

inline VADDR VDP_GetLayoutTable() { return VADDR_GET(g_ScreenLayoutLow, g_ScreenLayoutHigh); }
inline VADDR VDP_GetColorTable() { return VADDR_GET(g_ScreenColorLow, g_ScreenColorHigh); }
inline VADDR VDP_GetPatternTable() { return VADDR_GET(g_ScreenPatternLow, g_ScreenPatternHigh); }
inline VADDR VDP_GetSpriteAttributeTable() { return VADDR_GET(g_SpriteAttributeLow, g_SpriteAttributeHigh); }
inline VADDR VDP_GetSpritePatternTable() { return VADDR_GET(g_SpritePatternLow, g_SpritePatternHigh); }
inline VADDR VDP_GetSpriteColorTable() { return VADDR_GET(g_SpriteColorLow, g_SpriteColorHigh); }

//----- Palette
inline void VDP_SetPaletteEntryXY(u8 index, u16 color) { VDP_SetPaletteEntry(index, color); }

//----- Sprite helpers
inline void VDP_SetSpriteFlag(u8 flag) { VDP_RegWriteBakMask(1, (u8)~(R01_ST | R01_MAG), flag); }
inline void VDP_SetSpriteTables(VADDR patAddr, VADDR attAddr) { VDP_SetSpritePatternTable(patAddr); VDP_SetSpriteAttributeTable(attAddr); }
inline void VDP_HideSprite(u8 index) { VDP_SetSpritePositionY(index, VDP_SPRITE_HIDE); }
inline void VDP_HideAllSprites() { u8 i; for (i = 0; i < 32; i++) VDP_SetSpritePositionY(i, VDP_SPRITE_HIDE); }

//----- Blink (T2)
inline void VDP_SetBlinkColor(u8 color) { VDP_RegWrite(12, color); }
inline void VDP_SetBlinkColor2(u8 bg, u8 text) { VDP_RegWrite(12, bg | (text << 4)); }
inline void VDP_SetBlinkTime(u8 time) { VDP_RegWrite(13, time); }
inline void VDP_SetBlinkTime2(u8 even, u8 odd) { VDP_RegWrite(13, odd | (even << 4)); }
inline void VDP_SetInfiniteBlink() { VDP_RegWrite(13, 0x10); }
inline void VDP_CleanBlinkScreen() { VDP_FillVRAM(0x00, g_ScreenColorLow, g_ScreenColorHigh, 80 * 27 / 8); }
inline void VDP_SetBlinkScreen() { VDP_FillVRAM(0xFF, g_ScreenColorLow, g_ScreenColorHigh, 80 * 27 / 8); }
inline void VDP_SetBlinkLine(u8 y) { VDP_FillVRAM(0xFF, g_ScreenColorLow + y * 10, g_ScreenColorHigh, 10); }
inline void VDP_SetBlinkChunk(u8 x, u8 y) { VDP_Poke(0xFF, g_ScreenColorLow + (y * 10) + (x / 8), g_ScreenColorHigh); }
inline void VDP_SetBlinkChunkMask(u8 x, u8 y, u8 mask) { VDP_Poke(mask, g_ScreenColorLow + (y * 10) + (x / 8), g_ScreenColorHigh); }
inline void VDP_SetBlinkChunkX(u8 x, u8 y, u8 num) { VDP_FillVRAM(0xFF, g_ScreenColorLow + (y * 10) + (x / 8), g_ScreenColorHigh, num); }

//----- GM2 helpers
inline void VDP_FillScreen_GM2(u8 value) { VDP_FillVRAM(value, g_ScreenLayoutLow, g_ScreenLayoutHigh, 32 * 24); }
inline void VDP_Poke_GM2(u8 x, u8 y, u8 value) { VDP_Poke(value, g_ScreenLayoutLow + (y * 32) + x, g_ScreenLayoutHigh); }
inline u8 VDP_Peek_GM2(u8 x, u8 y) { return VDP_Peek(g_ScreenLayoutLow + (y * 32) + x, g_ScreenLayoutHigh); }
inline void VDP_LoadBankPattern_GM2(const u8* src, u8 count, u8 bank, u8 offset) { VDP_WriteVRAM(src, g_ScreenPatternLow + (bank * 0x800) + (offset * 8), g_ScreenPatternHigh, count * 8); }
inline void VDP_LoadBankColor_GM2(const u8* src, u8 count, u8 bank, u8 offset) { VDP_WriteVRAM(src, g_ScreenColorLow + (bank * 0x800) + (offset * 8), g_ScreenColorHigh, count * 8); }
inline VADDR VDP_GetColorTable_GM2(u8 bank) { return VDP_GetColorTable() + (bank * 2048); }
inline VADDR VDP_GetPatternTable_GM2(u8 bank) { return VDP_GetPatternTable() + (bank * 2048); }

//----- Command engine wrappers (build g_VDP_Command then setup)
inline void VDP_CommandHMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NX = nx; g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = *addr; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_HMMC;
	VDP_CommandSetupR36(); VDP_CommandWriteLoop(addr);
}
inline void VDP_CommandHMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny) { VDP_CommandHMMC_Arg(addr, dx, dy, nx, ny, 0); }

inline void VDP_CommandYMMM(u16 sy, u16 dx, u16 dy, u16 ny, u8 dir)
{
	g_VDP_Command.SY = sy; g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NY = ny;
	g_VDP_Command.ARG = dir; g_VDP_Command.CMD = VDP_CMD_YMMM;
	VDP_CommandSetupR32();
}

inline void VDP_CommandHMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)
{
	g_VDP_Command.SX = sx; g_VDP_Command.SY = sy; g_VDP_Command.DX = dx; g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx; g_VDP_Command.NY = ny; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_HMMM;
	VDP_CommandSetupR32();
}
inline void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny) { VDP_CommandHMMM_Arg(sx, sy, dx, dy, nx, ny, 0); }

inline void VDP_CommandHMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NX = nx; g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_HMMV;
	VDP_CommandSetupR36();
}
inline void VDP_CommandHMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col) { VDP_CommandHMMV_Arg(dx, dy, nx, ny, col, 0); }

inline void VDP_CommandLMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NX = nx; g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = *addr; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_LMMC + op;
	VDP_CommandSetupR36(); VDP_CommandWriteLoop(addr);
}
inline void VDP_CommandLMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op) { VDP_CommandLMMC_Arg(addr, dx, dy, nx, ny, op, 0); }

inline void VDP_CommandLMCM() { }

inline void VDP_CommandLMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)
{
	g_VDP_Command.SX = sx; g_VDP_Command.SY = sy; g_VDP_Command.DX = dx; g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx; g_VDP_Command.NY = ny; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_LMMM + op;
	VDP_CommandSetupR32();
}
inline void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op) { VDP_CommandLMMM_Arg(sx, sy, dx, dy, nx, ny, op, 0); }

inline void VDP_CommandLMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op, u8 arg)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NX = nx; g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_LMMV + op;
	VDP_CommandSetupR36();
}
inline void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op) { VDP_CommandLMMV_Arg(dx, dy, nx, ny, col, op, 0); }

inline void VDP_CommandLINE(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg, u8 op)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.NX = nx; g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col; g_VDP_Command.ARG = arg; g_VDP_Command.CMD = VDP_CMD_LINE + op;
	VDP_CommandSetupR36();
}

inline void VDP_CommandSRCH(u16 sx, u16 sy, u8 col, u8 arg)
{
	g_VDP_Command.SX = sx; g_VDP_Command.SY = sy; g_VDP_Command.CLR = col; g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_SRCH;
	VDP_CommandSetupR32();
}

inline void VDP_CommandPSET(u16 dx, u16 dy, u8 col, u8 op)
{
	g_VDP_Command.DX = dx; g_VDP_Command.DY = dy; g_VDP_Command.CLR = col; g_VDP_Command.ARG = 0;
	g_VDP_Command.CMD = VDP_CMD_PSET + op;
	VDP_CommandSetupR36();
}

inline u8 VDP_CommandPOINT(u16 sx, u16 sy)
{
	g_VDP_Command.SX = sx; g_VDP_Command.SY = sy; g_VDP_Command.CMD = VDP_CMD_POINT;
	VDP_CommandSetupR32(); VDP_CommandWait();
	return VDP_ReadStatus(7);
}

inline void VDP_CommandSTOP() { VDP_RegWrite(46, VDP_CMD_STOP); }

//----- Command aliases
#define VDP_CopyRAMtoVRAM			VDP_CommandHMMC
#define VDP_YMoveVRAM				VDP_CommandYMMM
#define VDP_MoveVRAM				VDP_CommandHMMM
#define VDP_LogicalCopyRAMtoVRAM	VDP_CommandLMMC
#define VDP_LogicalYMoveVRAM		VDP_CommandLMCM
#define VDP_LogicalMoveVRAM			VDP_CommandLMMM
#define VDP_LogicalFillVRAM			VDP_CommandLMMV
#define VDP_DrawLine				VDP_CommandLINE
#define VDP_SearchColor				VDP_CommandSRCH
#define VDP_DrawPoint				VDP_CommandPSET
#define VDP_ReadPoint				VDP_CommandPOINT
#define VDP_AbortCommand			VDP_CommandSTOP

#endif // MSXMVL_VDP_H
