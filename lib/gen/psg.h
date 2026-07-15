// msxmvl — clean-room reimplementation of MSXgl "psg" module public API.
// Interface (names, signatures, documented behaviour, register/port map) taken
// from MSXgl's engine/src/psg.h + psg_reg.h + system_port.h headers ONLY. No
// MSXgl implementation source (.c/.asm/.lst) was read.
//
// AY-3-8910 / YM2149 Programmable Sound Generator handler.
//   Internal PSG is accessed through three I/O ports:
//     0xA0 - register-select (address latch)
//     0xA1 - data write
//     0xA2 - data read
//   The chip exposes 16 registers (R#0..R#15):
//     R0/R1  - Tone A period (12-bit)   R2/R3  - Tone B   R4/R5 - Tone C
//     R6     - Noise period (5-bit)     R7     - Mixer control (active-low)
//     R8/9/10- Amplitude A/B/C (bit4 = envelope enable)
//     R11/R12- Envelope period (16-bit) R13    - Envelope shape
//     R14/R15- I/O port A / B
//
// Access model (default PSG_ACCESS == PSG_INDIRECT): setters write a 16-byte
// shadow buffer (g_PSG_Regs, laid out to match R#0..R#15 exactly); PSG_Apply()
// flushes R#0..R#13 to the chip once per frame. The mixer field is stored in
// MSXgl "logical" convention (a set bit = generator ENABLED, matching the
// PSG_*_ON constants); PSG_Apply inverts the six tone/noise bits when writing
// the chip, since the hardware mixer is active-low.
//
// Toolchain: SDCC --sdcccall 1. MSXgl's psg.h declares no __PRESERVES()
// contract on any function, so the bodies follow the standard SDCC calling
// convention (AF/BC/DE/HL/IX caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_PSG_H
#define MSXMVL_PSG_H

#include "types.h"

//=============================================================================
// CONFIG (defaults mirror MSXgl template msxgl_config.h)
//=============================================================================
#define PSG_INTERNAL			0
#define PSG_EXTERNAL			1
#define PSG_BOTH				2
#define PSG_DIRECT				0
#define PSG_INDIRECT			1

#ifndef PSG_CHIP
#define PSG_CHIP				PSG_INTERNAL
#endif
#ifndef PSG_ACCESS
#define PSG_ACCESS				PSG_INDIRECT
#endif
#ifndef PSG_USE_NOTES
#define PSG_USE_NOTES			FALSE
#endif
#ifndef PSG_USE_EXTRA
#define PSG_USE_EXTRA			TRUE
#endif
#ifndef PSG_USE_RESUME
#define PSG_USE_RESUME			TRUE
#endif

//=============================================================================
// PSG I/O PORTS (from system_port.h)
//=============================================================================
#if (PSG_CHIP == PSG_EXTERNAL)
	#define P_PSG_REGS			0x10
	#define P_PSG_DATA			0x11
	#define P_PSG_STAT			0x12
#else
	#define P_PSG_REGS			0xA0   // register-select (address latch)
	#define P_PSG_DATA			0xA1   // data write
	#define P_PSG_STAT			0xA2   // data read
#endif
__sfr __at(P_PSG_REGS)			g_PSG_RegPort;
__sfr __at(P_PSG_DATA)			g_PSG_DataPort;
__sfr __at(P_PSG_STAT)			g_PSG_StatPort;

//=============================================================================
// REGISTER MAP (from psg_reg.h)
//=============================================================================
#define PSG_REG(a)				((a) & 0x0F)
#define PSG_CHAN(a)				((a) & 0x03)

#define PSG_REG_TONE_A			0
#define PSG_REG_TONEC_A			1
#define PSG_REG_TONE_B			2
#define PSG_REG_TONEC_B			3
#define PSG_REG_TONE_C			4
#define PSG_REG_TONEC_C			5
#define PSG_REG_NOISE			6
#define PSG_REG_MIXER			7
#define PSG_REG_AMP_A			8
#define PSG_REG_AMP_B			9
#define PSG_REG_AMP_C			10
#define PSG_REG_ENV				11
#define PSG_REG_ENVC			12
#define PSG_REG_SHAPE			13
#define PSG_REG_IO_PORT_A		14
#define PSG_REG_IO_PORT_B		15

