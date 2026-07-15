// Example: MSX-AUDIO (Y8950) — detect the cartridge and program a register.
#include "msx-audio.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	bool found;

	MSXAudio_Initialize();
	found = MSXAudio_Detect();

	MSXAudio_SetRegister(0x20, 0x21);    // op0: multiple + tremolo/vibrato flags
	MSXAudio_SetRegister(0x40, 0x1B);    // op0: key-scale level + total level

	R[1] = found ? 1 : 0;
	R[2] = MSXAudio_GetRegister(0x20);   // unlike the OPLL, the Y8950 reads back
	R[3] = MSXAudio_GetRegister(0x40);

	R[0] = (found && R[2] == 0x21 && R[3] == 0x1B) ? 0xA5 : 0x00;
	for (;;) {}
}
