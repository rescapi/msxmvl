// msxmvl clean-room reimplementation of MSXgl "print" module.
//
// Written from the documented contract (print.h) + standard MSX/V9938 knowledge
// only. Rendering targets a screen-5 style 4bpp bitmap for the BITMAP/VRAM
// back-ends, the pattern name table for TEXT, and the sprite attribute/pattern
// tables for SPRITE. The print module carries no __PRESERVES contract (the
// vendor header declares plain C functions), so everything is portable C and
// SDCC honours the ABI for the VDP calls.

// The GRAPHIC2/3 pattern-bank replication in Print_SetTextFont needs VDP_GetMode()
// and the mode constants. msxmvl's own print.h does not pull vdp.h; include it
// FIRST so print.h's #ifndef fallbacks for the same VDP macros stay quiet.
// MSXgl's print.h includes vdp.h itself (#pragma once), so this is a no-op there.
#include "vdp.h"
#include "print.h"

#if (PRINT_USE_FORMAT) && !defined(va_arg)
#include <stdarg.h>   /* standalone build; in-tree MSXgl provides va_* via string.h */
#endif

//=============================================================================
// GLOBALS
//=============================================================================

#if (PRINT_USE_MULTIFONT)
Print_Data* g_PrintData;
#else
Print_Data g_PrintData;
#endif

// Assumed bitmap geometry (screen 5 / GRAPHIC 4): 4 bits per pixel.
#define PRINT_BPP			4
#define PRINT_TEXT_ROWS		24
#define PRINT_SCREEN_HEIGHT	212

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

// Current text color, independent of the PRINT_COLOR_NUM config (scalar vs
// per-line array). Drop-in builds may enable multi-line color; use line 0.
#if (PRINT_COLOR_NUM == 1)
	#define PRINT_CUR_TEXTCOLOR   (PRINT_DATA.TextColor)
#else
	#define PRINT_CUR_TEXTCOLOR   (PRINT_DATA.TextColor[0])
#endif

// Bitmap text works in GRAPHIC 4/5/6/7, which differ in bits-per-pixel and hence in how
// many pixels pack into a VRAM byte. g_Print_Bpp caches the current mode's depth so the
// glyph expander and colour table build the right layout without a per-glyph mode read:
//   4 -> GRAPHIC4 (S5) & GRAPHIC6 (S7): 2 px/byte, colours 0..15
//   2 -> GRAPHIC5 (S6):                 4 px/byte, colours 0..3
//   8 -> GRAPHIC7 (S8):                 1 px/byte, colours 0..255
// Derived from VDP_GetMode() in Print_SetMode/Print_SetColor (both entered with the VDP
// mode already established), so a bitmap draw never has to query the mode itself.
static u8 g_Print_Bpp = PRINT_BPP;

#if (PRINT_USE_G5G7)
// GRAPHIC5/GRAPHIC7 support present. The bit depth of a bitmap VDP mode.
//
// A MACRO, not a static function. SDCC does not inline a static this small, so each of the
// three callers paid a real `call` plus its compares -- ~95 T -- to fetch a number that folds
// into two comparisons. MSXgl does the equivalent switch inline in Print_SetMode, and that
// call was the whole of our 29% deficit there.
#define Print_BppOfMode(vmode) \
	(((vmode) == VDP_MODE_GRAPHIC5) ? 2 : (((vmode) == VDP_MODE_GRAPHIC7) ? 8 : 4))
#endif

// Build the pixel-expansion table for a fg/bg pair at the current bit depth.
//   4bpp: t[4]        indexed by a 2-bit glyph pair  -> one byte = 2 pixels
//   2bpp: t[16]       indexed by a 4-bit glyph nibble -> one byte = 4 pixels
//   8bpp: (u16)t[4]   indexed by a 2-bit glyph pair  -> one u16  = 2 pixels
// In every case the leftmost glyph pixel maps to the lowest-address VRAM pixel.
static void Print_BuildTable(u8 fg, u8 bg, u8* t)
{
#if (PRINT_USE_G5G7)
	if (g_Print_Bpp == 2)
	{
		// 4 pixels per byte; bit 3 of the index is the leftmost pixel.
		u8 n;
		for (n = 0; n < 16; ++n)
			t[n] = (u8)((((n & 8) ? fg : bg) << 6) | (((n & 4) ? fg : bg) << 4)
			          | (((n & 2) ? fg : bg) << 2) |  ((n & 1) ? fg : bg));
		return;
	}
	if (g_Print_Bpp == 8)
	{
		// 2 pixels per u16; low byte = left pixel (VRAM is little-endian in address order).
		u16* w = (u16*)t;
		w[0] = (u16)(bg | (bg << 8));	// left bg, right bg
		w[1] = (u16)(bg | (fg << 8));	// left bg, right fg
		w[2] = (u16)(fg | (bg << 8));	// left fg, right bg
		w[3] = (u16)(fg | (fg << 8));	// left fg, right fg
		return;
	}
#endif
	// 4bpp (index bits: (leftPixel << 1) | rightPixel ; left pixel = high nibble).
	t[0] = (bg << 4) | bg;
	t[1] = (bg << 4) | fg;
	t[2] = (fg << 4) | bg;
	t[3] = (fg << 4) | fg;
}

