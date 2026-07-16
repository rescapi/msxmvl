// savegame.c — save the game to disk and load it back, MSX-DOS 2 file handles.
//
// The modern file API: create/open a file, write to it, flush, close, reopen, read.
// Full paths and real error codes, the way any other OS does it — but MSX-DOS 2 only.
// A save file lives on the game disk; these two calls are the whole save/load path.
#include "dos.h"

#define SAVE_PATH  "DOCS.DAT"   // the save file on the game disk

// Save the game: create the file, write the state out, flush it, close.
u16 savegame_write(const void* state, u16 size)
{
	u8  h  = DOS_CreateHandle(SAVE_PATH, O_RDWR, 0);   // 0 = normal attributes
	u16 wr = DOS_WriteHandle(h, state, size);
	DOS_EnsureHandle(h);                               // flush to disk BEFORE closing
	DOS_CloseHandle(h);
	return wr;
}

// Load the game: reopen the save file read-only, read the state back, close.
u16 savegame_read(void* out, u16 size)
{
	u8  h  = DOS_OpenHandle(SAVE_PATH, O_RDONLY);
	u16 rd = DOS_ReadHandle(h, out, size);
	DOS_CloseHandle(h);
	return rd;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xB000) R[8];
static const u8 g_State[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
static u8       g_Back[4];
void main(void)
{
	u16 wr = savegame_write(g_State, 4);
	u16 rd = savegame_read(g_Back, 4);
	DOS_Delete(SAVE_PATH);                      // tidy up after ourselves

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
