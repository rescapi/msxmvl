// msxmvl — clean-room reimplementation of MSXgl "msx-audio" module public API.
// Interface (names, signatures, documented behaviour, register/port map) taken
// from MSXgl's engine/src/msx-audio.h + msx-audio_reg.h + system_port.h headers
// ONLY. No MSXgl implementation source (.c/.asm/.lst) was read.
//
// MSX-Audio (Y8950 / OPL1 + ADPCM) handler.
// The chip is accessed through two I/O ports of the primary unit:
//   0xC0 - register index port (write); reading it returns the STATUS register
//   0xC1 - register data port  (write); reading it returns the selected
//          register's data (only meaningful for the few readable registers)
//
// Toolchain: SDCC --sdcccall 1. The MSXgl msx-audio header declares no
// __PRESERVES() contract, so the bodies below follow the standard SDCC calling
// convention (AF/BC/DE/HL/IX caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_MSXAUDIO_H
#define MSXMVL_MSXAUDIO_H

#include "types.h"

//=============================================================================
// CONFIG (defaults mirror MSXgl msxgl_config.h)
//=============================================================================
#ifndef MSXAUDIO_USE_RESUME
#define MSXAUDIO_USE_RESUME		TRUE	// Add function to allow playback pause and resume
#endif

//=============================================================================
// MSX-AUDIO I/O PORTS (from system_port.h)
//=============================================================================
// Primary MSX-Audio I/O ports
#define P_MSXAUDIO_INDEX		0xC0	// Register index (write) / Status (read)
__sfr __at(P_MSXAUDIO_INDEX)	g_MSXAudio_IndexPort;
#define P_MSXAUDIO_DATA			0xC1	// Register data
__sfr __at(P_MSXAUDIO_DATA)		g_MSXAudio_DataPort;

// Secondary MSX-Audio I/O ports
#define P_MSXAUDIO_INDEX2		0xC2	// Register index (write) / Status (read)
__sfr __at(P_MSXAUDIO_INDEX2)	g_MSXAudio_IndexPort2;
#define P_MSXAUDIO_DATA2		0xC3	// Register data
__sfr __at(P_MSXAUDIO_DATA2)	g_MSXAudio_DataPort2;

//=============================================================================
// REGISTER / STATUS BITS (subset from msx-audio_reg.h)
//=============================================================================
#define MSXAUDIO_STATUS_PCM			0b00000001
#define MSXAUDIO_STATUS_BUFFER		0b00001000
#define MSXAUDIO_STATUS_EOS			0b00010000
#define MSXAUDIO_STATUS_TIMER2		0b00100000
#define MSXAUDIO_STATUS_TIMER1		0b01000000
#define MSXAUDIO_STATUS_IRQ			0b10000000

#define MSXAUDIO_REG_TEST			0x01
#define MSXAUDIO_REG_TIMER_1		0x02
#define MSXAUDIO_REG_TIMER_2		0x03
#define MSXAUDIO_REG_FLAG_CTRL		0x04
#define MSXAUDIO_REG_RHYTHM			0xBD
#define MSXAUDIO_REG_CTRL_1			0xB0	// F-Num MSB / block / KEY (channels 0xB0..0xB8)

// R#04 (Flag control) bits
#define MSXAUDIO_FLAG_ST1			0b00000001	// Start timer-1
#define MSXAUDIO_FLAG_ST2			0b00000010	// Start timer-2
#define MSXAUDIO_FLAG_MT2			0b00100000	// Mask timer-2 flag
#define MSXAUDIO_FLAG_MT1			0b01000000	// Mask timer-1 flag
#define MSXAUDIO_FLAG_IRQ_RESET		0b10000000	// Reset IRQ status

// R#B0..B8 KEY bit (channel on/off)
#define MSXAUDIO_KEY_ON				0b00100000

//=============================================================================
// PROTOTYPES
//=============================================================================

// Function: MSXAudio_Initialize
// Initialize MSX-Audio module
void MSXAudio_Initialize(void);

// Function: MSXAudio_Detect
// Search for MSX-Audio (Y8950) chip
//
// Return:
//   FALSE if no MSX-Audio chip found
bool MSXAudio_Detect(void);

// Function: MSXAudio_SetRegister
// Set MSX-Audio register value
//
// Parameters:
//   reg   - Register to set
//   value - Value to set
void MSXAudio_SetRegister(u8 reg, u8 value);

// Function: MSXAudio_GetRegister
// Get MSX-Audio register value
//
// Parameters:
//   reg - Register to get
//
// Return:
//   Value of the register
u8 MSXAudio_GetRegister(u8 reg);

// Function: MSXAudio_Mute
// Mute MSX-Audio sound
void MSXAudio_Mute(void);

#if (MSXAUDIO_USE_RESUME)
// Function: MSXAudio_Resume
// Resume MSX-Audio sound
void MSXAudio_Resume(void);
#endif


//=============================================================================
// ADPCM — sample playback (the Y8950's headline feature)
//=============================================================================
// The Y8950 can record and replay 4-bit ADPCM samples out of the sample RAM on
// the cartridge. That is what gives MSX-AUDIO real drums and voices instead of
// synthesised approximations.
//
// Sample RAM sizes differ per cartridge, and the chip must be told which it has:
//   - Philips Music Module ships with a SMALL DRAM  -> MSXAUDIO_ADPCM_RAM_64K
//   - the common upgrade is 256 KB                  -> MSXAUDIO_ADPCM_RAM_256K
// The bit selects the DRAM addressing width; get it wrong and addresses wrap.
#define MSXAUDIO_ADPCM_RAM_256K   0x00   // 256 KB sample RAM (the common expansion)
#define MSXAUDIO_ADPCM_RAM_64K    0x02   // <= 64 KB sample RAM (stock Music Module)

// Function: MSXAudio_ADPCM_SetRamMode
// Tell the chip how much sample RAM the cartridge has. Call before any of the
// functions below. Defaults to MSXAUDIO_ADPCM_RAM_256K.
void MSXAudio_ADPCM_SetRamMode(u8 mode);

// Function: MSXAudio_ADPCM_Reset
// Reset the ADPCM unit and re-apply the RAM mode.
void MSXAudio_ADPCM_Reset(void);

// Function: MSXAudio_ADPCM_WriteMemory
// Upload sample data into the cartridge's sample RAM.
//
// Parameters:
//   addr - byte address in sample RAM. The chip addresses in 4-byte units, so
//          this is rounded down to a multiple of 4.
//   data - source bytes in main RAM
//   len  - number of bytes
void MSXAudio_ADPCM_WriteMemory(u32 addr, const u8* data, u16 len);

// Function: MSXAudio_ADPCM_ReadMemory
// Read sample data back out of the cartridge's sample RAM.
void MSXAudio_ADPCM_ReadMemory(u32 addr, u8* data, u16 len);

// Function: MSXAudio_ADPCM_Play
// Play the sample stored between two byte addresses in sample RAM.
//
// Parameters:
//   start  - first byte address
//   stop   - last byte address
//   delta  - DELTA-N: the replay rate. Higher = faster/higher pitched.
//   volume - 0..255
void MSXAudio_ADPCM_Play(u32 start, u32 stop, u16 delta, u8 volume);

// Function: MSXAudio_ADPCM_Stop
// Stop playback.
void MSXAudio_ADPCM_Stop(void);

#endif // MSXMVL_MSXAUDIO_H