#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
// Expand a 1bpp glyph (PatternY rows of 8 pixels) into the current mode's packed
// pixel format using table 't' (built by Print_BuildTable for the same bit depth).
static void Print_ExpandGlyph(u8 chr, const u8* t, u8* buf)
{
	const u8* g = PRINT_DATA.FontAddr + (u16)chr * PRINT_DATA.PatternY;
	u8 h = PRINT_DATA.PatternY;
#if (PRINT_USE_G5G7)
	if (g_Print_Bpp == 2)			// GRAPHIC5: 4 px/byte -> 2 bytes/row
	{
		while (h--)
		{
			u8 b = *g++;
			*buf++ = t[b >> 4];
			*buf++ = t[b & 0x0F];
		}
		return;
	}
	if (g_Print_Bpp == 8)			// GRAPHIC7: 1 px/byte -> 8 bytes/row (u16 per pair)
	{
		const u16* w = (const u16*)t;
		while (h--)
		{
			u8 b = *g++;
			*(u16*)buf = w[(b >> 6) & 3]; buf += 2;
			*(u16*)buf = w[(b >> 4) & 3]; buf += 2;
			*(u16*)buf = w[(b >> 2) & 3]; buf += 2;
			*(u16*)buf = w[b & 3];        buf += 2;
		}
		return;
	}
#endif
	while (h--)						// GRAPHIC4/6: 2 px/byte -> 4 bytes/row
	{
		u8 b = *g++;
		*buf++ = t[(b >> 6) & 3];
		*buf++ = t[(b >> 4) & 3];
		*buf++ = t[(b >> 2) & 3];
		*buf++ = t[b & 3];
	}
}

// Draw a glyph to (x,y) in bitmap mode using the currently prepared table.
static void Print_BlitGlyph(u8 chr, UX x, UY y, const u8* t)
{
	// Worst case is GRAPHIC7 (8bpp): 8 bytes per row. 16 rows covers any font height
	// (standard fonts are 8 tall); NX stays 8 pixels -- the VDP consumes the right byte
	// count for the current mode.
	u8 buf[128];
	Print_ExpandGlyph(chr, t, buf);
	VDP_CommandHMMC(buf, x, y, 8, PRINT_DATA.PatternY);
}
#endif

//=============================================================================
// PER-MODE CHARACTER DRAW ROUTINES (stored in PRINT_DATA.DrawChar)
// Each routine draws the glyph at the current cursor; cursor advance is done
// by Print_DrawChar.
//=============================================================================

#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
static void Print_DrawChar_Bitmap(u8 chr)
{
#if (PRINT_USE_FX_SHADOW)
	if (PRINT_DATA.FX & PRINT_FX_SHADOW)
	{
		u8 st[4];
		i8 ox = (i8)PRINT_DATA.ShadowOffsetX - 3;
		i8 oy = (i8)PRINT_DATA.ShadowOffsetY - 3;
		Print_BuildTable(PRINT_DATA.ShadowColor, PRINT_DATA.BGColor, st);
		Print_BlitGlyph(chr, PRINT_DATA.CursorX + ox, PRINT_DATA.CursorY + oy, st);
	}
#endif
#if (PRINT_USE_FX_OUTLINE)
	if (PRINT_DATA.FX & PRINT_FX_OUTLINE)
	{
		u8 ot[4];
		Print_BuildTable(PRINT_DATA.OutlineColor, PRINT_DATA.BGColor, ot);
		Print_BlitGlyph(chr, PRINT_DATA.CursorX - 1, PRINT_DATA.CursorY,     ot);
		Print_BlitGlyph(chr, PRINT_DATA.CursorX + 1, PRINT_DATA.CursorY,     ot);
		Print_BlitGlyph(chr, PRINT_DATA.CursorX,     PRINT_DATA.CursorY - 1, ot);
		Print_BlitGlyph(chr, PRINT_DATA.CursorX,     PRINT_DATA.CursorY + 1, ot);
	}
#endif
	Print_BlitGlyph(chr, PRINT_DATA.CursorX, PRINT_DATA.CursorY, PRINT_DATA.Buffer);
}

static void Print_DrawChar_BitmapTrans(u8 chr)
{
	u8 t[4];
	Print_BuildTable(PRINT_CUR_TEXTCOLOR, 0, t); // background pixels = transparent color 0
	Print_BlitGlyph(chr, PRINT_DATA.CursorX, PRINT_DATA.CursorY, t);
}
#endif

#if (PRINT_USE_VRAM)
static void Print_DrawChar_BitmapVRAM(u8 chr)
{
	u8 idx = chr - PRINT_DATA.CharFirst;
	u16 sx = (idx % PRINT_DATA.CharPerLine) * PRINT_DATA.UnitX;
	u16 sy = PRINT_DATA.FontVRAMY + (idx / PRINT_DATA.CharPerLine) * PRINT_DATA.UnitY;
	u8 op = (PRINT_DATA.SourceMode == PRINT_MODE_BITMAP_VRAM_TRANS) ? VDP_OP_TIMP : VDP_OP_IMP;
	VDP_CommandLMMM(sx, sy, PRINT_DATA.CursorX, PRINT_DATA.CursorY, PRINT_DATA.UnitX, PRINT_DATA.UnitY, op);
}
#endif

