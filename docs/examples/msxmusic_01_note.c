// Example: MSX-MUSIC (OPLL) — find the chip, then play a note.
// The OPLL is WRITE-ONLY: the CPU cannot read a single register back, so this
// example is verified against the emulated chip's own registers.
#include "msx-music.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 type = MSXMusic_Initialize();     // scans the slots for an OPLL

	R[1] = type;                         // 1 = internal, 2 = external (FM-PAC)
	R[2] = MSXMusic_GetSlotId();         // where it was found

	MSXMusic_SetRegister(0x30, 0x50);    // ch0: instrument 5 (piano), max volume
	MSXMusic_SetRegister(0x10, 0xAD);    // ch0: F-number low byte
	MSXMusic_SetRegister(0x20, 0x15);    // ch0: key-ON + octave + F-number high

	R[0] = (type != MSXMUSIC_NOTFOUND) ? 0xA5 : 0x00;
	for (;;) {}
}
