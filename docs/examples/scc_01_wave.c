// instrument.c — give an SCC channel its own voice, then play a note on it.
//
// The SCC is a wavetable chip: each channel plays back a 32-byte waveform you upload, so
// you design the instrument's timbre yourself. SCC_LoadWaveform sends the 32 bytes,
// SCC_SetFrequency picks the pitch, SCC_SetVolume the level, and SCC_SetMixer switches
// the channel on. The wave table is the one bank you can read back, to confirm it landed.
#include "scc.h"

#define VOICE_LEAD   0     // channel 0 — our lead voice

// A 32-step waveform, one signed byte per step (a crude triangle-ish ramp).
static const u8 g_Wave[32] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x7F, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
	0x00, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
	0x81, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};

// Give the lead channel our waveform and sound an A (~440 Hz) on it.
void play_lead_A(void)
{
	SCC_LoadWaveform(VOICE_LEAD, g_Wave);   // upload channel 0's 32-byte instrument
	SCC_SetFrequency(VOICE_LEAD, 0x01BC);   // ~440 Hz
	SCC_SetVolume(VOICE_LEAD, 15);          // 0..15
	SCC_SetMixer(0x01);                     // enable channel 0 only
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	bool ok = SCC_Initialize();             // FALSE if no SCC cartridge

	play_lead_A();

	R[1] = ok ? 1 : 0;
	R[2] = SCC_GetRegister(SCC_REG_WAVE_1 + 0);    // 0x00
	R[3] = SCC_GetRegister(SCC_REG_WAVE_1 + 8);    // 0x7F — the ramp's peak
	R[4] = SCC_GetRegister(SCC_REG_WAVE_1 + 24);   // 0x81 — its trough

	R[0] = (ok && R[2] == 0x00 && R[3] == 0x7F && R[4] == 0x81) ? 0xA5 : 0x00;
	for (;;) {}
}