#if (PRINT_USE_SPRITE)
static void Print_DrawChar_Sprite(u8 chr)
{
	u16 attr = g_SpriteAttributeLow + (u16)PRINT_DATA.SpriteID * 4;
	VDP_Poke_16K((u8)PRINT_DATA.CursorY, attr);
	VDP_Poke_16K((u8)PRINT_DATA.CursorX, attr + 1);
	VDP_Poke_16K(PRINT_DATA.SpritePattern + (chr - PRINT_DATA.CharFirst), attr + 2);
	VDP_Poke_16K(PRINT_CUR_TEXTCOLOR, attr + 3);
	PRINT_DATA.SpriteID++;
}
#endif

#if (PRINT_USE_TEXT)
static void Print_DrawChar_Text(u8 chr)
{
	u16 cols = PRINT_DATA.ScreenWidth;   // text modes: ScreenWidth IS the column count
	u16 addr = g_ScreenLayoutLow + (u16)PRINT_DATA.CursorY * cols + PRINT_DATA.CursorX;
	VDP_Poke_16K((u8)(PRINT_DATA.PatternOffset + (chr - PRINT_DATA.CharFirst)), addr);
}
#endif

//=============================================================================
// INITIALIZATION
//=============================================================================

void Print_SetMode(u8 mode)
{
	PRINT_DATA.SourceMode = mode;
#if (PRINT_USE_G5G7)
	// Cache the bit depth of the current bitmap VDP mode so glyph packing matches it
	// (2bpp GRAPHIC5, 8bpp GRAPHIC7, else 4bpp). MSXgl selects DrawChar_2B/4B/8B here;
	// msxmvl keeps one draw routine and switches the packing on this cached depth.
	//
	// Written as plain assignments, NOT `g_Print_Bpp = Print_BppOfMode(...)`. From the ternary
	// SDCC materialised each result as a 16-bit constant in DE and then stored the low byte
	// through IY -- `ld iy,#_g_Print_Bpp` + `ld 0(iy),e`, ~33 T to write ONE byte. Assigning
	// the u8 directly in each arm gives `ld a,#n` / `ld (_g_Print_Bpp),a` (~17 T).
	{
		u8 vmode = VDP_GetMode();
		g_Print_Bpp = 4;						// GRAPHIC4 / GRAPHIC6
		if (vmode == VDP_MODE_GRAPHIC5)
			g_Print_Bpp = 2;
		else if (vmode == VDP_MODE_GRAPHIC7)
			g_Print_Bpp = 8;
	}
#endif
	// An if-chain, NOT a switch. There are only six modes, and SDCC turned the switch into a
	// computed jump: `ld a,#5 / sub c / ret c / ld b,#0 / ld hl,#tbl / add hl,bc (x3) / jp (hl)`
	// -- about 80 T before it reaches ANY case. The chain costs ~15 T to reach the first mode
	// and ~60 T to reach the last, so it is cheaper for EVERY mode here, not just the common
	// one. A jump table only starts paying once there are many more cases to amortise it over.
	// (Bitmap is also the mode that matters most on MSX2, and it is first.)
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	if (mode == PRINT_MODE_BITMAP)
		PRINT_DATA.DrawChar = Print_DrawChar_Bitmap;
	else if (mode == PRINT_MODE_BITMAP_TRANS)
		PRINT_DATA.DrawChar = Print_DrawChar_BitmapTrans;
	else
#endif
#if (PRINT_USE_VRAM)
	if ((mode == PRINT_MODE_BITMAP_VRAM) || (mode == PRINT_MODE_BITMAP_VRAM_TRANS))
		PRINT_DATA.DrawChar = Print_DrawChar_BitmapVRAM;
	else
#endif
#if (PRINT_USE_SPRITE)
	if (mode == PRINT_MODE_SPRITE)
		PRINT_DATA.DrawChar = Print_DrawChar_Sprite;
	else
#endif
#if (PRINT_USE_TEXT)
	if (mode == PRINT_MODE_TEXT)
		PRINT_DATA.DrawChar = Print_DrawChar_Text;
	else
#endif
	{ }		// unknown mode: leave the current DrawChar in place
}

