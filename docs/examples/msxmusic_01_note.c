// fmnote.c — play a single FM note on the MSX-MUSIC (OPLL / FM-PAC).
//
// The OPLL ships with 15 ready-made instruments, so a note is just three writes: pick the
// instrument + volume (0x3n), set the pitch as an F-number (0x1n + 0x2n), and set the
// key-ON bit (0x2n). MSXMusic_SetRegister selects a register and writes it in one call.
#include "msx-music.h"

// Play a piano note on channel 0.
void play_piano_note(void)
{
	MSXMusic_SetRegister(0x30, 0x50);    // ch0: instrument 5 (piano), max volume
	MSXMusic_SetRegister(0x10, 0xAD);    // ch0: F-number low byte
	MSXMusic_SetRegister(0x20, 0x15);    // ch0: key-ON + octave + F-number high
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 type = MSXMusic_Initialize();     // scans the slots for an OPLL

	R[1] = type;                         // 1 = internal, 2 = external (FM-PAC)
	R[2] = MSXMusic_GetSlotId();         // where it was found

	play_piano_note();

	R[0] = (type != MSXMUSIC_NOTFOUND) ? 0xA5 : 0x00;
	for (;;) {}
}
