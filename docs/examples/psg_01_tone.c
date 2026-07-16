// sfx.c — one-shot sound effects on the PSG (a coin pickup, a jump blip).
//
// A sound effect is just a tone at a volume on one of the PSG's three channels.
// PSG_SetTone picks the pitch (a 12-bit period — smaller is higher), PSG_SetVolume
// picks how loud (0..15). Point them at a channel you keep free for effects and you
// have an arcade blip. (PSG_GetRegister can read any register back.)
#include "psg.h"

#define SFX_CHANNEL   0        // channel A, kept free for sound effects

// Start a sound effect: a tone at a given pitch and volume on the effects channel.
void sfx_play(u16 period, u8 volume)
{
	PSG_SetTone(SFX_CHANNEL, period);
	PSG_SetVolume(SFX_CHANNEL, volume);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	sfx_play(0x123, 12);           // a blip: channel A, period 0x123, volume 12

	R[1] = PSG_GetRegister(0);     // 0x23 (fine period)
	R[2] = PSG_GetRegister(1);     // 0x01 (coarse period)
	R[3] = PSG_GetRegister(8);     // 12   (volume)
	R[0] = (R[1] == 0x23 && R[2] == 0x01 && R[3] == 12) ? 0xA5 : 0x00;
	for (;;) {}
}
