// Example: MSX-DOS *1* file I/O with an FCB. Works on DOS 1 AND DOS 2 --
// unlike the file-handle API (dos_01_file.c), which is DOS 2 only.
#include "dos.h"
volatile u8 __at(0xB000) R[8];

static DOS_FCB g_Fcb;
static u8      g_Dta[128];          // one FCB record

// FCB names are 8.3, SPACE-padded, with no dot: "DOCSFCB DAT"
static void SetupFcb(void)
{
	const c8* n = "DOCSFCB DAT";
	u8 i;
	for (i = 0; i < sizeof(g_Fcb); i++)
		((u8*)&g_Fcb)[i] = 0;       // an FCB must start out zeroed
	g_Fcb.Drive = 0;                // 0 = current drive
	for (i = 0; i < 11; i++)
		g_Fcb.Name[i] = n[i];
}

void main(void)
{
	u8 cr, wr, cl, op, rd, i;

	SetupFcb();
	for (i = 0; i < 128; i++) g_Dta[i] = 0;
	g_Dta[0] = 0xC0; g_Dta[1] = 0xDE;

	DOS_SetTransferAddr(g_Dta);     // FCB calls transfer through the DTA
	cr = DOS_CreateFCB(&g_Fcb);     // leaves RecordSize = 0, i.e. 128-byte records
	wr = DOS_SequentialWriteFCB(&g_Fcb);
	cl = DOS_CloseFCB(&g_Fcb);

	SetupFcb();                     // a fresh, zeroed FCB for the read
	for (i = 0; i < 128; i++) g_Dta[i] = 0;
	DOS_SetTransferAddr(g_Dta);
	op = DOS_OpenFCB(&g_Fcb);
	rd = DOS_SequentialReadFCB(&g_Fcb);

	R[1] = cr; R[2] = wr; R[3] = op; R[4] = rd;
	R[5] = g_Dta[0];                // 0xC0 — came back off the disk
	R[6] = g_Dta[1];                // 0xDE

	R[0] = (cr == 0 && wr == 0 && cl == 0 && op == 0 && rd == 0
	     && g_Dta[0] == 0xC0 && g_Dta[1] == 0xDE) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