bool Print_Initialize()
{
	PRINT_DATA.PatternX     = 8;
	PRINT_DATA.PatternY     = 8;
	PRINT_DATA.UnitX        = 1;
	PRINT_DATA.UnitY        = 1;
	PRINT_DATA.TabSize      = PRINT_TAB_SIZE;
	PRINT_DATA.CursorX      = 0;
	PRINT_DATA.CursorY      = 0;
	PRINT_DATA.BGColor      = 0;
	// ScreenWidth follows MSXgl's convention: the number of COLUMNS in the text/tile
	// modes and the pixel width in the bitmap modes. Hardcoding 256 (and dividing by 8
	// at the use site) happened to give the right 32 columns for GRAPHIC 1/2/3, but was
	// wrong for TEXT1 (40 columns) and TEXT2 (80) -- every text row was addressed with
	// the wrong stride.
	switch (VDP_GetMode())
	{
	case VDP_MODE_TEXT1:
		PRINT_DATA.ScreenWidth = 40;
		break;
	case VDP_MODE_MULTICOLOR:
	case VDP_MODE_GRAPHIC1:
	case VDP_MODE_GRAPHIC2:
		PRINT_DATA.ScreenWidth = 32;
		break;
#if (MSX_VERSION >= MSX_2)
	case VDP_MODE_TEXT2:
		PRINT_DATA.ScreenWidth = 80;
		break;
	case VDP_MODE_GRAPHIC3:
		PRINT_DATA.ScreenWidth = 32;
		break;
	case VDP_MODE_GRAPHIC5:
	case VDP_MODE_GRAPHIC6:
		PRINT_DATA.ScreenWidth = 512;
		break;
#endif
	default:
		PRINT_DATA.ScreenWidth = 256;
		break;
	}
	// MSXgl's Print_Initialize never touches the font or PatternOffset, so a caller
	// may select a font first and then initialize (Print_SetTextFont relies on that
	// order, because Print_SetColor below paints the colour table for exactly the
	// selected font's characters). Only install the fallback font when none is set.
	if (PRINT_DATA.FontPatterns == NULL)
	{
		PRINT_DATA.CharFirst    = 0;
		PRINT_DATA.CharLast     = 255;
		PRINT_DATA.CharCount    = 255;
		PRINT_DATA.FontPatterns = PRINT_DEFAULT_FONT;
		PRINT_DATA.FontAddr     = PRINT_DEFAULT_FONT;
#if (PRINT_USE_TEXT)
		PRINT_DATA.PatternOffset = 0;
#endif
	}
#if (PRINT_USE_VRAM)
	PRINT_DATA.FontVRAMY    = 0;
	PRINT_DATA.CharPerLine  = 32;
#endif
#if (PRINT_USE_SPRITE)
	PRINT_DATA.SpritePattern = 0;
	PRINT_DATA.SpriteID      = 0;
#endif
#if ((PRINT_USE_FX_SHADOW) || (PRINT_USE_FX_OUTLINE))
	PRINT_DATA.FX = 0;
#endif
#if (PRINT_USE_DIRECTION)
	PRINT_DATA.Direction = PRINT_DIR_LEFT;
#endif
	Print_SetColor(0x0F, 0x00);
#if (PRINT_USE_TEXT)
	Print_SetMode(PRINT_MODE_TEXT);
#elif ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	Print_SetMode(PRINT_MODE_BITMAP);
#endif
	return TRUE;
}

void Print_SetFont(const u8* font)
{
	// Selecting a font must not change the draw mode -- Print_SetBitmapFont() owns that
	// decision. Forcing PRINT_MODE_BITMAP here cost every caller a Print_SetMode() they
	// did not ask for (and silently overrode a text-mode selection).
	//
	// The fields are assigned AS THEY ARE COMPUTED rather than handed to Print_SetFontEx as
	// seven arguments. Same work, but the seven-argument form keeps all four unpacked nibbles
	// live at once, and SDCC responded by building an IX stack frame, spilling every one of
	// them into it, and reloading each with an `ld -5(ix),a` (19 T) to store a single byte.
	// Exactly the failure that made Mem_DynamicAlloc slow. Print_SetFontEx stays in the header
	// for callers who want it -- this is just the version that does not pay for the call shape.
	u8 f0 = font[0];
	u8 f1 = font[1];
	u8 first = font[2];
	u8 py = f0 & 0x0F;

	PRINT_DATA.PatternX     = f0 >> 4;
	PRINT_DATA.PatternY     = py;
	PRINT_DATA.UnitX        = f1 >> 4;
	PRINT_DATA.UnitY        = f1 & 0x0F;
	PRINT_DATA.CharFirst    = first;
	PRINT_DATA.CharLast     = font[3];
	PRINT_DATA.CharCount    = font[3] - first + 1;
	PRINT_DATA.FontPatterns = font + 4;
	PRINT_DATA.FontAddr     = PRINT_DATA.FontPatterns - (first * py);
}

#if (PRINT_USE_BITMAP)
void Print_SetBitmapFont(const u8* font)
{
	Print_Initialize();
	Print_SetFont(font);
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	Print_SetMode(PRINT_MODE_BITMAP);
#endif
}
#endif

#if (PRINT_USE_VRAM)
void Print_SetVRAMFont(const u8* font, UY y, u8 color, bool trans)
{
	u8 i;
	u8 buf[64];
	u8 t[4];
	Print_SetFontEx(font[0] >> 4, font[0] & 0x0F, font[1] >> 4, font[1] & 0x0F, font[2], font[3], font + 4);
	PRINT_DATA.FontVRAMY   = y;
	PRINT_DATA.CharPerLine = PRINT_DATA.ScreenWidth / PRINT_DATA.UnitX;
	Print_BuildTable(color, trans ? 0 : PRINT_DATA.BGColor, t);
	for (i = 0; i < PRINT_DATA.CharCount; ++i)
	{
		u8 chr = PRINT_DATA.CharFirst + i;
		u16 col = i % PRINT_DATA.CharPerLine;
		u16 row = i / PRINT_DATA.CharPerLine;
		Print_ExpandGlyph(chr, t, buf);
		VDP_CommandHMMC(buf, col * PRINT_DATA.UnitX, y + row * PRINT_DATA.UnitY, 8, PRINT_DATA.PatternY);
	}
	Print_SetMode(trans ? PRINT_MODE_BITMAP_VRAM_TRANS : PRINT_MODE_BITMAP_VRAM);
}
#endif

