// msxmvl clean-room reimplementation of MSXgl "tile" module.
//
// Only the non-inline public functions live here; every other public entry
// point is an inline in tile.h. The tile layer is a thin wrapper over the VDP
// command engine, so these functions carry no __PRESERVES contract of their own
// (the vendor header declares them as plain C functions) and are written in
// portable C -- SDCC honours the ABI for the calls into the VDP module.

#include "tile.h"
#include "vdp.h"		// g_VDP_Command / VDP_CommandSetupR32 (Tile_DrawMapChunk)

//=============================================================================
// GLOBALS
//=============================================================================

// Current pattern bank
u8  g_Tile_CurBank  = 0;
// Page number where pattern are located
u8  g_Tile_BankPage = 2;
// Base bank Y coordinate
u16 g_Tile_BankBase = 0;
// Page number to draw at
u8  g_Tile_DrawPage = 0;

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Function: Tile_LoadBankEx
// Load 'num' tiles of pattern data to VRAM, starting at 'offset' tiles into the
// given bank of the current bank page. The destination is the linear VRAM
// address exposed by Tile_GetBankAddressEx(); because a bank spans the full
// screen width, that linear range is contiguous in VRAM, so a single VRAM write
// of num*TILE_CELL_BYTES bytes places the tiles correctly.
void Tile_LoadBankEx(u8 bank, const u8* data, u16 offset, u16 num)
{
	u32 addr = g_Tile_BankPage * TILE_PAGE_SIZE;
	addr += ((bank * 256) + offset) * TILE_CELL_BYTES;

	VDP_WriteVRAM_128K(data, (u16)addr, addr >> 16, num * TILE_CELL_BYTES);
}

//-----------------------------------------------------------------------------
// Function: Tile_DrawMapChunk
// Draw a width x height rectangle of tiles read row-major from 'map' at cell
// coordinate (x, y). Cells whose index equals TILE_SKIP_INDEX are left
// untouched when TILE_USE_SKIP is enabled (transparent tilemap entries).
void Tile_DrawMapChunk(u8 dx, u8 dy, const u8* map, u8 width, u8 height)
{
	g_VDP_Command.NX = TILE_WIDTH;
	g_VDP_Command.NY = TILE_HEIGHT;
	g_VDP_Command.ARG = 0;
	g_VDP_Command.CMD = (u8)(VDP_CMD_LMMM + VDP_OP_TIMP);

	u8 x = dx;
	u8 y = dy;
	g_VDP_Command.DY = g_Tile_DrawPage * 256 + y * TILE_HEIGHT;
	for (u16 j = 0; j < height; ++j)
	{
		x = dx;
		g_VDP_Command.DX = dx * TILE_WIDTH;
		for (u16 i = 0; i < width; ++i)
		{
			#if (TILE_USE_SKIP)
			if (*map != TILE_SKIP_INDEX) // only draw file not the transparency
			#endif
			{
				g_VDP_Command.SX = (*map % TILE_PER_ROW) * TILE_WIDTH;
				g_VDP_Command.SY = g_Tile_BankBase + (*map / TILE_PER_ROW) * TILE_HEIGHT;
				VDP_CommandSetupR32();
			}

			map++;
			x++;
			g_VDP_Command.DX += TILE_WIDTH;
		}
		y++;
		g_VDP_Command.DY += TILE_HEIGHT;
	}
}

//-----------------------------------------------------------------------------
// Function: Tile_DrawScreen
// Draw a full screen of tiles from a tilemap sized TILE_PER_ROW x
// TILE_PER_COLUMN, anchored at the top-left cell.
void Tile_DrawScreen(const u8* map)
{
	Tile_DrawMapChunk(0, 0, map, TILE_PER_ROW, TILE_PER_COLUMN);
}
