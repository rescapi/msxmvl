// drums.c — upload a drum sample to the MSX-AUDIO and hit it.
//
// The Y8950's headline feature is ADPCM: real 4-bit samples played out of the RAM on the
// cartridge. You tell it how much sample RAM the board has, upload the sample bytes with
// MSXAudio_ADPCM_WriteMemory, then MSXAudio_ADPCM_Play streams them back out as sound.
#include "msx-audio.h"

#define SNARE_ADDR   0x1000       // where the snare sample lives in sample RAM

// A tiny snare sample (bytes of packed 4-bit ADPCM nibbles).
static const u8 g_Snare[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

// Upload the snare into the cartridge's sample RAM at startup.
void drum_upload_snare(void)
{
	MSXAudio_ADPCM_SetRamMode(MSXAUDIO_ADPCM_RAM_256K);   // the common 256 KB board
	MSXAudio_ADPCM_WriteMemory(SNARE_ADDR, g_Snare, 8);
}

// Hit the snare: play the uploaded sample once.
void drum_hit_snare(void)
{
	MSXAudio_ADPCM_Play(SNARE_ADDR, SNARE_ADDR + 7, 0x0400, 200);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
static u8 g_Back[8];

void main(void)
{
	bool found;
	u8 i, ok = 1;

	MSXAudio_Initialize();
	found = MSXAudio_Detect();

	drum_upload_snare();
	MSXAudio_ADPCM_ReadMemory(SNARE_ADDR, g_Back, 8);

	for (i = 0; i < 8; i++)
		if (g_Back[i] != g_Snare[i])
			ok = 0;

	drum_hit_snare();

	R[1] = found ? 1 : 0;
	R[2] = g_Back[0];     // 0x11 — straight out of the cartridge's RAM
	R[3] = g_Back[7];     // 0x88

	R[0] = (found && ok) ? 0xA5 : 0x00;
	for (;;) {}
}