// Mixer bits (R#7) — MSXgl logical convention: set = enabled.
#define PSG_F_TA				0x01
#define PSG_F_TB				0x02
#define PSG_F_TC				0x04
#define PSG_F_NA				0x08
#define PSG_F_NB				0x10
#define PSG_F_NC				0x20
#define PSG_F_IA				0x40
#define PSG_F_IB				0x80
#define PSG_MIXER_TONE_NOISE	0x3F   // six tone+noise bits (inverted for HW)

// Amplitude bit (R#8..10)
#define PSG_F_ENV				0x10   // volume controlled by envelope

// Envelope shape bits (R#13)
#define PSG_F_HLD				0x01
#define PSG_F_ALT				0x02
#define PSG_F_ATT				0x04
#define PSG_F_CNT				0x08

// Channels
#define PSG_CHANNEL_A			0
#define PSG_CHANNEL_B			1
#define PSG_CHANNEL_C			2

// Mixer helper constants (as in psg.h)
#define PSG_TONE_A_ON			0b00000001
#define PSG_TONE_B_ON			0b00000010
#define PSG_TONE_C_ON			0b00000100
#define PSG_NOISE_A_ON			0b00001000
#define PSG_NOISE_B_ON			0b00010000
#define PSG_NOISE_C_ON			0b00100000
#define PSG_TONE_A_OFF			0
#define PSG_TONE_B_OFF			0
#define PSG_TONE_C_OFF			0
#define PSG_NOISE_A_OFF			0
#define PSG_NOISE_B_OFF			0
#define PSG_NOISE_C_OFF			0

#define PSG_USE_ENVELOPE		PSG_F_ENV

#if (PSG_USE_NOTES)
enum NOTE
{
	NOTE_DO = 0, NOTE_DOd, NOTE_RE, NOTE_REd, NOTE_MI, NOTE_FA,
	NOTE_FAd, NOTE_SOL, NOTE_SOLd, NOTE_LA, NOTE_LAd, NOTE_SI,
	NOTE_C = 0, NOTE_Cd, NOTE_D, NOTE_Dd, NOTE_E, NOTE_F,
	NOTE_Fd, NOTE_G, NOTE_Gd, NOTE_A, NOTE_Ad, NOTE_B
};
#endif

//-----------------------------------------------------------------------------
// 16-byte shadow buffer, laid out to match R#0..R#15 one-to-one.
typedef struct PSG_Data
{
	u16		Tone[3];	// R#0..R#5
	u8		Noise;		// R#6
	u8		Mixer;		// R#7  (logical: set bit = enabled)
	u8		Volume[3];	// R#8..R#10
	u16		Envelope;	// R#11..R#12
	u8		Shape;		// R#13
	u8		IOPortA;	// R#14
	u8		IOPortB;	// R#15
} PSG_Data;

#if (PSG_ACCESS == PSG_INDIRECT)
extern PSG_Data g_PSG_Regs;
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

// Group: Register
void PSG_SetRegister(u8 reg, u8 value);
u8   PSG_GetRegister(u8 reg);

// Group: Helper
#if (PSG_USE_EXTRA)
void PSG_SetTone(u8 chan, u16 period);
void PSG_SetNoise(u8 period);
void PSG_SetMixer(u8 mix);
void PSG_SetVolume(u8 chan, u8 vol);
void PSG_SetEnvelope(u16 period);
void PSG_SetShape(u8 shape);
void PSG_EnableTone(u8 chan, bool val);
void PSG_EnableNoise(u8 chan, bool val);
void PSG_EnableEnvelope(u8 chan, bool val);
#endif // (PSG_USE_EXTRA)

void PSG_Mute();

#if (PSG_USE_RESUME)
void PSG_Resume();
#endif

#if (PSG_ACCESS == PSG_INDIRECT)
// Group: Indirect
void PSG_Apply();
#endif

#endif // MSXMVL_PSG_H
