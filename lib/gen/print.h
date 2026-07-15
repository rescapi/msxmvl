// msxmvl clean-room reimplementation of MSXgl "print" module.
// Drop-in: exposes MSXgl's exact public names + signatures so one difftest
// driver can link against real MSXgl OR msxmvl.
//
// The print module renders text through several back-ends selected at run time
// (RAM bitmap, VRAM bitmap, sprite, text/name-table) plus numeric/format
// helpers, box-drawing graph helpers and shadow/outline FX. This header is a
// self-contained port of the vendor CONTRACT (enums, struct, macros, inline
// helpers and non-inline prototypes); the implementation lives in print.c and
// is written from the documented behaviour only.
#ifndef MSXMVL_PRINT_H
#define MSXMVL_PRINT_H

#include "types.h"

//=============================================================================
// COMPILE-TIME CONFIGURATION (defaults, identical spirit to msxgl_config.h)
//=============================================================================
// Fixed, self-consistent configuration for this build. Variable char size
// (WIDTH_X / HEIGHT_X) is used so both text (unit 1) and bitmap (unit 8) fonts
// coexist, matching the MSX2 sample configuration.

#ifndef PRINT_USE_MULTIFONT
	#define PRINT_USE_MULTIFONT		FALSE
#endif
#ifndef PRINT_USE_BITMAP
	#define PRINT_USE_BITMAP		TRUE
#endif
#ifndef PRINT_USE_VRAM
	#define PRINT_USE_VRAM			TRUE
#endif
// Bitmap text in GRAPHIC5 (2bpp) and GRAPHIC7 (8bpp). GRAPHIC4/6 (4bpp) always work; set
// FALSE to drop the 2bpp/8bpp packing paths if the project only ever prints in G4/G6.
#ifndef PRINT_USE_G5G7
	#define PRINT_USE_G5G7			TRUE
#endif
// When TRUE, Print_DrawChar auto-wraps to a new line at the right screen edge (and, in
// MSX2 bitmap modes, waits for the VDP command engine first). Matches MSXgl's
// PRINT_USE_VALIDATOR; MSXgl's engine default is FALSE, so msxmvl defaults FALSE too.
#ifndef PRINT_USE_VALIDATOR
	#define PRINT_USE_VALIDATOR		FALSE
#endif
#ifndef PRINT_USE_SPRITE
	#define PRINT_USE_SPRITE		TRUE
#endif
#ifndef PRINT_USE_TEXT
	#define PRINT_USE_TEXT			TRUE
#endif
#ifndef PRINT_USE_FX_SHADOW
	#define PRINT_USE_FX_SHADOW		TRUE
#endif
#ifndef PRINT_USE_FX_OUTLINE
	#define PRINT_USE_FX_OUTLINE	TRUE
#endif
#ifndef PRINT_USE_2_PASS_FX
	#define PRINT_USE_2_PASS_FX		TRUE
#endif
#ifndef PRINT_USE_GRAPH
	#define PRINT_USE_GRAPH			TRUE
#endif
#ifndef PRINT_USE_FORMAT
	#define PRINT_USE_FORMAT		TRUE
#endif
#ifndef PRINT_USE_32B
	#define PRINT_USE_32B			TRUE
#endif
#ifndef PRINT_COLOR_NUM
	#define PRINT_COLOR_NUM			1
#endif
#ifndef PRINT_SKIP_SPACE
	#define PRINT_SKIP_SPACE		TRUE
#endif

// Character width / height option ids
#define PRINT_WIDTH_1				1
#define PRINT_WIDTH_6				6
#define PRINT_WIDTH_8				8
#define PRINT_WIDTH_X				0
#define PRINT_HEIGHT_1				1
#define PRINT_HEIGHT_8				8
#define PRINT_HEIGHT_X				0

#ifndef PRINT_WIDTH
	#define PRINT_WIDTH				PRINT_WIDTH_X
