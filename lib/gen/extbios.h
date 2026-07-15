// msxmvl — clean-room reimplementation of MSXgl "extbios" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/extbios.h header ONLY. No MSXgl implementation source was read
// (the vendor module is header-only; there is no .c/.asm to copy).
//
// MSX-DOS 2 extension BIOS (EXTBIO) support.
// Reference: https://map.grauw.nl/resources/dos2_environment.php#c5
#ifndef MSXMVL_EXTBIOS_H
#define MSXMVL_EXTBIOS_H

#include "types.h"

//=============================================================================
// DEFINES
//=============================================================================

// Extended BIOS system hooks / flags (system RAM absolute addresses).
#define M_HOKVLD					0xFB20
const u8 __at(M_HOKVLD) g_HOKVLD;   // bit0 set => extended BIOS installed
#define M_EXTBIO					0xFFCA
const u8 __at(M_EXTBIO) g_EXTBIO;   // EXTBIO hook entry (first byte)

// Extended BIOS device/manufacturer identifiers (register D on EXTBIO call).
#define EXTBIOS_BROADCAST			0x00 // Broad-cast
#define EXTBIOS_MEMMAPPER			0x04 // Memory Mapper (MSX-DOS 2)
#define EXTBIOS_MODEM				0x08 // MSX-Modem & RS-232C
#define EXTBIOS_MSXAUDIO			0x0A // MSX-AUDIO
#define EXTBIOS_MSXJE				0x10 // MSX-JE
#define EXTBIOS_KANJI				0x11 // Kanji driver
#define EXTBIOS_UNAPI				0x22 // MSX-UNAPI (Unofficial)
#define EXTBIOS_MEMMAN				0x4D // MemMan (Unofficial)
#define EXTBIOS_UDRIVER				0x84 // µ-driver by Yoshikazu Yamamoto (Unofficial)
#define EXTBIOS_MULTIMENTE			0xF1 // MultiMente (Unofficial)
#define EXTBIOS_SYSTEM				0xFF // System

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: ExtBIOS_Check
// Check if the extended BIOS (EXTBIO hook) is installed.
//
// The presence flag lives in HOKVLD (0xFB20): bit 0 is set by MSX-DOS 2 /
// other extensions when the EXTBIO hook has been hooked/validated.
//
// Return:
//   FALSE (0) if not installed, non-zero (bit0) if installed.
inline bool ExtBIOS_Check() { return g_HOKVLD & 0x01; }

#endif // MSXMVL_EXTBIOS_H
