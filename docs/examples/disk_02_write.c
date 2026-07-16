// fastsave.c — a custom save format: write the save block straight to a raw sector.
//
// No file system to update, no directory to grow, no FAT to walk — the fastest
// possible save. You pick a scratch sector, write your 512-byte block to it, and
// read it straight back. There is no safety net: the write goes to the medium as-is.
#include "disk.h"

#define SAVE_SECTOR  700    // a scratch sector near the end of a 720-sector disk

static u8 g_Out[512];       // the block we save
static u8 g_In[512];        // ...and read back to verify

// Save: write one 512-byte block straight to SAVE_SECTOR. -> 0 on success.
u8 fastsave_write(void)
{
	Disk_SetDrive(0);
	return DiskDOS_WriteSectors(SAVE_SECTOR, 1, g_Out);
}

// Load: read the block straight back from SAVE_SECTOR. -> 0 on success.
u8 fastsave_read(void)
{
	Disk_SetDrive(0);
	return DiskDOS_ReadSectors(SAVE_SECTOR, 1, g_In);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xB000) R[8];
void main(void)
{
	u8 werr, rerr;
	u16 i;

	for (i = 0; i < 512; i++)
		g_Out[i] = (u8)i;                            // 00 01 02 ... FF 00 01 ...

	werr = fastsave_write();
	rerr = fastsave_read();

	R[1] = werr;        // 0
	R[2] = rerr;        // 0
	R[3] = g_In[0];     // 0x00
	R[4] = g_In[1];     // 0x01
	R[5] = g_In[255];   // 0xFF
	R[6] = g_In[511];   // 0xFF

	R[0] = (werr == 0 && rerr == 0 && g_In[1] == 0x01
	     && g_In[255] == 0xFF && g_In[511] == 0xFF) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