#endif
#ifndef PRINT_HEIGHT
	#define PRINT_HEIGHT			PRINT_HEIGHT_X
#endif

//=============================================================================
// VDP / STRING DEPENDENCIES (self-contained; skipped in a full build)
//=============================================================================
// Signatures match MSXgl's vdp.h / string.h exactly so behaviour is identical
// whether the driver links against real MSXgl or msxmvl.

#ifndef VDP_OP_IMP
#define VDP_OP_IMP		0x00	// Copy source to destination (IMP)
#endif
#ifndef VDP_OP_TIMP
#define VDP_OP_TIMP		0x08	// Copy source to destination, skip transparent (TIMP)
#endif

#ifndef MSXMVL_PRINT_VDP_DECLS
#define MSXMVL_PRINT_VDP_DECLS
// Table base addresses (low 16 bits) published by the VDP module.
extern u16 g_ScreenLayoutLow;		// Pattern Layout (Name) Table
extern u16 g_ScreenColorLow;		// Color Table
extern u16 g_ScreenPatternLow;		// Pattern Generator Table
extern u16 g_SpriteAttributeLow;	// Sprite Attribute Table
extern u16 g_SpritePatternLow;		// Sprite Pattern Table

void VDP_Poke_16K(u8 val, u16 dest);
u8   VDP_Peek_16K(u16 src);
void VDP_FillVRAM_16K(u8 value, u16 dest, u16 count);
void VDP_WriteVRAM_16K(const u8* src, u16 dest, u16 count);
void VDP_CommandHMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny);
void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op);
void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op);
void VDP_CommandHMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col);
void VDP_CommandWait(void);
#ifndef VDP_Poke
#define VDP_Poke		VDP_Poke_16K
#endif
#ifndef VDP_Peek
#define VDP_Peek		VDP_Peek_16K
#endif
#ifndef VDP_FillVRAM
#define VDP_FillVRAM	VDP_FillVRAM_16K
#endif
#ifndef VDP_WriteVRAM
#define VDP_WriteVRAM	VDP_WriteVRAM_16K
#endif
#endif // MSXMVL_PRINT_VDP_DECLS

#ifndef MSXMVL_PRINT_STRING_DECLS
#define MSXMVL_PRINT_STRING_DECLS
// Compute the length of a null-terminated string (max 255).
inline u8 String_Length(const c8* str)
{
	u8 len = 0;
	while (*str++)
		++len;
	return len;
}
#endif // MSXMVL_PRINT_STRING_DECLS

//=============================================================================
// DEFINES
//=============================================================================

#define PRINT_USE_ALIGN			TRUE
#define PRINT_USE_DIRECTION		TRUE

// Character display sources
enum PRINT_MODE
{
#if ((PRINT_USE_BITMAP) || (PRINT_USE_VRAM))
	PRINT_MODE_BITMAP		     = 0,
	PRINT_MODE_BITMAP_TRANS      = 1,
#endif
#if (PRINT_USE_VRAM)
	PRINT_MODE_BITMAP_VRAM	     = 2,
	PRINT_MODE_BITMAP_VRAM_TRANS = 3,
#endif
#if (PRINT_USE_SPRITE)
	PRINT_MODE_SPRITE		     = 4,
#endif
#if (PRINT_USE_TEXT)
	PRINT_MODE_TEXT			     = 5,
#endif
	PRINT_MODE_MAX,
};

// Handle fixed or variable character width
#if (PRINT_WIDTH == PRINT_WIDTH_1)
	#define PRINT_W(a) 1
	#define PRINT_MOD256
	#define PRINT_MOD512
#elif (PRINT_WIDTH == PRINT_WIDTH_6)
	#define PRINT_W(a) 6
	#define PRINT_MOD256			42
	#define PRINT_MOD512			85
#elif (PRINT_WIDTH == PRINT_WIDTH_8)
	#define PRINT_W(a) 8
	#define PRINT_MOD256			32
	#define PRINT_MOD512			64
