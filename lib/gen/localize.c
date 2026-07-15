// msxmvl clean-room reimplementation of MSXgl "localize" module.
// Text localization: selects a language table and retrieves strings by index.
#include "localize.h"

//=============================================================================
// GLOBALS
//=============================================================================

u8          g_Loc_TextNum  = 0;			// Entries per language table
u16         g_Loc_DataRoot = 0;			// Address of the localization data root
const c8**  g_Loc_CurLang  = NULL;		// Currently selected language table

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Function: Loc_Initialize
// Store the data root and entry count, then default to language 0 (the table
// located at the data root).
void Loc_Initialize(const void* data, u8 num)
{
	g_Loc_DataRoot = (u16)data;
	g_Loc_TextNum  = num;
	Loc_SetLanguage(0);	// == g_Loc_CurLang = data; matches MSXgl exactly (byte-identical)
}
