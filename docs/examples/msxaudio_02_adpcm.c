// Example: MSX-AUDIO ADPCM — upload a sample into the cartridge's sample RAM,
// read it back, and play it. Works with both common RAM sizes.
#include "msx-audio.h"
volatile u8 __at(0xE000) R[8];

static const u8 g_Sample[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
static u8 g_Back[8];

#define SAMPLE_ADDR  0x1000       // byte address in the cartridge's sample RAM

void main(void)
{
	bool found;
	u8 i, ok = 1;

	MSXAudio_Initialize();
	found = MSXAudio_Detect();

	MSXAudio_ADPCM_SetRamMode(MSXAUDIO_ADPCM_RAM_256K);   // the common 256 KB board
	MSXAudio_ADPCM_WriteMemory(SAMPLE_ADDR, g_Sample, 8);
	MSXAudio_ADPCM_ReadMemory(SAMPLE_ADDR, g_Back, 8);

	for (i = 0; i < 8; i++)
		if (g_Back[i] != g_Sample[i])
			ok = 0;

	MSXAudio_ADPCM_Play(SAMPLE_ADDR, SAMPLE_ADDR + 7, 0x0400, 200);

	R[1] = found ? 1 : 0;
	R[2] = g_Back[0];     // 0x11 — straight out of the cartridge's RAM
	R[3] = g_Back[7];     // 0x88

	R[0] = (found && ok) ? 0xA5 : 0x00;
	for (;;) {}
}