#if (PRINT_USE_TEXT)
void Print_SetTextFont(const u8* font, u8 offset)
{
	// Order matters, and matches MSXgl: select the font (which sets PatternOffset and
	// the character range) BEFORE Print_Initialize(), because Initialize's Print_SetColor
	// paints the colour table for exactly those characters. Skipping Initialize entirely
	// -- as this used to -- left PRINT_DATA at BSS zeros: CharCount == 0 copied no font
	// at all, and ScreenWidth == 0 made Print_DrawChar_Text address column 0 of every row.
	Print_SelectTextFont(font, offset);
	Print_Initialize();
	Print_SetMode(PRINT_MODE_TEXT);

	u16 dst = (u16)(g_ScreenPatternLow + (u16)offset * 8);
	u16 len = (u16)PRINT_DATA.CharCount * 8;
	VDP_WriteVRAM_16K(font + 4, dst, len);

	// GRAPHIC 2/3 split the pattern generator into THREE independent banks of 256
	// characters (one per screen third). The font must be replicated into all of
	// them or text only appears in the top third. Same as MSXgl.
	switch (VDP_GetMode())
	{
#if (MSX_VERSION >= MSX_2)
	case VDP_MODE_GRAPHIC3:
#endif
	case VDP_MODE_GRAPHIC2:
		VDP_WriteVRAM_16K(font + 4, (u16)(dst + 256 * 8), len);
		VDP_WriteVRAM_16K(font + 4, (u16)(dst + 512 * 8), len);
		break;
	default:
		break;
	}
}
#endif

#if (PRINT_USE_SPRITE)
void Print_SetSpriteFont(const u8* font, u8 patIdx, u8 sprtIdx)
{
	Print_SetFontEx(font[0] >> 4, font[0] & 0x0F, font[1] >> 4, font[1] & 0x0F, font[2], font[3], font + 4);
	PRINT_DATA.SpritePattern = patIdx;
	PRINT_DATA.SpriteID      = sprtIdx;
	VDP_WriteVRAM_16K(font + 4, (u16)(g_SpritePatternLow + (u16)patIdx * 8), (u16)PRINT_DATA.CharCount * 8);
	Print_SetMode(PRINT_MODE_SPRITE);
}
#endif

//=============================================================================
// DRAW
//=============================================================================

void Print_DrawChar(u8 chr)
{
#if (PRINT_USE_VALIDATOR)
	// Auto-wrap to the next line when the glyph would cross the right edge, as MSXgl does
	// under PRINT_USE_VALIDATOR. In MSX2 bitmap modes the pending VDP command must finish
	// before the wrap repositions the cursor.
	if (PRINT_DATA.CursorX + PRINT_W(PRINT_DATA.UnitX) > PRINT_DATA.ScreenWidth)
		Print_Return();
	#if ((MSX_VERSION >= MSX_2) && (PRINT_USE_BITMAP))
		VDP_CommandWait();
	#endif
#endif
	PRINT_DATA.DrawChar(chr);
	PRINT_DATA.CursorX += PRINT_W(PRINT_DATA.UnitX);
}

void Print_DrawCharX(c8 chr, u8 num)
{
	while (num--)
		Print_DrawChar((u8)chr);
}

void Print_DrawText(const c8* string)
{
	c8 c;
	while ((c = *string++) != 0)
	{
		if (c == '\n')
			Print_Return();
		else if (c == '\t')
			Print_Tab();
#if (PRINT_SKIP_SPACE)
		else if (c == ' ')
			Print_Space();
#endif
		else
			Print_DrawChar((u8)c);
	}
}

//-----------------------------------------------------------------------------
// Numeric

static void Print_DrawNibble(u8 n)
{
	Print_DrawChar((u8)(n < 10 ? '0' + n : 'A' + (n - 10)));
}

void Print_DrawHex8(u8 value)
{
	Print_DrawNibble(value >> 4);
	Print_DrawNibble(value & 0x0F);
}

void Print_DrawHex16(u16 value)
{
	Print_DrawHex8((u8)(value >> 8));
	Print_DrawHex8((u8)(value & 0xFF));
}

void Print_DrawBin8(u8 value)
{
	u8 i = 8;
	while (i--)
		Print_DrawChar((u8)('0' + ((value >> i) & 1)));
}

#if (PRINT_USE_32B)
void Print_DrawHex32(u32 value)
{
	Print_DrawHex16((u16)(value >> 16));
	Print_DrawHex16((u16)(value & 0xFFFF));
}

static void Print_DrawUInt(u32 u)
{
	// "u % 10" then "u /= 10" is TWO calls to __divulong per digit. One division gives
	// both: rem = u - q*10 (a 32-bit *10 is cheap shifts/adds). And once the value fits
	// in 16 bits -- which is immediately, for every value a caller is likely to print --
	// switch to __divuint, which is several times cheaper than __divulong.
	c8 buf[11];
	u8 i = 0;
	u16 v;

	while (u > 0xFFFF)
	{
		u32 q = u / 10;
		buf[i++] = (c8)('0' + (u8)(u - (q * 10)));
		u = q;
	}

	v = (u16)u;
	do
	{
		u16 q = v / 10;
		buf[i++] = (c8)('0' + (u8)(v - (q * 10)));
		v = q;
	} while (v);

	while (i)
		Print_DrawChar((u8)buf[--i]);
}