#else // (PRINT_WIDTH == PRINT_WIDTH_X)
	#define PRINT_W(a) (a)
	#define PRINT_MOD256			PRINT_DATA.CharPerLine
	#define PRINT_MOD512			PRINT_DATA.CharPerLine
#endif

// Handle fixed or variable character height
#if (PRINT_HEIGHT == PRINT_HEIGHT_1)
	#define PRINT_H(a) 1
#elif (PRINT_HEIGHT == PRINT_HEIGHT_8)
	#define PRINT_H(a) 8
#else // (PRINT_HEIGHT == PRINT_HEIGHT_X)
	#define PRINT_H(a) a
#endif

#define PRINT_TAB_SIZE				24
#define PRINT_DEFAULT_FONT			NULL

// Functions
typedef void (*print_drawchar)(u8);
typedef void (*print_loadfont)(VADDR);

// Print VFX flags
#define PRINT_FX_SHADOW				0b00000001
#define PRINT_FX_OUTLINE			0b00000010
#define PRINT_FX_ONLY				0b10000000

// Print direction enum
enum PRINT_DRI
{
	PRINT_DIR_RIGHT = 0,
	PRINT_DIR_LEFT,
};

// Print alignment enum
enum PRINT_ALIGN
{
	PRINT_ALIGN_LEFT = 0,
	PRINT_ALIGN_CENTER,
	PRINT_ALIGN_RIGHT,
};

//-----------------------------------------------------------------------------
// STRUCTURES
//-----------------------------------------------------------------------------

typedef struct Print_Data
{
	u8 PatternX;
	u8 PatternY;
	u8 UnitX;
	u8 UnitY;
	u8 TabSize;
	UX CursorX;
	UY CursorY;
#if (PRINT_COLOR_NUM == 1)
	u8 TextColor;
#else
	u8 TextColor[PRINT_COLOR_NUM];
#endif
	u8 BGColor;
	u8 CharFirst;
	u8 CharLast;
	u8 CharCount;
	print_drawchar DrawChar;
	u8 SourceMode       : 4;
	u16 ScreenWidth;
	const u8* FontPatterns;
	const u8* FontAddr;
#if (PRINT_USE_VRAM)
	UY FontVRAMY;
	u8 CharPerLine;
#endif
#if (PRINT_USE_SPRITE)
	u8 SpritePattern;
	u8 SpriteID;
#endif
#if (PRINT_USE_TEXT)
	u8 PatternOffset;
#endif
#if ((PRINT_USE_FX_SHADOW) || (PRINT_USE_FX_OUTLINE))
	u8 FX;
#endif
#if (PRINT_USE_FX_SHADOW)
	u8 ShadowOffsetX	: 3;
	u8 ShadowOffsetY	: 3;
	u8 ShadowColor;
#endif
#if (PRINT_USE_FX_OUTLINE)
	u8 OutlineColor;
#endif
	u8 Buffer[16];
#if (PRINT_USE_DIRECTION)
	i8 Direction;
#endif
} Print_Data;

#if (PRINT_USE_MULTIFONT)
extern Print_Data* g_PrintData;
#define PRINT_DATA (*g_PrintData)
#else
extern Print_Data g_PrintData;
#define PRINT_DATA (g_PrintData)
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Group: Initialization

#if (PRINT_USE_MULTIFONT)
inline void Print_SetFontData(Print_Data* data) { g_PrintData = data; }
#endif

// Initialize print module. Must be called after VDP_SetMode().
bool Print_Initialize();

// Change current print mode.
void Print_SetMode(u8 mode);

// Set the current font (and set mode to RAM).
void Print_SetFont(const u8* font);

