// msxmvl — clean-room reimplementation of MSXgl "pac" module public API.
// Derived from MSXgl engine/src/device/pac.h (interface only) + the config
// option lists in MSXgl's msxgl_config.h templates. No MSXgl implementation
// source (.c/.asm/.lst) was read.
//
// Panasoft PAC / FM-PAC (SW-M001 / SW-M004) SRAM handler: 8 KB of battery-backed
// SRAM in the cartridge's slot at 0x4000-0x5FFD, unlocked by writing 'M' to
// 0x5FFE and 'i' to 0x5FFF (anything else relocks). MSXgl carves it into
// PAC_PAGE_MAX pages of 1 KB; page 7 keeps only 1022 usable bytes because the
// two lock registers live at its end.
//
// This is a thin layer over lib/ext/sram.c's PAC core: LINK pac.c WITH sram.c
// (default backend). Unlike a raw PAC_Activate discipline, every read/write/
// format/check here unlocks and RELOCKS internally, so the SRAM is never left
// writable by accident; PAC_Activate remains available for MSXgl-style code.
//
// Toolchain: SDCC --sdcccall 1. The MSXgl pac header declares no __PRESERVES()
// contract, so the bodies follow the standard SDCC convention (AF/BC/DE/HL/IX
// caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_PAC_H
#define MSXMVL_PAC_H

#include "types.h"

//=============================================================================
// CONFIG (mirrors MSXgl msxgl_config.h defaults; no #warning spam)
//=============================================================================

// PAC_ACCESS values (msxgl_config.h option list)
#define PAC_ACCESS_DIRECT		0	// Direct access to SRAM (must be selected in page 1)
#define PAC_ACCESS_BIOS			1	// Access through BIOS routines
#define PAC_ACCESS_SWITCH_BIOS	2	// Access through BIOS routines with BIOS switched in
#define PAC_ACCESS_SYSTEM		3	// Access through library routine (no need BIOS)

#ifndef PAC_ACCESS
#define PAC_ACCESS				PAC_ACCESS_BIOS
#endif
#if (PAC_ACCESS != PAC_ACCESS_BIOS)
#error "msxmvl pac implements PAC_ACCESS_BIOS only (per-byte RDSLT/WRSLT interslot access)"
#endif

// Signed pages need the application signature: APPSIGN is a 4-character string
// (e.g. -DAPPSIGN='"MVZ8"'). Without it the signature option is silently off,
// like MSXgl's fallback (minus the warning).
#if (defined(APPSIGN))
	#ifndef PAC_USE_SIGNATURE
	#define PAC_USE_SIGNATURE	TRUE
	#endif
#else
	#undef  PAC_USE_SIGNATURE
	#define PAC_USE_SIGNATURE	FALSE
#endif

// Validate page/device parameters at runtime (out-of-range page -> no-op /
// PAC_CHECK_ERROR instead of scribbling outside the SRAM).
#ifndef PAC_USE_VALIDATOR
#define PAC_USE_VALIDATOR		TRUE
#endif

// How many PAC cartridges PAC_Initialize records (a machine can hold several).
#ifndef PAC_DEVICE_MAX
#define PAC_DEVICE_MAX			4
#endif

//=============================================================================
// DEFINES
//=============================================================================

#define PAC_PAGE_MAX			8		// 8 x 1 KB pages
#define PAC_EMPTY_CHAR			0xFF	// a formatted page is full of these

// PAC check result
enum PAC_CHECK
{
	PAC_CHECK_EMPTY = 0,			// Page is empty (full of 'empty' character)
	PAC_CHECK_UNDEF,				// Page content is undefined (nor empty, nor signed content)
	PAC_CHECK_ERROR,				// Check failed
#if (PAC_USE_SIGNATURE)
	PAC_CHECK_APP,					// Page contains signed data
#endif
	//.................................
	PAC_CHECK_MAX,
};

// Number of detected PAC cartridges
extern u8 g_PAC_Num;

// Detected PACs' slot ID
extern u8 g_PAC_Slot[PAC_DEVICE_MAX];

// Selected PAC's slot ID
extern u8 g_PAC_Current;

//=============================================================================
// FUNCTIONS
//=============================================================================

// Scan every (sub)slot for PAC-compatible SRAM; record up to PAC_DEVICE_MAX
// and select the first. FALSE if no PAC-compatible SRAM was found.
bool PAC_Initialize();

// Number of detected PAC devices (PAC_Initialize must be called first).
inline u8 PAC_GetNumber() { return g_PAC_Num; }

// A given PAC device's slot ID.
inline u8 PAC_GetSlot(u8 dev) { return g_PAC_Slot[dev]; }

// The default (first) PAC device's slot ID.
inline u8 PAC_GetDefaultSlot() { return PAC_GetSlot(0); }

// Select a given PAC device (index into the detected list).
inline void PAC_Select(u8 dev) { g_PAC_Current = g_PAC_Slot[dev]; }

// Activate (unlock) or deactivate (relock) the current PAC device.
void PAC_Activate(bool bEnable);

// Write data to the current PAC device in the given page (0-7). With
// PAC_USE_SIGNATURE the 4-byte APPSIGN is written at the page start and the
// data lands after it. size is clamped to the page.
void PAC_Write(u8 page, const u8* data, u16 size);

// Read data back from the given page (0-7); skips the signature bytes when
// PAC_USE_SIGNATURE is set, mirroring PAC_Write.
void PAC_Read(u8 page, u8* data, u16 size);

// Format the given page (0-7): fill it with PAC_EMPTY_CHAR, erasing any
// signature. Never format a page you didn't sign -- a PAC is shared between
// games by design.
void PAC_Format(u8 page);

// Format all pages in the current PAC device.
inline void PAC_FormatAll() { for (u8 i = 0; i < PAC_PAGE_MAX; ++i) PAC_Format(i); }

// State of the given page: PAC_CHECK_APP (signed by this app), PAC_CHECK_EMPTY,
// PAC_CHECK_UNDEF (someone else's data), or PAC_CHECK_ERROR. See <PAC_CHECK>.
u8 PAC_Check(u8 page);

#endif // MSXMVL_PAC_H