void Print_DrawInt(i32 value)
{
	u32 u;
	if (value < 0)
	{
		Print_DrawChar('-');
		u = (u32)(-value);
	}
	else
		u = (u32)value;
	Print_DrawUInt(u);
}
#else
static void Print_DrawUInt(u16 u)
{
	// One __divuint per digit instead of two (see the 32-bit variant above).
	c8 buf[6];
	u8 i = 0;
	do
	{
		u16 q = u / 10;
		buf[i++] = (c8)('0' + (u8)(u - (q * 10)));
		u = q;
	} while (u);
	while (i)
		Print_DrawChar((u8)buf[--i]);
}

void Print_DrawInt(i16 value)
{
	u16 u;
	if (value < 0)
	{
		Print_DrawChar('-');
		u = (u16)(-value);
	}
	else
		u = (u16)value;
	Print_DrawUInt(u);
}
#endif

//-----------------------------------------------------------------------------
// Format

#if (PRINT_USE_FORMAT)
static void Print_DrawHexDigits(u32 v, u8 digits)
{
	while (digits)
	{
		--digits;
		Print_DrawNibble((u8)((v >> (digits * 4)) & 0x0F));
	}
}

void Print_DrawFormat(const c8* format, ...)
{
	va_list ap;
	c8 c;
	va_start(ap, format);
	while ((c = *format++) != 0)
	{
		if (c == '\n')
		{
			Print_Return();
			continue;
		}
		if (c == '\t')
		{
			Print_Tab();
			continue;
		}
		if (c != '%')
		{
#if (PRINT_SKIP_SPACE)
			if (c == ' ')
				Print_Space();
			else
#endif
				Print_DrawChar((u8)c);
			continue;
		}
		// parse optional field width
		u8 width = 0;
		bool haswidth = FALSE;
		while (*format >= '0' && *format <= '9')
		{
			width = width * 10 + (u8)(*format++ - '0');
			haswidth = TRUE;
		}
		c = *format++;
		switch (c)
		{
		case 'd':
		case 'i':
			Print_DrawInt((i16)va_arg(ap, int));
			break;
		case 'u':
			Print_DrawUInt((unsigned int)va_arg(ap, unsigned int));
			break;
		case 'x':
			Print_DrawHexDigits((u32)(unsigned int)va_arg(ap, unsigned int), haswidth ? width : 4);
			break;
		case 'c':
			Print_DrawChar((u8)va_arg(ap, int));
			break;
		case 's':
			Print_DrawText((const c8*)va_arg(ap, char*));
			break;
#if (PRINT_USE_32B)
		case 'I':
		case 'D':
			Print_DrawInt((i32)va_arg(ap, long));
			break;
		case 'U':
			Print_DrawUInt((u32)va_arg(ap, unsigned long));
			break;
		case 'X':
			Print_DrawHexDigits((u32)va_arg(ap, unsigned long), haswidth ? width : 8);
			break;
#endif
		case '%':
			Print_DrawChar('%');
			break;
		default:
			Print_DrawChar((u8)c);
			break;
		}
	}
	va_end(ap);
}
#endif // (PRINT_USE_FORMAT)

//=============================================================================
// HELPER
//=============================================================================

void Print_Clear()
{
	PRINT_DATA.CursorX = 0;
	PRINT_DATA.CursorY = 0;
	switch (PRINT_DATA.SourceMode)
	{
#if (PRINT_USE_TEXT)
	case PRINT_MODE_TEXT:
	{
		u16 cols = PRINT_DATA.ScreenWidth;   // text modes: column count
		u8 sp = (u8)(PRINT_DATA.PatternOffset + (' ' - PRINT_DATA.CharFirst));
		VDP_FillVRAM_16K(sp, g_ScreenLayoutLow, cols * PRINT_TEXT_ROWS);
		break;
	}
#endif
#if (PRINT_USE_SPRITE)
	case PRINT_MODE_SPRITE:
		PRINT_DATA.SpriteID = 0;
		break;
#endif
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	default:
	{
		// High-speed per-BYTE fill (HMMV) instead of the per-PIXEL logical fill
		// (LMMV): in GRAPHIC4 a byte packs 2 pixels, so replicate BGColor into
		// both nibbles and let the VDP clear whole bytes (~pixels-per-byte fewer
		// VDP cycles). Full-width, byte-aligned clear => byte-identical result.
		u8 fill = (u8)((PRINT_DATA.BGColor << 4) | PRINT_DATA.BGColor);
		VDP_CommandHMMV(0, 0, PRINT_DATA.ScreenWidth, PRINT_SCREEN_HEIGHT, fill);
		// WAIT for the fill to finish. Returning while a full-screen HMMV is still sweeping
		// looked 14.6% faster, but the cost was only DEFERRED -- and worse, it was a race:
		// VDP_CommandSetupR32/R36 wait for the engine before issuing the next COMMAND, but a
		// direct VRAM write does not, and Print_DrawText blits glyph bytes straight to VRAM.
		// Text drawn immediately after a clear was racing the fill that was still coming for
		// it. Measured: the deferred cost reappeared in the very next segment, which ran 57%
		// slower (462k -> 728k cycles) as its VRAM accesses fought the running command.
		VDP_CommandWait();
		break;
	}
#else
	default:
		break;
#endif
	}
}

