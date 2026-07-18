// pacpages_msxgl.c — the MSXgl-style PAC surface: 8 pages of 1 KB, signed saves.
//
// A PAC is SHARED between games, so MSXgl carves it into 8 one-KB pages and marks
// ownership with a 4-byte app signature (build with -DAPPSIGN='"MVZ8"'). PAC_Check
// tells you whose data a page holds BEFORE you touch it: your own (PAC_CHECK_APP),
// nobody's (PAC_CHECK_EMPTY), or someone else's (PAC_CHECK_UNDEF — leave it alone).
#include "pac.h"

// Find every PAC in the machine and select the first. FALSE = no PAC anywhere.
bool pac_boot(void)
{
	return PAC_Initialize();
}

// Claim page 2 for this game: erase it, then write a signed record.
void pac_save(const u8* data, u16 size)
{
	PAC_Format(2);                     // page 2 -> all 0xFF (erases any signature)
	PAC_Write(2, data, size);          // writes APPSIGN, then the data after it
}

// Whose data is in page 2 right now? PAC_CHECK_APP once pac_save has run.
u8 pac_state(void)
{
	return PAC_Check(2);
}

// Read the record back (the signature bytes are skipped, mirroring PAC_Write).
void pac_load(u8* data, u16 size)
{
	PAC_Read(2, data, size);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

static const u8 g_Rec[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
static u8 g_Buf[6];

void main(void)
{
	u8 found, empty_after_format, app_after_write, roundtrip, i;

	found = pac_boot() ? 1 : 0;

	PAC_Format(2);
	empty_after_format = (pac_state() == PAC_CHECK_EMPTY) ? 1 : 0;

	pac_save(g_Rec, 6);
	app_after_write = (pac_state() == PAC_CHECK_APP) ? 1 : 0;

	g_Buf[0] = 0;
	pac_load(g_Buf, 6);
	roundtrip = 1;
	for (i = 0; i < 6; ++i)
		if (g_Buf[i] != g_Rec[i])
			roundtrip = 0;

	R[1] = found;
	R[2] = g_PAC_Num;                  // how many PACs the scan found
	R[3] = empty_after_format;
	R[4] = app_after_write;
	R[5] = roundtrip;
	R[6] = PAC_Check(9);               // validator: bad page -> PAC_CHECK_ERROR (2)

	R[0] = (found && empty_after_format && app_after_write && roundtrip &&
	        R[6] == PAC_CHECK_ERROR) ? 0xA5 : 0x00;
	for (;;) {}
}
