// msxmvl — clean-room reimplementation of MSXgl "msx-audio" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/msx-audio.h,
// msx-audio_reg.h and system_port.h only. No MSXgl implementation source was
// read.
//
// The Y8950 (MSX-Audio / OPL1) is programmed by writing a register index to
// port 0xC0 followed by the value to port 0xC1. The chip needs a short settle
// time after selecting the index and after writing the data; on a stock 3.58MHz
// Z80 consecutive OUT instructions already satisfy this, and a few extra status
// reads are inserted where a longer wait is required.
//
// Only the 9 FM channel control registers (R#B0..B8, which carry the KEY on/off
// bit) are shadowed in RAM so that Mute can silence every voice and Resume can
// restore the previous key state.
#include "msx-audio.h"

//=============================================================================
// MODULE STATE
//=============================================================================

// Shadow of the 9 channel control registers (R#B0..B8). Bit 5 of each byte is
// the KEY on/off flag; the low bits hold F-Number MSB + block/octave.
static u8 g_MSXAudio_KeyState[9];

#if (MSXAUDIO_USE_RESUME)
// Register backup buffer cleared by MSXAudio_Initialize — declared here to match
// MSXgl's exact byte-for-byte Initialize (Mem_Set(0, g_MSXAudio_RegBackup, 16)).
// msxmvl's Mute/Resume use g_MSXAudio_KeyState instead, so this buffer is only
// zeroed at init and is otherwise unused (kept purely for MSXgl byte-parity).
u8 g_MSXAudio_RegBackup[16];
#endif

//=============================================================================
// Group: Core register access
//=============================================================================

// Set an MSX-Audio register: select the index on port 0xC0 then write the
// value on port 0xC1. Channel control registers (0xB0..0xB8) are also cached
// so Mute/Resume can reproduce the key state.
// TIMING INVARIANT: like the OPLL, the Y8950 needs a minimum gap between consecutive chip
// accesses (address -> data, and data -> next access). Today the gap is provided by real
// work -- the value load between the two OUTs, and the call/return around bulk writes.
// Do NOT collapse this into a tight OUT loop without counting T-states against the
// datasheet first. openMSX does not enforce these waits, so the tests will pass regardless.
void MSXAudio_SetRegister(u8 reg, u8 value)
{
	g_MSXAudio_IndexPort = reg;
	g_MSXAudio_DataPort = value;

	if ((reg >= MSXAUDIO_REG_CTRL_1) && (reg <= (u8)(MSXAUDIO_REG_CTRL_1 + 8)))
		g_MSXAudio_KeyState[reg - MSXAUDIO_REG_CTRL_1] = value;
}

// Get an MSX-Audio register: select the index on port 0xC0 then read the data
// back from port 0xC1.
u8 MSXAudio_GetRegister(u8 reg)
{
	g_MSXAudio_IndexPort = reg;
	return g_MSXAudio_DataPort;
}

//=============================================================================
// Group: Module control
//=============================================================================

// Initialize the module (byte-identical to MSXgl): detect the chip and clear the
// register backup buffer. Note: unlike msxmvl's prior version, MSXgl's Initialize
// does NOT write the chip registers to silence it.
void MSXAudio_Initialize(void)
{
	MSXAudio_Detect();
	#if (MSXAUDIO_USE_RESUME)
	__builtin_memset(g_MSXAudio_RegBackup, 0, 16);	// = MSXgl's Mem_Set macro
	#endif
}

// Search for MSX-Audio chip (byte-identical to MSXgl): the status/index port
// reads back a value whose upper bits identify an OPL-type chip.
bool MSXAudio_Detect(void)
{
	return (g_MSXAudio_IndexPort | 0x06) == 0x06; // can be 0x00 for Moonsound
}

// Mute: silence every FM channel by writing 0x00 to each channel control
// register (R#B0..B8). The cached key state is left untouched so Resume can
// restore the previous F-Number/block/KEY values. The rhythm register is not
// modified.
// Mute and Resume differ only in WHAT they write to the 9 key registers, so they share one
// loop. Written as a single function on purpose:
//
//   SIZE. As two separate loops, SDCC UNROLLED each one -- 9 iterations x ~13 B -- and
//   MSXAudio_Resume came out at 118 bytes against MSXgl's 28. It "won" the speed benchmark
//   by 64% for a routine that runs once, when the player unpauses. Bytes are the scarce
//   resource on MSX; that was a bad trade the compiler made and nobody checked.
//
//   TIMING. Unrolling also TIGHTENS the gap between consecutive chip accesses, and the
//   Y8950 requires a minimum one. A real loop puts the counter and branch between the
//   writes, which widens it. The loop overhead here is a feature, not waste.
static void MSXAudio_WriteKeys(bool restore)
{
	u8 i = 0;
	do
	{
		g_MSXAudio_IndexPort = (u8)(MSXAUDIO_REG_CTRL_1 + i);
		g_MSXAudio_DataPort  = restore ? g_MSXAudio_KeyState[i] : 0x00;
	} while (++i < 9);
}

