// hiscore_cmos.c — keep a high score in battery-backed CMOS, no disk needed.
//
// Block 3 of the RTC is free CMOS RAM kept alive by the machine's battery.
// RTC_SaveData writes 6 bytes there and RTC_LoadData reads them back, returning FALSE
// if the block has never held a valid save. Six bytes is the whole budget — enough
// for a high score and a level number, say — and it survives a power-off with no disk.
#include "clock.h"

// Persist 6 bytes of save data to CMOS block 3.
void hiscore_store(u8* data)
{
	RTC_SaveData(data);                // -> CMOS block 3
}

// Read the 6 bytes back; FALSE if the block holds no valid save.
bool hiscore_fetch(u8* data)
{
	return RTC_LoadData(data);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

static u8 g_Save[6] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC };
static u8 g_Back[6];

void main(void)
{
	u8 i, ok = 1;

	hiscore_store(g_Save);             // -> CMOS block 3
	ok = hiscore_fetch(g_Back) ? 1 : 0; // FALSE if the block holds no valid save

	for (i = 0; i < 6; i++)
		if (g_Back[i] != g_Save[i])
			ok = 0;

	R[1] = g_Back[0];    // 0x12
	R[2] = g_Back[5];    // 0xBC

	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
