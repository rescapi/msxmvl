// The PSG (AY-3-8910) makes sound via 14 registers. High-level setters write them
// for you: PSG_SetTone(channel, period) sets a channel's pitch (12-bit period ->
// registers 0/1 for channel A), PSG_SetVolume(channel, vol) its 0..15 level
// (register 8). PSG_GetRegister reads any register back.
#include "psg.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	PSG_SetTone(0, 0x123);         // channel A period 0x123 -> reg0=0x23, reg1=0x01
	PSG_SetVolume(0, 12);          // channel A volume -> reg8 = 12

	R[1] = PSG_GetRegister(0);     // 0x23 (fine period)
	R[2] = PSG_GetRegister(1);     // 0x01 (coarse period)
	R[3] = PSG_GetRegister(8);     // 12   (volume)
	R[0] = (R[1] == 0x23 && R[2] == 0x01 && R[3] == 12) ? 0xA5 : 0x00;
	for (;;) {}
}
