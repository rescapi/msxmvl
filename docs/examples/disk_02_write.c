// Example: write a raw sector and read it straight back.
// The harness runs on a scratch COPY of the disk, so scribbling is safe.
#include "disk.h"
volatile u8 __at(0xB000) R[8];

static u8 g_Out[512];
static u8 g_In[512];

#define SCRATCH_SECTOR  700     // near the end of a 720-sector disk

void main(void)
{
	u8 werr, rerr;
	u16 i;

	for (i = 0; i < 512; i++)
		g_Out[i] = (u8)i;                            // 00 01 02 ... FF 00 01 ...

	Disk_SetDrive(0);
	werr = DiskDOS_WriteSectors(SCRATCH_SECTOR, 1, g_Out);
	rerr = DiskDOS_ReadSectors(SCRATCH_SECTOR, 1, g_In);

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