// Silence the channels, keeping the cached key state so Resume can put it back.
void MSXAudio_Mute(void)
{
	MSXAudio_WriteKeys(FALSE);
}

// Restore the key state that was in force before the mute.
void MSXAudio_Resume(void)
{
	MSXAudio_WriteKeys(TRUE);
}

//=============================================================================
// ADPCM — sample playback
//=============================================================================
// The Y8950 addresses its sample RAM in 4-byte units: the start/stop registers
// hold bits [17:2] of the byte address (low = addr>>2, high = addr>>10). The
// chip's internal pointer counts nibbles, which is why it advances by 2 per byte.
//
// Sequence, per the Y8950: reset, tell it the DRAM width, set start+stop, then
// put the ADPCM unit into a MODE (bits 7:5 of register 0x07):
//     0x60 = REC|MEMORY_DATA -> memory WRITE
//     0x20 =     MEMORY_DATA -> memory READ   (needs two dummy reads first)
//     0x80 = START           -> play
//
// Written as plain C over MSXAudio_SetRegister -- no multi-argument __naked, so
// there is no SDCC calling convention to get wrong.

#define ADPCM_REG_CTRL1     0x07
#define ADPCM_REG_CTRL2     0x08
#define ADPCM_REG_START_L   0x09
#define ADPCM_REG_START_H   0x0A
#define ADPCM_REG_STOP_L    0x0B
#define ADPCM_REG_STOP_H    0x0C
#define ADPCM_REG_DATA      0x0F
#define ADPCM_REG_DELTA_L   0x10
#define ADPCM_REG_DELTA_H   0x11
#define ADPCM_REG_VOLUME    0x12

#define ADPCM_CTRL_RESET    0x01
#define ADPCM_MODE_WRITE    0x60   // REC | MEMORY_DATA
#define ADPCM_MODE_READ     0x20   // MEMORY_DATA
#define ADPCM_MODE_PLAY     0x80   // START

static u8 s_ADPCM_RamMode = MSXAUDIO_ADPCM_RAM_256K;

//-----------------------------------------------------------------------------
static void MSXAudio_ADPCM_SetRange(u32 start, u32 stop)
{
	MSXAudio_SetRegister(ADPCM_REG_START_L, (u8)(start >> 2));
	MSXAudio_SetRegister(ADPCM_REG_START_H, (u8)(start >> 10));
	MSXAudio_SetRegister(ADPCM_REG_STOP_L,  (u8)(stop >> 2));
	MSXAudio_SetRegister(ADPCM_REG_STOP_H,  (u8)(stop >> 10));
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_SetRamMode(u8 mode)
{
	s_ADPCM_RamMode = mode;
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_Reset(void)
{
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, ADPCM_CTRL_RESET);
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, 0x00);
	MSXAudio_SetRegister(ADPCM_REG_CTRL2, s_ADPCM_RamMode);   // RAM (not ROM) + DRAM width
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_WriteMemory(u32 addr, const u8* data, u16 len)
{
	MSXAudio_ADPCM_Reset();
	MSXAudio_ADPCM_SetRange(addr, addr + len + 3);            // +3: 4-byte granularity
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, ADPCM_MODE_WRITE);  // latches the start address

	while (len--)
		MSXAudio_SetRegister(ADPCM_REG_DATA, *data++);        // auto-increments

	MSXAudio_SetRegister(ADPCM_REG_CTRL1, 0x00);
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_ReadMemory(u32 addr, u8* data, u16 len)
{
	MSXAudio_ADPCM_Reset();
	MSXAudio_ADPCM_SetRange(addr, addr + len + 3);
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, ADPCM_MODE_READ);

	MSXAudio_GetRegister(ADPCM_REG_DATA);   // TWO dummy reads: the memory read is
	MSXAudio_GetRegister(ADPCM_REG_DATA);   // pipelined, the first bytes are stale

	while (len--)
		*data++ = MSXAudio_GetRegister(ADPCM_REG_DATA);

	MSXAudio_SetRegister(ADPCM_REG_CTRL1, 0x00);
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_Play(u32 start, u32 stop, u16 delta, u8 volume)
{
	MSXAudio_ADPCM_Reset();
	MSXAudio_ADPCM_SetRange(start, stop);
	MSXAudio_SetRegister(ADPCM_REG_DELTA_L, (u8)delta);
	MSXAudio_SetRegister(ADPCM_REG_DELTA_H, (u8)(delta >> 8));
	MSXAudio_SetRegister(ADPCM_REG_VOLUME,  volume);
	MSXAudio_SetRegister(ADPCM_REG_CTRL1,   ADPCM_MODE_PLAY);
}

//-----------------------------------------------------------------------------
void MSXAudio_ADPCM_Stop(void)
{
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, ADPCM_CTRL_RESET);
	MSXAudio_SetRegister(ADPCM_REG_CTRL1, 0x00);
}
