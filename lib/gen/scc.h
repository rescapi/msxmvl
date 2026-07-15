// msxmvl — clean-room reimplementation of MSXgl "scc" module public API.
// Interface (names, signatures, documented behaviour, register/address map)
// taken from MSXgl's engine/src/scc.h + config_option.h SCC section ONLY.
// No MSXgl implementation source (.c/.asm/.lst) was read.
//
// Konami SCC / SCC-I sound chip handler.
//
// The SCC is a memory-mapped sound chip living inside a cartridge slot. Its
// control registers appear in page 2 at 0x9800-0x9FFF once "sound mode" has
// been enabled by writing 0x3F to the third bank-select register (0x9000) of
// the Konami mapper. Because the chip is in an arbitrary slot, every access
// temporarily selects SCC_SLOT into page 2 (via Sys_GetPageSlot/Sys_SetPageSlot)
// and restores the previous slot afterwards.
//
// Toolchain: SDCC --sdcccall 1. The MSXgl scc header declares no __PRESERVES()
// contract, so the bodies follow the standard SDCC convention (AF/BC/DE/HL/IX
// caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_SCC_H
#define MSXMVL_SCC_H

#include "types.h"

//=============================================================================
// CONFIG (mirrors MSXgl config_option.h + msxgl_config.h defaults)
//=============================================================================

// SCC_SLOT_MODE values
#define SCC_SLOT_DIRECT		0	// Program runs on the SCC cartridge itself
#define SCC_SLOT_FIXED		1	// Fixed slot-id (non-expanded 2nd cartridge slot)
#define SCC_SLOT_USER		2	// Slot defined by the user at runtime
#define SCC_SLOT_AUTO		3	// First auto-detected cartridge

#ifndef SCC_SLOT_MODE
#define SCC_SLOT_MODE		SCC_SLOT_USER
#endif
#ifndef SCC_USE_RESUME
#define SCC_USE_RESUME		TRUE
#endif
#ifndef SCC_USE_EXTA
#define SCC_USE_EXTA		TRUE
#endif

// Fallback for SLOT_2 (normally from system.h) so the FIXED branch resolves.
#ifndef SLOT_2
#define SLOT_2				(0x02)
#endif

#if (SCC_SLOT_MODE == SCC_SLOT_FIXED)
	#define SCC_SLOT		SLOT_2 // Non-expanded second cartridge slot
#else
	extern u8 g_SCC_SlotId;
	#define SCC_SLOT		g_SCC_SlotId
#endif

//=============================================================================
// SCC REGISTERS (offsets inside the 0x9800 access window)
//=============================================================================

// Read / write - 32 bytes each
#define SCC_REG_WAVE_1		0x00 // Waveform channel 1
#define SCC_REG_WAVE_2		0x20 // Waveform channel 2
#define SCC_REG_WAVE_3		0x40 // Waveform channel 3
#define SCC_REG_WAVE_4		0x60 // Waveform channel 4 (and channel 5 for SCC)
#define SCC_REG_WAVE_5		0xA0 // Waveform channel 5 (SCC-I only)

// Write only - 16 bits each
#define SCC_REG_FREQ_1		0x80 // Frequency channel 1
#define SCC_REG_FREQ_2		0x82 // Frequency channel 2
#define SCC_REG_FREQ_3		0x84 // Frequency channel 3
#define SCC_REG_FREQ_4		0x86 // Frequency channel 4
#define SCC_REG_FREQ_5		0x88 // Frequency channel 5

// Write only - 8 bits each
#define SCC_REG_VOL_1		0x8A // Volume channel 1
#define SCC_REG_VOL_2		0x8B // Volume channel 2
#define SCC_REG_VOL_3		0x8C // Volume channel 3
#define SCC_REG_VOL_4		0x8D // Volume channel 4
#define SCC_REG_VOL_5		0x8E // Volume channel 5

// Write only - 8 bits
#define SCC_REG_MIXER		0x8F // On/off switch channel 1 to 5

// Write only - 8 bits
#define SCC_REG_TEST		0xC0 // Test register

//-----------------------------------------------------------------------------
// SCC Access Address (absolute, page 2)

