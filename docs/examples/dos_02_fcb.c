// savegame.c — save & load a record through an MSX-DOS *1* File Control Block.
//
// The CP/M-style file API: an FCB you own and DOS scribbles in, with data moving
// through a DTA (disk transfer address) rather than a buffer argument. Older and
// clunkier than the DOS 2 file handles, but it runs on DOS 1 *and* DOS 2 — so a
// game that needs one binary for every machine saves this way.
#include "dos.h"

static DOS_FCB g_Fcb;
static u8      g_Dta[128];          // the disk-transfer buffer: one 128-byte record

// FCB names are 8.3, SPACE-padded, with no dot: "DOCSFCB DAT".
// Zero the whole FCB, then set only Drive and Name — DOS fills in the rest.
static void savegame_setup_fcb(void)
{
	const c8* n = "DOCSFCB DAT";
	u8 i;
	for (i = 0; i < sizeof(g_Fcb); i++)
		((u8*)&g_Fcb)[i] = 0;       // an FCB must start out zeroed
	g_Fcb.Drive = 0;                // 0 = current drive
	for (i = 0; i < 11; i++)
		g_Fcb.Name[i] = n[i];
}

// Save: create the file and write the record now sitting in the DTA. -> 0 on success.
u8 savegame_write(void)
{
	u8 cr, wr, cl;
	savegame_setup_fcb();
	DOS_SetTransferAddr(g_Dta);     // FCB calls transfer through the DTA
	cr = DOS_CreateFCB(&g_Fcb);     // leaves RecordSize = 0, i.e. 128-byte records
	wr = DOS_SequentialWriteFCB(&g_Fcb);
	cl = DOS_CloseFCB(&g_Fcb);
	return cr | wr | cl;
}

// Load: open the file and read the record back into the DTA. -> 0 on success.
u8 savegame_read(void)
{
	u8 op, rd;
	savegame_setup_fcb();           // a fresh, zeroed FCB for the read
	DOS_SetTransferAddr(g_Dta);
	op = DOS_OpenFCB(&g_Fcb);
	rd = DOS_SequentialReadFCB(&g_Fcb);
	return op | rd;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xB000) R[8];
void main(void)
{
	u8 w, r, i;

	for (i = 0; i < 128; i++) g_Dta[i] = 0;
	g_Dta[0] = 0xC0; g_Dta[1] = 0xDE;   // the two marker bytes we save
	w = savegame_write();

	for (i = 0; i < 128; i++) g_Dta[i] = 0;   // wipe, so a real read must refill it
	r = savegame_read();

	R[1] = w; R[2] = r; R[3] = 0; R[4] = 0;
	R[5] = g_Dta[0];                // 0xC0 — came back off the disk
	R[6] = g_Dta[1];                // 0xDE

	R[0] = (w == 0 && r == 0 && g_Dta[0] == 0xC0 && g_Dta[1] == 0xDE) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