// Set the current font from explicit parameters.
inline void Print_SetFontEx(u8 patternX, u8 patternY, u8 sizeX, u8 sizeY, u8 firstChr, u8 lastChr, const u8* patterns)
{
	PRINT_DATA.PatternX     = patternX;
	PRINT_DATA.PatternY     = patternY;
	PRINT_DATA.UnitX        = sizeX;
	PRINT_DATA.UnitY        = sizeY;
	PRINT_DATA.CharFirst    = firstChr;
	PRINT_DATA.CharLast     = lastChr;
	PRINT_DATA.CharCount    = lastChr - firstChr + 1;
	PRINT_DATA.FontPatterns = patterns;
	PRINT_DATA.FontAddr     = PRINT_DATA.FontPatterns - (firstChr * PRINT_DATA.PatternY);
}

inline const Print_Data* Print_GetFontInfo() { return &PRINT_DATA; }

inline void Print_SetPosition(UX x, UY y)
{
	PRINT_DATA.CursorX = x;
	PRINT_DATA.CursorY = y;
}

inline void Print_SetPositionX(UX x) { PRINT_DATA.CursorX = x; }
inline void Print_SetPositionY(UY y) { PRINT_DATA.CursorY = y; }

inline void Print_SetCharSize(u8 x, u8 y)
{
	PRINT_DATA.UnitX = x;
	PRINT_DATA.UnitY = y;
}

inline void Print_SetTabSize(u8 size) { PRINT_DATA.TabSize = size; }

#if (PRINT_USE_BITMAP)
// Initialize print module and set a font in RAM.
void Print_SetBitmapFont(const u8* font);
#endif

#if (PRINT_USE_VRAM)
// Set the current font and upload data to VRAM.
void Print_SetVRAMFont(const u8* font, UY y, u8 color, bool trans);
#endif

#if (PRINT_USE_TEXT)
// Set a font in the pattern generator table (text mode).
void Print_SetTextFont(const u8* font, u8 offset);

inline void Print_SelectTextFont(const u8* font, u8 offset)
{
	PRINT_DATA.PatternOffset = offset;
	Print_SetFontEx(8, 8, 1, 1, font[2], font[3], font+4);
}

inline u8 Print_GetPatternOffset() { return PRINT_DATA.PatternOffset; }
inline void Print_SetPatternOffset(u8 offset) { PRINT_DATA.PatternOffset = offset; }
#endif

#if (PRINT_USE_SPRITE)
// Set the current font and upload to the Sprite Pattern Table.
void Print_SetSpriteFont(const u8* font, u8 patIdx, u8 sprtIdx);

inline u8 Print_GetSpritePattern() { return PRINT_DATA.SpritePattern; }
inline u8 Print_GetSpriteID() { return PRINT_DATA.SpriteID; }
inline void Print_SetSpriteID(u8 id) { PRINT_DATA.SpriteID = id; }
#endif

//-----------------------------------------------------------------------------
// Group: Draw

void Print_DrawChar(u8 chr);
void Print_DrawCharX(c8 chr, u8 num);
void Print_DrawText(const c8* string);
void Print_DrawHex8(u8 value);
void Print_DrawHex16(u16 value);
void Print_DrawBin8(u8 value);

#if (PRINT_USE_32B)
void Print_DrawHex32(u32 value);
void Print_DrawInt(i32 value);
#else
void Print_DrawInt(i16 value);
#endif

inline void Print_Space() { PRINT_DATA.CursorX += PRINT_W(PRINT_DATA.UnitX); }

inline void Print_Tab()
{
	PRINT_DATA.CursorX += PRINT_W(PRINT_DATA.UnitX) + PRINT_DATA.TabSize - 1;
	PRINT_DATA.CursorX &= ~(PRINT_DATA.TabSize - 1);
}

inline void Print_Return()
{
	PRINT_DATA.CursorX = 0;
	PRINT_DATA.CursorY += PRINT_H(PRINT_DATA.UnitY);
}

#if (PRINT_USE_FORMAT)
void Print_DrawFormat(const c8* format, ...);
#endif

//-----------------------------------------------------------------------------
// Group: Helper