void Print_Backspace(u8 num)
{
	// Dispatch on the ACTUAL VDP mode, as MSXgl does -- not on PRINT_DATA.SourceMode.
	// A caller can leave SourceMode at TEXT while the VDP sits in a bitmap screen, and
	// the text path would then poke the (non-existent) name table.
	if (VDP_IsBitmapMode(VDP_GetMode()))
	{
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
		// Erase the last <num> cells with one high-speed byte fill (HMMV) rather than
		// re-blitting <num> space glyphs.
		//
		// TESTED AND REJECTED: blitting space glyphs for short runs (num <= 4), on the
		// theory that HMMV's seven command registers + R#36 setup would cost more than a
		// couple of glyphs. It does not -- that variant measured 1,152,483 cycles against
		// this one's 746,191. The glyph path is far more expensive. HMMV stays.
		// (Print_Backspace is still ~6% slower than MSXgl; the cost is NOT the choice of
		// erase primitive, so look elsewhere -- probably the per-call VDP_GetMode().)
		u16 back = (u16)num * PRINT_W(PRINT_DATA.UnitX);
		UX x = (PRINT_DATA.CursorX >= back) ? (PRINT_DATA.CursorX - back) : 0;
		u8 fill = (u8)((PRINT_DATA.BGColor << 4) | PRINT_DATA.BGColor);
		VDP_CommandHMMV(x, PRINT_DATA.CursorY, back, PRINT_DATA.PatternY, fill);
		PRINT_DATA.CursorX = x;
#endif
	}
	else
	{
#if (PRINT_USE_TEXT)
		// One VRAM fill over the <num> name-table cells. This used to call
		// Print_DrawCharX(' ', num), i.e. <num> separate VDP pokes -- O(num) VDP
		// round-trips where a single fill does.
		u8 back = (PRINT_DATA.CursorX >= num) ? num : (u8)PRINT_DATA.CursorX;
		u16 dst;
		PRINT_DATA.CursorX -= back;
		dst = g_ScreenLayoutLow
		    + (u16)PRINT_DATA.CursorY * PRINT_DATA.ScreenWidth
		    + PRINT_DATA.CursorX;
		VDP_FillVRAM_16K(0, dst, back);
#endif
	}
}

//=============================================================================
// FX - COLOR
//=============================================================================

void Print_SetColor(u8 text, u8 bg)
{
#if (PRINT_COLOR_NUM == 1)
	PRINT_DATA.TextColor = text;
#else
	{
		u8 i;
		for (i = 0; i < PRINT_COLOR_NUM; ++i)
			PRINT_DATA.TextColor[i] = text;
	}
#endif
	PRINT_DATA.BGColor = bg;
#if (PRINT_USE_G5G7)
	// Match the colour buffer's packing to the current bitmap mode's depth, so a later
	// draw in GRAPHIC5/GRAPHIC7 uses the correct 2bpp/8bpp table even if SetColor runs
	// before SetMode (as it does inside Print_Initialize).
	if (VDP_IsBitmapMode(VDP_GetMode()))
		g_Print_Bpp = Print_BppOfMode(VDP_GetMode());
#endif
	Print_BuildTable(text, bg, PRINT_DATA.Buffer);

	// In the TILE modes the glyph colour does not live in a register -- it lives in the
	// VDP colour table, one entry per pattern row. So it must be painted into the colour
	// table for the font's characters, and in GRAPHIC 2/3 into each of the three banks
	// (just like the patterns). Without this the text keeps whatever colour happened to
	// be in the table -- usually invisible. Same as MSXgl.
#if (PRINT_USE_TEXT)
	if (!VDP_IsBitmapMode(VDP_GetMode()))
	{
		u8 col = (u8)((text << 4) | bg);
		switch (VDP_GetMode())
		{
		case VDP_MODE_TEXT1:		// SCREEN 0 / TEXT2: the whole screen shares one
#if (MSX_VERSION >= MSX_2)
		case VDP_MODE_TEXT2:		// text/background colour, held in VDP R#7 -- there is
#endif
			VDP_SetColor(col);		// no per-cell colour table. MSXgl writes R#7 here too;
			break;					// omitting it left text colour unchanged in TEXT modes.
		case VDP_MODE_GRAPHIC1:
			VDP_FillVRAM_16K(col, g_ScreenColorLow, 32);
			break;
#if (MSX_VERSION >= MSX_2)
		case VDP_MODE_GRAPHIC3:
#endif
		case VDP_MODE_GRAPHIC2:
		{
			u16 dst = (u16)(g_ScreenColorLow + (u16)PRINT_DATA.PatternOffset * 8);
			u16 len = (u16)PRINT_DATA.CharCount * 8;
			VDP_FillVRAM_16K(col, dst, len);
			VDP_FillVRAM_16K(col, (u16)(dst + 256 * 8), len);
			VDP_FillVRAM_16K(col, (u16)(dst + 512 * 8), len);
			break;
		}
		default:
			break;
		}
	}
#endif
}

#if (PRINT_COLOR_NUM > 1)
void Print_SetColorShade(const u8* shade)
{
	u8 i;
	for (i = 0; i < PRINT_COLOR_NUM; ++i)
		PRINT_DATA.TextColor[i] = shade[i];
	Print_BuildTable(shade[0], PRINT_DATA.BGColor, PRINT_DATA.Buffer);
}
#endif

//=============================================================================
// FX - SHADOW
//=============================================================================

#if (PRINT_USE_FX_SHADOW)
void Print_SetShadow(bool enable, i8 offsetX, i8 offsetY, u8 color)
{
	PRINT_DATA.ShadowOffsetX = (u8)(offsetX + 3) & 0x07;
	PRINT_DATA.ShadowOffsetY = (u8)(offsetY + 3) & 0x07;
	PRINT_DATA.ShadowColor   = color;
	Print_EnableShadow(enable);
}

