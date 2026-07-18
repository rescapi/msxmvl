// bootdisk_02_diskos.c — boot with no DOS, then pull a data blob off the disk.
//
// The mini-diskOS win: this program does NOT open a file, walk a FAT, or call
// MSX-DOS. It asks DiskOS_Load(id, dst) for blob id 1, which the build tool
// (tools/mkbootdsk.sh) already resolved to a fixed run of sectors — one raw
// sector read, no file system in the loop. The blob is a known byte pattern the
// build wrote to the disk, so we can verify it came back intact.
#include "diskos.h"
#include "types.h"

static u8 g_Buf[512];

volatile u8 __at(0xB000) R[8];

void main(void)
{
	u8  err;
	u16 len;

	// id 0 is this payload; id 1 is the resident data blob (see the manifest).
	len = DiskOS_Length(1);
	err = DiskOS_Load(1, g_Buf);

	R[1] = err;              // 0 = the sector read succeeded
	R[2] = (u8)len;          // blob length (low byte) straight from the index
	R[3] = g_Buf[0];         // 0xC0 ┐ the pattern the build put on the disk
	R[4] = g_Buf[1];         // 0xDE │
	R[5] = g_Buf[2];         // 0x01 ┘
	R[6] = g_Buf[3];         // 0x02

	R[0] = (err == 0 && len == 260
	     && g_Buf[0] == 0xC0 && g_Buf[1] == 0xDE
	     && g_Buf[2] == 0x01 && g_Buf[3] == 0x02) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
