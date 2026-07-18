// bootdisk_01_sentinel.c — the smallest self-booting disk program.
//
// No MSX-DOS on the disk: the boot sector (lib/ext/bootsector.asm) loaded this
// straight to 0x0100 and jumped here, and crt0_bootdisk.asm ran the C startup.
// All this payload does is prove it got control — it writes a sentinel to RAM and
// spins, so a headless run can read the sentinel back.
#include "types.h"

volatile u8 __at(0xB000) R[8];

void main(void)
{
	R[0] = 0xA5;            // "I booted from a DOS-less disk"
	R[1] = 0x42;            // a second byte, to prove it is really us

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end        // spin so the sentinel stays put for the sampler
	__endasm;
}
