// msxmvl clean-room reimplementation of MSXgl "scroll" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
//
// Interface + documented behaviour taken from MSXgl engine/src/scroll.h and the
// documented config macros (msxgl_config.h) ONLY. No MSXgl implementation body
// (.c/.asm/.lst) was read.
#ifndef MSXMVL_SCROLL_H
#define MSXMVL_SCROLL_H

#include "types.h"

//=============================================================================
// CONFIGURATION (documented MSXgl scroll macros; overridable by a project
// config before including this header). Defaults mirror the MSX2 sample so
// that every public function of the module is compiled.
//=============================================================================

#ifndef SCROLL_HORIZONTAL
#define SCROLL_HORIZONTAL		TRUE	// Activate horizontal scrolling
#endif
#ifndef SCROLL_VERTICAL
#define SCROLL_VERTICAL			TRUE	// Activate vertical scrolling
#endif

#ifndef SCROLL_SRC_X
#define SCROLL_SRC_X			64		// Start X coordinate of the source data
#endif
#ifndef SCROLL_SRC_Y
#define SCROLL_SRC_Y			0		// Start Y coordinate of the source data
#endif
#ifndef SCROLL_SRC_W
#define SCROLL_SRC_W			128		// Width of the source data (tiles)
#endif
#ifndef SCROLL_SRC_H
#define SCROLL_SRC_H			24		// Height of the source data (tiles)
#endif

#ifndef SCROLL_DST_X
#define SCROLL_DST_X			0		// Destination x coordinate (in layout table)
#endif
#ifndef SCROLL_DST_Y
#define SCROLL_DST_Y			2		// Destination y coordinate (in layout table)
#endif
#ifndef SCROLL_DST_W
#define SCROLL_DST_W			32		// Destination width (tiles)
#endif
#ifndef SCROLL_DST_H
#define SCROLL_DST_H			20		// Destination height (tiles)
#endif
#ifndef SCROLL_SCREEN_W
#define SCROLL_SCREEN_W			32		// Screen width in tile number
#endif

#ifndef SCROLL_WRAP
#define SCROLL_WRAP				TRUE	// Wrap the source map at its borders
#endif

#ifndef SCROLL_ADJUST
#define SCROLL_ADJUST			TRUE	// Global (sub-tile) adjustement via R#18/R#23
#endif
#ifndef SCROLL_ADJUST_SPLIT
#define SCROLL_ADJUST_SPLIT		TRUE	// Destination window adjustement using screen split
#endif

#ifndef SCROLL_MASK
#define SCROLL_MASK				TRUE	// Use sprites to mask the scrolling seam
#endif
#ifndef SCROLL_MASK_ID
#define SCROLL_MASK_ID			0		// First sprite ID to use for the mask
#endif
#ifndef SCROLL_MASK_COLOR
#define SCROLL_MASK_COLOR		1		// Must be the same than the border color
#endif
#ifndef SCROLL_MASK_PATTERN
#define SCROLL_MASK_PATTERN		0		// Sprite pattern to use for the mask
#endif

// VDP layout (name) table base address in VRAM. Screen 1/2 default = 0x1800.
#ifndef SCROLL_NT_ADDR
#define SCROLL_NT_ADDR			0x1800
#endif
// VDP sprite attribute table base address in VRAM (screen 1/2 default = 0x1B00).
#ifndef SCROLL_ATTR_ADDR
#define SCROLL_ATTR_ADDR		0x1B00
#endif

//=============================================================================
// DEFINES
//=============================================================================

// Address of the source map data
extern u16 g_Scroll_Map;

#if (SCROLL_HORIZONTAL)
// Horizontal offset (in pixel)
extern u16 g_Scroll_OffsetX;
// Horizontal offset (in tiles)
extern u8 g_Scroll_TileX;
#endif

#if (SCROLL_VERTICAL)
// Vertical offset (in pixel)
extern u16 g_Scroll_OffsetY;
// Vertical offset (in tiles)
extern u8 g_Scroll_TileY;
#endif

#if (SCROLL_ADJUST)
extern u8 g_Scroll_Adjust;
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Scroll_Initialize
// Initialize scrolling module.
//   map - Address of the tilemap used to render the scrolling background.
// Return: first free sprite index (module reserves sprites if mask is activated).
u8 Scroll_Initialize(u16 map);

#if (SCROLL_HORIZONTAL)
// Function: Scroll_SetOffsetH
// Set scrolling horizontal offset (in pixels, signed step).
void Scroll_SetOffsetH(i8 offset);
#endif

#if (SCROLL_VERTICAL)
// Function: Scroll_SetOffsetV
// Set scrolling vertical offset (in pixels, signed step).
void Scroll_SetOffsetV(i8 offset);
#endif

#if ((SCROLL_ADJUST) && (SCROLL_ADJUST_SPLIT))
// Function: Scroll_HBlankAdjust
// Adjust screen offset. To be called in the H-Blank handler.
//   adjust - 0: Frame start, 1: Scrolling adjust, 2: No more adjust
void Scroll_HBlankAdjust(u8 adjust);
#endif

// Function: Scroll_Update
// Update scrolling module (render exposed tiles + apply fine adjust).
void Scroll_Update();

#endif // MSXMVL_SCROLL_H
