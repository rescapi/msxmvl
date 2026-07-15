// Example: RTC_SaveData / RTC_LoadData — 6 bytes of battery-backed CMOS.
// This survives a power-off with no disk at all: the classic MSX high-score save.
#include "clock.h"
volatile u8 __at(0xE000) R[8];

static u8 g_Save[6] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC };
static u8 g_Back[6];

void main(void)
{
	u8 i, ok = 1;

	RTC_SaveData(g_Save);              // -> CMOS block 3
	ok = RTC_LoadData(g_Back) ? 1 : 0; // FALSE if the block holds no valid save

	for (i = 0; i < 6; i++)
		if (g_Back[i] != g_Save[i])
			ok = 0;

	R[1] = g_Back[0];    // 0x12
	R[2] = g_Back[5];    // 0xBC

	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
