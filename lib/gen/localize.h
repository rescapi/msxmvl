// msxmvl clean-room reimplementation of MSXgl "localize" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
// Text localization: selects a language table and retrieves strings by index.
#ifndef MSXMVL_LOCALIZE_H
#define MSXMVL_LOCALIZE_H

#include "types.h"

//=============================================================================
// DEFINES
//=============================================================================

// Number of localized text entries (per language).
extern u8 g_Loc_TextNum;

// Root address of the localization data (first language's pointer table).
extern u16 g_Loc_DataRoot;

// Pointer to the selected language's pointer table.
extern const c8** g_Loc_CurLang;

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Loc_Initialize
// Initialize the localization module.
//
// Parameters:
//   data - Pointer to generated localization data.
//   num  - Maximum number of entries for a language (should be TEXT_MAX).
void Loc_Initialize(const void* data, u8 num);

// Function: Loc_SetLanguage
// Set the current language.
//
// Parameters:
//   lang - Index of the language to select (must be between 0 and LANG_MAX-1).
inline void Loc_SetLanguage(u8 lang) { g_Loc_CurLang = (const c8**)(g_Loc_DataRoot + g_Loc_TextNum * 2 * lang); }

// Function: Loc_GetText
// Get a given text in the current language.
//
// Parameters:
//   text - Index of the text to retrieve in the current language (0..TEXT_MAX-1).
//
// Return:
//   Pointer to the zero terminated string.
inline const c8* Loc_GetText(u16 text) { return g_Loc_CurLang[text]; }

#endif // MSXMVL_LOCALIZE_H