#define SCC_ADDR_WAVE_1		(0x9800 + SCC_REG_WAVE_1)
#define SCC_ADDR_WAVE_2		(0x9800 + SCC_REG_WAVE_2)
#define SCC_ADDR_WAVE_3		(0x9800 + SCC_REG_WAVE_3)
#define SCC_ADDR_WAVE_4		(0x9800 + SCC_REG_WAVE_4)
#define SCC_ADDR_WAVE_5		(0x9800 + SCC_REG_WAVE_5)
#define SCC_ADDR_FREQ_1		(0x9800 + SCC_REG_FREQ_1)
#define SCC_ADDR_FREQ_2		(0x9800 + SCC_REG_FREQ_2)
#define SCC_ADDR_FREQ_3		(0x9800 + SCC_REG_FREQ_3)
#define SCC_ADDR_FREQ_4		(0x9800 + SCC_REG_FREQ_4)
#define SCC_ADDR_FREQ_5		(0x9800 + SCC_REG_FREQ_5)
#define SCC_ADDR_VOL_1		(0x9800 + SCC_REG_VOL_1)
#define SCC_ADDR_VOL_2		(0x9800 + SCC_REG_VOL_2)
#define SCC_ADDR_VOL_3		(0x9800 + SCC_REG_VOL_3)
#define SCC_ADDR_VOL_4		(0x9800 + SCC_REG_VOL_4)
#define SCC_ADDR_VOL_5		(0x9800 + SCC_REG_VOL_5)
#define SCC_ADDR_MIXER		(0x9800 + SCC_REG_MIXER)
#define SCC_ADDR_TEST		(0x9800 + SCC_REG_TEST)

// Konami mapper bank-select register that toggles SCC sound mode.
#define SCC_ENABLE_ADDR		0x9000
#define SCC_ENABLE_VALUE	0x3F

//=============================================================================
// PROTOTYPES
//=============================================================================

// Group: Core

// Function: SCC_Initialize
// Initialize SCC module (enable sound mode and mute the chip).
#ifndef SLOT_NOTFOUND
#define SLOT_NOTFOUND		0xFF
#endif

// Function: SCC_AutoDetect
// Scan every slot for an SCC cartridge (only with SCC_SLOT_MODE == SCC_SLOT_AUTO).
// Returns the slot ID, or SLOT_NOTFOUND. Writes 2 probe bytes into page 2 of each
// slot it tries.
#if (SCC_SLOT_MODE == SCC_SLOT_AUTO)
u8 SCC_AutoDetect();
#endif

bool SCC_Initialize();

#if (SCC_SLOT_MODE == SCC_SLOT_USER)
// Function: SCC_SetSlot
// Set SCC slot ID
inline void SCC_SetSlot(u8 slotId) { g_SCC_SlotId = slotId; }
#endif

// Function: SCC_Select
// Select sound chip on SCC cartridge
void SCC_Select();

// Function: SCC_SetRegister
// Set the value of a given register
void SCC_SetRegister(u8 reg, u8 value);

// Function: SCC_GetRegister
// Get the value of a given register
u8 SCC_GetRegister(u8 reg);

// Function: SCC_Mute
// Mute the SCC sound chip
void SCC_Mute();

#if (SCC_USE_RESUME)
// Function: SCC_Resume
// Resume SCC sound
void SCC_Resume();
#endif

#if (SCC_USE_EXTA)
// Group: Extra

// Function: SCC_LoadWaveform
// Load a full waveform (32 bytes) into a channel (1 to 5).
void SCC_LoadWaveform(u8 channel, const u8* data);

// Function: SCC_SetFrequency
// Set channel (1 to 5) frequency.
void SCC_SetFrequency(u8 channel, u16 freq);

// Function: SCC_SetVolume
// Set channel volume
inline void SCC_SetVolume(u8 channel, u8 vol) { SCC_SetRegister(SCC_REG_VOL_1 + channel, vol); }

// Function: SCC_SetMixer
// Set channels mixer
inline void SCC_SetMixer(u8 mix) { SCC_SetRegister(SCC_REG_MIXER, mix); }

#endif // (SCC_USE_EXTA)

#endif // MSXMVL_SCC_H
