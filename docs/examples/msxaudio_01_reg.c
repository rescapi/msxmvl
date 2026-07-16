// fmvoice.c — build a custom FM voice on the MSX-AUDIO (Y8950).
//
// The Y8950 has no canned instruments: you shape a voice by writing its operator
// registers yourself. MSXAudio_SetRegister writes one. Unlike the OPLL, the Y8950 can be
// read back with MSXAudio_GetRegister, so the game can confirm the chip is really there.
#include "msx-audio.h"

// Program operator 0 of the first channel: its tone shape and its level.
void fmvoice_program_op0(void)
{
	MSXAudio_SetRegister(0x20, 0x21);    // op0: multiple + tremolo/vibrato flags
	MSXAudio_SetRegister(0x40, 0x1B);    // op0: key-scale level + total level
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	bool found;

	MSXAudio_Initialize();
	found = MSXAudio_Detect();

	fmvoice_program_op0();

	R[1] = found ? 1 : 0;
	R[2] = MSXAudio_GetRegister(0x20);   // unlike the OPLL, the Y8950 reads back
	R[3] = MSXAudio_GetRegister(0x40);

	R[0] = (found && R[2] == 0x21 && R[3] == 0x1B) ? 0xA5 : 0x00;
	for (;;) {}
}
