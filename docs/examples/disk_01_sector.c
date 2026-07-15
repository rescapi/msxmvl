// Example: raw sector I/O — read the boot sector with NO file system involved.
// No FAT, no directory, no filename: just "give me sector 0".
#include "disk.h"
volatile u8 __at(0xB000) R[8];

static u8 g_Sector[512];

void main(void)
{
	u8 err;

	Disk_SetDrive(0);                             // A:
	err = DiskDOS_ReadSectors(0, 1, g_Sector);    // sector 0 = the boot sector

	R[1] = err;                                   // 0 = success
	R[2] = g_Sector[0];                           // 0xEB ) the boot sector's
	R[3] = g_Sector[1];                           // 0xFE ) jump instruction
	R[4] = g_Sector[2];                           // 0x90
	R[5] = g_Sector[0x15];                        // media descriptor byte

	R[0] = (err == 0 && g_Sector[0] == 0xEB && g_Sector[1] == 0xFE
	     && g_Sector[2] == 0x90) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