void Print_Clear();
void Print_Backspace(u8 num);

inline void Print_DrawTextAt(UX x, UY y, const c8* str) { Print_SetPosition(x, y); Print_DrawText(str); }

inline void Print_DrawTextAtV(UX x, UY y, const c8* str)
{
	while (*str)
	{
		Print_SetPosition(x, y++);
		Print_DrawChar(*str++);
	}
}

inline void Print_DrawCharAt(UX x, UY y, c8 chr) { Print_SetPosition(x, y); Print_DrawChar(chr); }
inline void Print_DrawCharXAt(UX x, UY y, c8 chr, u8 len) { Print_SetPosition(x, y); Print_DrawCharX(chr, len); }

inline void Print_DrawCharYAt(UX x, UY y, c8 chr, u8 len)
{
	for (u8 i = 0; i < len; ++i)
		Print_DrawCharAt(x, y++, chr);
}

inline void Print_DrawHex8At(UX x, UY y, u8 val) { Print_SetPosition(x, y); Print_DrawHex8(val); }
inline void Print_DrawHex16At(UX x, UY y, u16 val) { Print_SetPosition(x, y); Print_DrawHex16(val); }
inline void Print_DrawBin8At(UX x, UY y, u8 val) { Print_SetPosition(x, y); Print_DrawBin8(val); }
inline void Print_DrawIntAt(UX x, UY y, i16 val) { Print_SetPosition(x, y); Print_DrawInt(val); }

//-----------------------------------------------------------------------------
// Group: FX - Color

void Print_SetColor(u8 text, u8 bg);

#if (PRINT_COLOR_NUM > 1)
void Print_SetColorShade(const u8* shade);
#endif

//-----------------------------------------------------------------------------
// Group: FX - Shadow

#if (PRINT_USE_FX_SHADOW)
void Print_SetShadow(bool enable, i8 offsetX, i8 offsetY, u8 color);
void Print_EnableShadow(bool enable);
#if (PRINT_USE_2_PASS_FX)
void Print_DrawTextShadow(const c8* string, i8 offsetX, i8 offsetY, u8 color);
#endif
#endif

//-----------------------------------------------------------------------------
// Group: FX - Outline

#if (PRINT_USE_FX_OUTLINE)
void Print_SetOutline(bool enable, u8 color);
void Print_EnableOutline(bool enable);
#if (PRINT_USE_2_PASS_FX)
void Print_DrawTextOutline(const c8* string, u8 color);
#endif
#endif

//-----------------------------------------------------------------------------
// Group: Graph

#if (PRINT_USE_GRAPH)
void Print_DrawLineH(UX x, UY y, u8 len);
void Print_DrawLineV(UX x, UY y, u8 len);
void Print_DrawBox(UX x, UY y, u8 width, u8 height);
#endif

//-----------------------------------------------------------------------------
// Group: Align

#if (PRINT_USE_ALIGN)
inline void Print_DrawTextAlign(const c8* str, u8 align)
{
	u8 len = String_Length(str);
	switch (align)
	{
	case PRINT_ALIGN_LEFT:
		break;
	case PRINT_ALIGN_CENTER:
		PRINT_DATA.CursorX -= ((len - 1) / 2) * PRINT_W(PRINT_DATA.UnitX);
		break;
	case PRINT_ALIGN_RIGHT:
		PRINT_DATA.CursorX -= (len - 1) * PRINT_W(PRINT_DATA.UnitX);
		break;
	}
	Print_DrawText(str);
}

inline void Print_DrawTextAlignAt(UX x, UY y, const c8* str, u8 align)
{
	Print_SetPosition(x, y);
	Print_DrawTextAlign(str, align);
}
#endif // (PRINT_USE_ALIGN)

#if (PRINT_USE_DIRECTION)
inline void Print_SetDirection(u8 dir) { PRINT_DATA.Direction = dir; }
#endif

#endif // MSXMVL_PRINT_H
