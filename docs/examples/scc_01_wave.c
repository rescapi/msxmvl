// Example: SCC — load a waveform, then read it back out of the chip.
// The waveform table is the only part of the SCC you can read, which makes it
// the one register bank a test can actually verify.
#include "scc.h"
volatile u8 __at(0xE000) R[8];

// A 32-step waveform, one signed byte per step (a crude ramp).
static const u8 g_Wave[32] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x7F, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
	0x00, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
	0x81, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};

void main(void)
{
	bool ok;

	ok = SCC_Initialize();                  // FALSE if no SCC cartridge

	SCC_LoadWaveform(0, g_Wave);            // channel 0's 32-byte wave table
	SCC_SetFrequency(0, 0x01BC);            // ~440 Hz
	SCC_SetVolume(0, 15);                   // 0..15
	SCC_SetMixer(0x01);                     // enable channel 0 only

	R[1] = ok ? 1 : 0;
	R[2] = SCC_GetRegister(SCC_REG_WAVE_1 + 0);    // 0x00
	R[3] = SCC_GetRegister(SCC_REG_WAVE_1 + 8);    // 0x7F — the ramp's peak
	R[4] = SCC_GetRegister(SCC_REG_WAVE_1 + 24);   // 0x81 — its trough

	R[0] = (ok && R[2] == 0x00 && R[3] == 0x7F && R[4] == 0x81) ? 0xA5 : 0x00;
	for (;;) {}
}
