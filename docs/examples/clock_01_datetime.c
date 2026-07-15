// Example: set the RTC, then read it back with the date/time getters.
#include "clock.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	RTC_Initialize();          // select the time block (block 0)
	RTC_Set24H(TRUE);

	RTC_Initialize();          // Set24H switched to the alarm block — go back
	RTC_Write(RTC_REG_TIME_10HOUR, 1);   // each register is ONE BCD digit
	RTC_Write(RTC_REG_TIME_HOUR,   3);   // -> 13
	RTC_Write(RTC_REG_TIME_10MIN,  4);
	RTC_Write(RTC_REG_TIME_MIN,    5);   // -> 45

	R[1] = RTC_GetHour();      // 13
	R[2] = RTC_GetMinute();    // 45

	R[0] = (R[1] == 13 && R[2] == 45) ? 0xA5 : 0x00;
	for (;;) {}
}