void Print_EnableShadow(bool enable)
{
	PRINT_DATA.FX = enable ? PRINT_FX_SHADOW : 0;
	Print_SetMode(enable ? PRINT_MODE_BITMAP_TRANS : PRINT_MODE_BITMAP);
}

#if (PRINT_USE_2_PASS_FX)
void Print_DrawTextShadow(const c8* string, i8 offsetX, i8 offsetY, u8 color)
{
	UX x = PRINT_DATA.CursorX;
	UY y = PRINT_DATA.CursorY;
#if (PRINT_COLOR_NUM == 1)
	u8 keep = PRINT_DATA.TextColor;
#else
	u8 keep = PRINT_DATA.TextColor[0];
#endif
	u8 bg = PRINT_DATA.BGColor;
	// Shadow pass
	Print_SetColor(color, bg);
	Print_SetPosition(x + offsetX, y + offsetY);
	Print_DrawText(string);
	// Main pass
	Print_SetColor(keep, bg);
	Print_SetPosition(x, y);
	Print_DrawText(string);
}
#endif
#endif // (PRINT_USE_FX_SHADOW)

//=============================================================================
// FX - OUTLINE
//=============================================================================

#if (PRINT_USE_FX_OUTLINE)
void Print_SetOutline(bool enable, u8 color)
{
	PRINT_DATA.OutlineColor = color;
	// Body of Print_EnableOutline inlined here on purpose: the old version made a nested
	// CALL and then tested `enable` twice (once per ternary). One test, no extra call.
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	// Print_SetMode would re-derive the draw routine through a switch. We already KNOW which
	// of the two bitmap modes we want, so set it directly and skip the dispatch entirely.
	if (enable)
	{
		PRINT_DATA.FX         = PRINT_FX_OUTLINE;
		PRINT_DATA.SourceMode = PRINT_MODE_BITMAP_TRANS;
		PRINT_DATA.DrawChar   = Print_DrawChar_BitmapTrans;
	}
	else
	{
		PRINT_DATA.FX         = 0;
		PRINT_DATA.SourceMode = PRINT_MODE_BITMAP;
		PRINT_DATA.DrawChar   = Print_DrawChar_Bitmap;
	}
#if (PRINT_USE_G5G7)
	g_Print_Bpp = Print_BppOfMode(VDP_GetMode());   // same cache Print_SetMode would refresh
#endif
#else
	PRINT_DATA.FX = enable ? PRINT_FX_OUTLINE : 0;
	Print_SetMode(enable ? PRINT_MODE_BITMAP_TRANS : PRINT_MODE_BITMAP);
#endif
}

void Print_EnableOutline(bool enable)
{
	PRINT_DATA.FX = enable ? PRINT_FX_OUTLINE : 0;
	Print_SetMode(enable ? PRINT_MODE_BITMAP_TRANS : PRINT_MODE_BITMAP);
}

#if (PRINT_USE_2_PASS_FX)
void Print_DrawTextOutline(const c8* string, u8 color)
{
	static const i8 dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
	static const i8 dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
	u8 i;
	UX x = PRINT_DATA.CursorX;
	UY y = PRINT_DATA.CursorY;
#if (PRINT_COLOR_NUM == 1)
	u8 keep = PRINT_DATA.TextColor;
#else
	u8 keep = PRINT_DATA.TextColor[0];
#endif
	u8 bg = PRINT_DATA.BGColor;
	// Outline pass (8 neighbours)
	Print_SetColor(color, bg);
	for (i = 0; i < 8; ++i)
	{
		Print_SetPosition(x + dx[i], y + dy[i]);
		Print_DrawText(string);
	}
	// Main pass
	Print_SetColor(keep, bg);
	Print_SetPosition(x, y);
	Print_DrawText(string);
}
#endif
#endif // (PRINT_USE_FX_OUTLINE)

//=============================================================================
// GRAPH
//=============================================================================

#if (PRINT_USE_GRAPH)
void Print_DrawLineH(UX x, UY y, u8 len)
{
	Print_SetPosition(x, y);
	Print_DrawCharX(0x17, len);
}

void Print_DrawLineV(UX x, UY y, u8 len)
{
	u8 i;
	for (i = 0; i < len; ++i)
	{
		Print_SetPosition(x, y + i);
		Print_DrawChar(0x16);
	}
}

void Print_DrawBox(UX x, UY y, u8 width, u8 height)
{
	// Draw corners
	Print_SetPosition(x, y);
	Print_DrawChar(0x18);
	Print_SetPosition(x + width - 1, y);
	Print_DrawChar(0x19);
	Print_SetPosition(x, y + height - 1);
	Print_DrawChar(0x1A);
	Print_SetPosition(x + width - 1, y + height - 1);
	Print_DrawChar(0x1B);

	// Draw horizontal lines
	Print_DrawLineH(x + 1, y,              width - 2);
	Print_DrawLineH(x + 1, y + height - 1, width - 2);

	// Draw vertical lines
	Print_DrawLineV(x,             y + 1, height - 2);
	Print_DrawLineV(x + width - 1, y + 1, height - 2);
}
#endif // (PRINT_USE_GRAPH)
