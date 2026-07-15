// msxmvl clean-room reimplementation of MSXgl "tile" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
//
// Tile support for bitmap mode (screen 5-8 style, V9938+ VDP commands).
// The pattern "bank" is a full-screen-width 2D image region in VRAM; tiles are
// blitted out of it with LMMM. Bank addressing / geometry is defined entirely by
// the (config-driven) macros below, exactly as the vendor header does.
#ifndef MSXMVL_TILE_H
#define MSXMVL_TILE_H

#include "types.h"

//=============================================================================
// VDP DEPENDENCIES
//=============================================================================
// The tile module is a thin layer over the VDP command engine + VRAM writer.
// When this header is used inside a full MSXgl / msxmvl build the real vdp.h is
// present and these declarations are skipped. Standalone (module compile-check
// and difftest driver) they make the module self-sufficient. Signatures match
// MSXgl's vdp.h / vdp_inl.h exactly so behaviour is identical in both builds.

#ifndef VDP_OP_IMP
#define VDP_OP_IMP		0x00	// Copy source to destination (IMP)
#endif
#ifndef VDP_OP_TIMP
#define VDP_OP_TIMP		0x08	// Copy source to destination, skip transparent (TIMP)
#endif

#ifndef MSXMVL_TILE_VDP_DECLS
#define MSXMVL_TILE_VDP_DECLS
// Logical move VRAM to VRAM.
void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op);
// Logical move VDP to VRAM (fill).
void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op);
// Write data from RAM to VRAM (17-bit / 128KB addressing).
void VDP_WriteVRAM_128K(const u8* src, u16 destLow, u8 destHigh, u16 count);
#ifndef VDP_WriteVRAM
#define VDP_WriteVRAM	VDP_WriteVRAM_128K
#endif
#endif // MSXMVL_TILE_VDP_DECLS

//=============================================================================
// VALIDATION (config defaults, identical to vendor header)
//=============================================================================

#ifndef TILE_WIDTH
	#define TILE_WIDTH				8
#endif
#ifndef TILE_HEIGHT
	#define TILE_HEIGHT				8
#endif
#ifndef TILE_BPP
	#define TILE_BPP				4
#endif
#ifndef TILE_SCREEN_WIDTH
	#define TILE_SCREEN_WIDTH		256
#endif
#ifndef TILE_SCREEN_HEIGHT
	#define TILE_SCREEN_HEIGHT		212
#endif
#ifndef TILE_USE_SKIP
	#define TILE_USE_SKIP			TRUE
#endif
#ifndef TILE_SKIP_INDEX
	#define TILE_SKIP_INDEX			0
#endif

//=============================================================================
// DEFINES
//=============================================================================

// Precomputed values
#define TILE_PER_ROW				((TILE_SCREEN_WIDTH) / (TILE_WIDTH))
#define TILE_PER_COLUMN				((TILE_SCREEN_HEIGHT) / (TILE_HEIGHT))
#define TILE_CELL_BYTES				((TILE_WIDTH) * (TILE_HEIGHT) * (TILE_BPP) / 8)
#define TILE_PAGE_SIZE				(u32)((u32)256 * 256 * (TILE_BPP) / 8)
#define TILE_BANK_WIDTH				TILE_SCREEN_WIDTH
#define TILE_BANK_HEIGHT			(u16)((u32)(TILE_WIDTH) * (TILE_HEIGHT) * 256 / (TILE_SCREEN_WIDTH))

// Current pattern bank
extern u8  g_Tile_CurBank;
// Page number where pattern are located
extern u8  g_Tile_BankPage;
// Base bank Y coordinate
extern u16 g_Tile_BankBase;
// Page number to draw at
extern u8  g_Tile_DrawPage;

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Group: Bank

// Set page number where pattern are located in VRAM. Must be called before load data there.
inline void Tile_SetBankPage(u8 page) { g_Tile_BankPage = page; g_Tile_BankBase = g_Tile_BankPage * 256 + g_Tile_CurBank * TILE_BANK_HEIGHT; }

// Load data to the given bank (in the selected page ; see <Tile_SetBankPage>).
void Tile_LoadBankEx(u8 bank, const u8* data, u16 offset, u16 num);

// Get the VRAM address at a given offset in a given bank.
inline u32 Tile_GetBankAddressEx(u8 bank, u16 offset) { return (g_Tile_BankPage * TILE_PAGE_SIZE) + ((bank * 256) + offset) * TILE_CELL_BYTES; }

// Get the VRAM start address of a given bank.
inline u32 Tile_GetBankAddress(u8 bank) { return Tile_GetBankAddressEx(bank, 0); }

// Load data to the given bank (in the selected page ; see <Tile_SetBankPage>).
inline void Tile_LoadBank(u8 bank, const u8* data, u16 num) { Tile_LoadBankEx(bank, data, 0, num); }

// Fill the whole bank with the given color.
inline void Tile_FillBank(u8 bank, u8 value) { VDP_CommandLMMV(0, g_Tile_BankPage * 256 + bank * TILE_BANK_HEIGHT, TILE_BANK_WIDTH, TILE_BANK_HEIGHT, value, VDP_OP_IMP); }

//-----------------------------------------------------------------------------
// Group: Draw

// Set current pattern bank. Must be called at least once before using drawing functions.
inline void Tile_SelectBank(u8 bank) { g_Tile_CurBank = bank; g_Tile_BankBase = g_Tile_BankPage * 256 + g_Tile_CurBank * TILE_BANK_HEIGHT; }

// Set page number where to draw in VRAM. Must be called at least once before using drawing functions.
inline void Tile_SetDrawPage(u8 page) { g_Tile_DrawPage = page; }

// Draw a tile at the given coordinate.
inline void Tile_DrawTile(u8 x, u8 y, u8 tile) { VDP_CommandLMMM((tile % TILE_PER_ROW) * TILE_WIDTH, g_Tile_BankBase + (tile / TILE_PER_ROW) * TILE_HEIGHT, x * TILE_WIDTH, g_Tile_DrawPage * 256 + y * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT, VDP_OP_TIMP); }

// Fill a cell at the given position with the given color.
inline void Tile_FillTile(u8 x, u8 y, u8 color) { VDP_CommandLMMV(x * TILE_WIDTH, g_Tile_DrawPage * 256 + y * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT, color, VDP_OP_IMP); }

// Drawn a chunk of tilemap at a given coordinate.
void Tile_DrawMapChunk(u8 x, u8 y, const u8* map, u8 width, u8 height);

// Drawn a block of tilemap at a given coordinate.
inline void Tile_DrawBlock(u8 dx, u8 dy, u8 sx, u8 sy, u8 width, u8 height) { VDP_CommandLMMM(sx * TILE_WIDTH, g_Tile_BankBase + sy * TILE_HEIGHT, dx * TILE_WIDTH, g_Tile_DrawPage * 256 + dy * TILE_HEIGHT, width * TILE_WIDTH, height * TILE_HEIGHT, VDP_OP_TIMP); }

// Draw a whole screen with using a tilemap.
void Tile_DrawScreen(const u8* map);

// Fill the screen with the given color.
inline void Tile_FillScreen(u8 color) { VDP_CommandLMMV(0, g_Tile_DrawPage * 256, TILE_SCREEN_WIDTH, TILE_SCREEN_HEIGHT, color, VDP_OP_IMP); }

#endif // MSXMVL_TILE_H
