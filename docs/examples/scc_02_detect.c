// audio_boot.c — find the SCC cartridge at startup instead of guessing its slot.
//
// Built with -DSCC_SLOT_MODE=SCC_SLOT_AUTO, SCC_Initialize scans every slot and only
// returns TRUE if a real SCC answers — so the game knows whether to use SCC music or
// fall back. g_SCC_SlotId then holds the slot it was actually found in.
#include "scc.h"

// The lead voice's waveform (see instrument.c).
static const u8 g_Wave[32] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x7F, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
	0x00, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
	0x81, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};

// Detect the SCC (scanning every slot) and prime the lead channel. Returns TRUE if found.
bool audio_init_scc(void)
{
	bool found = SCC_Initialize();     // scans the slots, then selects the chip
	SCC_LoadWaveform(0, g_Wave);       // prime channel 0's instrument
	return found;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	bool found = audio_init_scc();

	R[1] = found ? 1 : 0;              // 0 if no SCC cartridge is present
	R[2] = g_SCC_SlotId;               // the slot it was actually found in
	R[3] = SCC_GetRegister(SCC_REG_WAVE_1 + 8);   // 0x7F — proves we found it

	R[0] = (found && g_SCC_SlotId != SLOT_NOTFOUND && R[3] == 0x7F) ? 0xA5 : 0x00;
	for (;;) {}
}
