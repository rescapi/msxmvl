// Example: MSX-DOS 2 file I/O — create, write, close, reopen, read back.
// Built as a .COM and run under real MSX-DOS 2. Results land at 0xB000 (inside
// the TPA, below the DOS 2 kernel), not 0xE000 as in the ROM examples.
#include "dos.h"
volatile u8 __at(0xB000) R[8];

static const c8 g_Path[] = "DOCS.DAT";
static u8 g_Back[4];

void main(void)
{
	u8 h;
	u16 wr, rd;

	h  = DOS_CreateHandle(g_Path, O_RDWR, 0);   // 0 = normal attributes
	wr = DOS_WriteHandle(h, "\xDE\xAD\xBE\xEF", 4);
	DOS_EnsureHandle(h);                        // flush to disk BEFORE closing
	DOS_CloseHandle(h);

	h  = DOS_OpenHandle(g_Path, O_RDONLY);      // reopen and read it back
	rd = DOS_ReadHandle(h, g_Back, 4);
	DOS_CloseHandle(h);
	DOS_Delete(g_Path);                         // tidy up after ourselves

	R[1] = (u8)wr;         // 4 bytes written
	R[2] = (u8)rd;         // 4 bytes read
	R[3] = g_Back[0];      // 0xDE
	R[4] = g_Back[3];      // 0xEF

	R[0] = (wr == 4 && rd == 4 && g_Back[0] == 0xDE && g_Back[3] == 0xEF)
	     ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
