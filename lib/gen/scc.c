// msxmvl "scc" module — ORIGINAL implementation.
//
// Clean-room: written from scc.h's interface and the Konami SCC's published register map.
// No MSXgl implementation source was consulted. (The file this replaces stated in its own
// header that its bodies were "adopted from MSXgl (engine/src/scc.c, CC BY-SA) to match its
// exact codegen" -- which made an MIT release impossible.)
//
// The SCC lives on a CARTRIDGE, not in the machine, so every access is an inter-slot one.
// The chip is not visible until you ask for it: writing 0x3F to the cartridge's bank
// register at 0x9000 swaps the SCC's register window into page 2, after which the sound
// registers appear at 0x9800.
//
// Unlike the OPLL and the Y8950, the SCC has NO inter-access wait requirement -- its
// registers behave like memory. So there is no timing invariant to preserve here.
#include "scc.h"
#include "system.h"

// From bios.h, declared directly: pulling the whole header would double-define the inline
// Call helpers that also live in system.h.
u8   BIOS_InterSlotRead(u8 slot, u16 addr);
void BIOS_InterSlotWrite(u8 slot, u16 addr, u8 value);

//=============================================================================
// DATA
//=============================================================================

#if (SCC_USE_EXTA)
// Waveform base address per channel. On the plain SCC channels 4 and 5 SHARE a waveform;
// the SCC-I gives channel 5 its own (SCC_ADDR_WAVE_5).
const u16 g_SCC_WaveformAddr[5] =
{
	SCC_ADDR_WAVE_1, SCC_ADDR_WAVE_2, SCC_ADDR_WAVE_3, SCC_ADDR_WAVE_4, SCC_ADDR_WAVE_5,
};
#endif

#if (SCC_USE_RESUME)
u8 g_SCC_MixerBackup;   // the mixer is write-only, so Resume needs a copy of what we set
#endif

#if ((SCC_SLOT_MODE == SCC_SLOT_USER) || (SCC_SLOT_MODE == SCC_SLOT_AUTO))
u8 g_SCC_SlotId = SLOT_NOTFOUND;
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

#if (SCC_SLOT_MODE == SCC_SLOT_AUTO)
// Find the slot holding an SCC.
//
// The naive probe -- switch the SCC in, write a byte into the wave table, read it back --
// FALSE-POSITIVES on every RAM slot, because RAM echoes whatever you wrote just as happily.
// So check that the window actually BEHAVES like an SCC: the byte must read back with SCC
// mode ON, and must NOT with SCC mode OFF. RAM keeps echoing either way and is rejected.
//
// Writes 2 probe bytes into page 2 of each slot it tries; call it before you put anything
// of value at 0x9000/0x9800.
u8 SCC_AutoDetect()
{
	u8 ps, ss, slot, on, off;

	for (ps = 0; ps < 4; ++ps)
	{
		for (ss = 0; ss < 4; ++ss)
		{
			slot = (ss == 0) ? ps : (u8)(ps | (ss << 2) | SLOT_EXP);

			BIOS_InterSlotWrite(slot, 0x9000, 0x3F);   // SCC window ON
			BIOS_InterSlotWrite(slot, 0x9800, 0x3C);   // marker into the wave table
			on = BIOS_InterSlotRead(slot, 0x9800);

			BIOS_InterSlotWrite(slot, 0x9000, 0x00);   // SCC window OFF
			off = BIOS_InterSlotRead(slot, 0x9800);

			if ((on == 0x3C) && (off != 0x3C))
				return slot;
		}
	}
	return SLOT_NOTFOUND;
}
#endif

// Locate the chip and map its registers in. FALSE if there is no SCC to talk to.
bool SCC_Initialize()
{
	#if (SCC_SLOT_MODE == SCC_SLOT_AUTO)
		g_SCC_SlotId = SCC_AutoDetect();
		if (g_SCC_SlotId == SLOT_NOTFOUND)
			return FALSE;
	#elif (SCC_SLOT_MODE == SCC_SLOT_USER)
		// NOTE: this mode ASSUMES the cartridge is in slot 2 and returns TRUE without
		// checking. If the player's SCC is elsewhere, every write goes into the void and
		// you get silence with no error. Build with SCC_SLOT_MODE=SCC_SLOT_AUTO to find
		// out for real.
		g_SCC_SlotId = SLOT_2;
	#endif

	SCC_Select();
	return TRUE;
}

// Swap the SCC's register window into page 2 of the cartridge.
void SCC_Select()
{
	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		SET_BANK_SEGMENT(2, 0x3F);          // we ARE the cartridge: set our own bank
	#else
		BIOS_InterSlotWrite(SCC_SLOT, 0x9000, 0x3F);
	#endif
}

// Registers live at 0x9800 + reg once the window is mapped.
void SCC_SetRegister(u8 reg, u8 value)
{
	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		Poke(0x9800 + reg, value);
	#else
		BIOS_InterSlotWrite(SCC_SLOT, 0x9800 + reg, value);
	#endif

	#if (SCC_USE_RESUME)
	if (reg == SCC_REG_MIXER)
		g_SCC_MixerBackup = value;          // the only register we can't read back
	#endif
}

// Only the WAVE TABLE reads back. Frequency, volume and the mixer are write-only.
u8 SCC_GetRegister(u8 reg)
{
	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		return Peek(0x9800 + reg);
	#else
		return BIOS_InterSlotRead(SCC_SLOT, 0x9800 + reg);
	#endif
}

// Silence every channel.
//
// This must NOT go through SCC_SetRegister: that keeps a copy of the mixer for Resume, so
// muting through it would overwrite the backup with 0 -- and SCC_Resume() would then
// faithfully restore silence. Write the port directly and leave the copy alone.
void SCC_Mute()
{
	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		Poke(0x9800 + SCC_REG_MIXER, 0x00);
	#else
		BIOS_InterSlotWrite(SCC_SLOT, 0x9800 + SCC_REG_MIXER, 0x00);
	#endif
}

#if (SCC_USE_RESUME)
// Put back the mixer that was in force before the mute.
void SCC_Resume()
{
	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		Poke(0x9800 + SCC_REG_MIXER, g_SCC_MixerBackup);
	#else
		BIOS_InterSlotWrite(SCC_SLOT, 0x9800 + SCC_REG_MIXER, g_SCC_MixerBackup);
	#endif
}
#endif

// Upload a channel's 32-sample waveform. This is what gives the SCC its character: you
// define the wave, rather than picking from a fixed set of shapes.
void SCC_LoadWaveform(u8 channel, const u8* data)
{
	u16 addr = g_SCC_WaveformAddr[channel];
	u8  i    = 32;

	#if (SCC_SLOT_MODE == SCC_SLOT_DIRECT)
		Mem_Copy(data, addr, 32);
	#else
		do
		{
			BIOS_InterSlotWrite(SCC_SLOT, addr++, *data++);
		} while (--i);
	#endif
}

// 12-bit pitch, low byte then high. Lower value = higher note.
void SCC_SetFrequency(u8 channel, u16 freq)
{
	u8 reg = (u8)(SCC_REG_FREQ_1 + (channel << 1));

	SCC_SetRegister(reg,     (u8)(freq & 0xFF));
	SCC_SetRegister(reg + 1, (u8)(freq >> 8));
}
