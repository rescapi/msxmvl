// diskload.c — read a raw 512-byte sector straight off the disk, no file system.
//
// The layer below MSX-DOS files: no FAT, no directory, no filename — just
// "give me sector N". A fast custom loader pulls a level (or, here, the boot
// sector) directly, without paying for a file system to find it.
#include "disk.h"

static u8 g_Sector[512];    // one sector = 512 bytes

// Read one sector from drive A: into g_Sector. Sector 0 is the boot sector,
// which begins with the jump EB FE 90 and carries the media byte at 0x15.
u8 load_sector(u16 sector)
{
	Disk_SetDrive(0);                             // A:
	return DiskDOS_ReadSectors(sector, 1, g_Sector);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xB000) R[8];
void main(void)
{
	u8 err = load_sector(0);                      // sector 0 = the boot sector

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
