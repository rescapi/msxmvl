// msxmvl — clean-room reimplementation of MSXgl "msx-music" module public API.
// Interface (names, signatures, documented behaviour, register/port map) taken
// from MSXgl's engine/src/msx-music.h + msx-music_reg.h + system_port.h headers
// ONLY. No MSXgl implementation source (.c/.asm/.lst) was read.
//
// MSX-Music (YM2413 / OPLL) handler module.
// The OPLL is a WRITE-ONLY device accessed through two I/O ports:
//   0x7C (register-index) and 0x7D (register-data).
// Because the chip cannot be read back, this module keeps a RAM shadow of every
// register written; MSXMusic_GetRegister returns that shadow and
// MSXMusic_Resume re-plays it to the chip after a MSXMusic_Mute.
//
// Detection scans every slot (including expanded sub-slots) for the ROM
// signature at 0x4018 via the RDSLT (0x000C) BIOS routine:
//   "APRL" -> internal MSX-Music,  "PAC2" -> external FM-PAC.
//
// Toolchain: SDCC --sdcccall 1. The MSXgl msx-music header declares no
// __PRESERVES() contract, so the bodies follow the standard SDCC calling
// convention (AF/BC/DE/HL/IX caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_MSX_MUSIC_H
#define MSXMVL_MSX_MUSIC_H

#include "types.h"

//=============================================================================
// CONFIG (defaults mirror MSXgl msxgl_config.h)
//=============================================================================
#ifndef MSXMUSIC_USE_RESUME
#define MSXMUSIC_USE_RESUME		TRUE
#endif

//=============================================================================
// MSX-MUSIC I/O PORTS (from system_port.h)
//=============================================================================
#define P_MSXMUSIC_INDEX		0x7C   // Register index (write-only)
__sfr __at(P_MSXMUSIC_INDEX)	g_MSXMusic_IndexPort;
#define P_MSXMUSIC_DATA			0x7D   // Register data (write-only)
__sfr __at(P_MSXMUSIC_DATA)		g_MSXMusic_DataPort;

//=============================================================================
// DEFINES
//=============================================================================

// Detection result
enum MSXMUSIC_DETECTION
{
	MSXMUSIC_NOTFOUND = 0, // No MSX-Music found
	MSXMUSIC_INTERNAL = 1, // Internal MSX-Music device found
	MSXMUSIC_EXTERNAL = 2, // External MSX-Music device found (FM-PAC)
};

// Special slot ID value (from system.h)
#ifndef SLOT_NOTFOUND
#define SLOT_NOTFOUND			0xFF
#endif
#ifndef SLOT_EXP
#define SLOT_EXP				(1 << 7)
#endif

// Key register map (from msx-music_reg.h) used by Mute
#define MSXMUSIC_REG_RHYTHM		0x0E
#define MSXMUSIC_REG_INST_1		0x30 // .. 0x38 : instrument / volume (9 channels)

// Highest register maintained in the shadow buffer (inclusive).
#define MSXMUSIC_REG_MAX		0x38

// Slot of the MSX-Music chip
extern u8 g_MSXMusic_SlotId;

//=============================================================================
// PROTOTYPE
//=============================================================================

// Function: MSXMusic_Initialize
// Initialize MSX-Music module.
// Return: MSXMUSIC_NOTFOUND / MSXMUSIC_INTERNAL / MSXMUSIC_EXTERNAL
u8 MSXMusic_Initialize();

// Function: MSXMusic_Detect
// Search for MSX-Music (YM2413) chip.
// Return: MSXMUSIC_NOTFOUND / MSXMUSIC_INTERNAL / MSXMUSIC_EXTERNAL
u8 MSXMusic_Detect();

// Function: MSXMusic_GetSlotId
// Get the Slot ID of the first MSX-Music chip found.
// Return: Slot ID if found or SLOT_NOTFOUND otherwise
inline u8 MSXMusic_GetSlotId() { return g_MSXMusic_SlotId; }

// Function: MSXMusic_SetRegister
// Set MSX-Music register value.
void MSXMusic_SetRegister(u8 reg, u8 value);

// Function: MSXMusic_GetRegister
// Get MSX-Music register value (from RAM shadow).
u8 MSXMusic_GetRegister(u8 reg);

// Function: MSXMusic_Mute
// Mute MSX-Music sound.
void MSXMusic_Mute();

#if (MSXMUSIC_USE_RESUME)
// Function: MSXMusic_Resume
// Resume MSX-Music sound (re-play the RAM shadow to the chip).
void MSXMusic_Resume();
#endif

#endif // MSXMVL_MSX_MUSIC_H
