// sfx_over_music.c -- a sound effect layered on top of music, sharing one PSG.
//
// ayFX mixes priority-ranked effects into msxmvl's PSG shadow (g_PSG_Regs): tone,
// volume and noise land in the same registers your music writes, and the single
// PSG_Apply() you already call each frame flushes the merged result. Here we set up
// "music" on channel A, fire an effect on channel C, and check that the effect plays
// AND the music is left untouched -- deterministically, so it self-verifies.
#include "psg.h"
#include "ayfx.h"

// A minimal hand-built ayFX bank: 1 effect, a channel-C tone at falling volume, then
// the end marker. Layout: [count][offset table][streams]; a stream frame is a control
// byte plus its 2 tone bytes. Control-byte bits: bit5=new tone follows, bit7=noise
// off, low nibble=volume -- which for channel C ALSO gates the tone-period write via
// bit2 (a quirk of the format: the tone must be written on a frame whose volume has
// bit2 clear, then it persists in the shadow). So we use volumes 11 (0x0B) and 8
// (0x08), both bit2-clear. The {0x40,0x20} frame (bit6=new noise, value 0x20) ends it.
static const u8 g_Bank[] = {
	1,                      // 1 effect in the bank
	1, 0,                   // offset table[0]: increment 1 -> stream starts at bank+3
	0xAB, 0x2C, 0x01,       // frame: volume 11, tone 0x012C (300) on the play channel
	0xA8, 0x2C, 0x01,       // frame: volume 8, tone 0x012C
	0x40, 0x20,             // end of stream
};

// ---- test harness --------------------------------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 ok, ret;

	// "music": a tone on channel A (period 0x100, volume 12, tone A enabled).
	PSG_SetTone(PSG_CHANNEL_A, 0x0100);
	PSG_SetVolume(PSG_CHANNEL_A, 12);
	PSG_SetMixer(PSG_TONE_A_ON);            // active-high: bit0 set

	// arm the SFX layer on channel C and start the effect (top priority band = 15).
	ayFX_Setup((u16)g_Bank, ayFX_FIXED, PSG_CHANNEL_C);
	ret  = ayFX_Play(0, 15);                // -> ayFX_OK (0)

	ayFX_Decode();                          // frame 1: volume 11 on channel C
	R[1] = g_PSG_Regs.Volume[2];            // 11
	R[2] = g_PSG_Regs.Mixer;                // tone A (0x01) + tone C (0x04) = 0x05
	ayFX_Decode();                          // frame 2: volume 8
	R[3] = g_PSG_Regs.Volume[2];            // 8

	// music must be untouched by the SFX mix
	R[4] = g_PSG_Regs.Volume[0];            // still 12
	R[5] = ret;                             // ayFX_OK == 0
	R[6] = (g_PSG_Regs.Tone[2] == 300) ? 1 : 0;   // channel C tone set

	ok = (R[1] == 11 && R[3] == 8 &&
	      (R[2] & PSG_TONE_A_ON) && (R[2] & PSG_TONE_C_ON) &&   // both channels enabled
	      R[4] == 12 && g_PSG_Regs.Tone[0] == 0x0100 &&         // music undisturbed
	      R[5] == ayFX_OK && R[6] == 1);
	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
