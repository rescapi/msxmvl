// gameclock.c — seed the in-game clock from the MSX real-time clock.
//
// The RTC exposes its time as separate 4-bit BCD digits, one register each. One
// gotcha: RTC_Set24H lives in the *alarm* block, so it leaves that block selected —
// you must RTC_Initialize back to the time block before writing the time.
#include "clock.h"

// Set the in-game clock to 13:45 in 24-hour mode.
void set_game_time(void)
{
	RTC_Initialize();          // select the time block (block 0)
	RTC_Set24H(TRUE);

	RTC_Initialize();          // Set24H switched to the alarm block — go back
	RTC_Write(RTC_REG_TIME_10HOUR, 1);   // each register is ONE BCD digit
	RTC_Write(RTC_REG_TIME_HOUR,   3);   // -> 13
	RTC_Write(RTC_REG_TIME_10MIN,  4);
	RTC_Write(RTC_REG_TIME_MIN,    5);   // -> 45
}

u8 game_hour(void)   { return RTC_GetHour(); }
u8 game_minute(void) { return RTC_GetMinute(); }

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	set_game_time();

	R[1] = game_hour();        // 13
	R[2] = game_minute();      // 45

	R[0] = (R[1] == 13 && R[2] == 45) ? 0xA5 : 0x00;
	for (;;) {}
}
