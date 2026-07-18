// mbsong.c — play a MoonBlaster 1.4 song, sample drums and all, through the
// optimised mbplay replayer.
//
// The song is "Example Song" by C.v/d Geest from the original MoonBlaster 1.4
// disk (assets/EXAMP1.MBM), embedded as a C array with the first kilobyte of its
// sample kit (assets/EXAMPLE.MBK). A real program would load both files from
// disk and pass 0 (= the full 32 KB) to MoonBlaster_LoadKit.
#include "moonblaster.h"
#include "msx-audio.h"
#include "moonblaster_song.h"    // MB_SONG_EXAMP1[]
#include "moonblaster_kit.h"     // MB_KIT_EXAMPLE_FRAG[], MB_KIT_FRAG_LEN

// Bring up the replayer and start the song (MSX-Audio mode: FM + sample drums).
void start_song(void)
{
	MoonBlaster_Init();                                          // replayer -> 0xB000
	MoonBlaster_LoadKit(MB_KIT_EXAMPLE_FRAG, MB_KIT_FRAG_LEN);   // drums -> sample RAM
	MoonBlaster_Start(MB_SONG_EXAMP1, MOONBLASTER_MSXAUDIO);     // H.TIMI hook + play
}

// Fast-forward two patterns' worth of interrupts, deterministically: each tick
// is one 50 Hz interrupt of playback. (Normally the BIOS delivers these and the
// song just plays -- ticking by hand is for fast-forward and paused contexts.)
void fast_forward(void)
{
	u16 i;
	for (i = 0; i < 400; ++i) MoonBlaster_Tick();
}

// ---- test harness (not shown in the docs) --------------------------------
// Harness contract: the runner reads 8 bytes at 0xE000 and byte 0 == 0xA5 means
// every check passed. A named struct instead of the usual R[] array — the same
// byte layout, but each result is readable at the point it is recorded.
struct test_results {
	u8 verdict;              // 0xA5 when every check below passed
	u8 playing_after_start;  // 255: IsPlaying immediately after Start
	u8 step_after_ff;        // 12: deterministic — (400 ticks / tempo 9) mod 16
	u8 step_after_bios;      // moved on with no Tick => the BIOS drove H.TIMI
	u8 kit_ok;               // 1: kit bytes read back from Y8950 sample RAM
	u8 playing_after_stop;   // 0: Stop silences and unhooks
	u8 y8950_found;          // 1: the audio hardware is present
	u8 playing_after_ff;     // 255: fast-forwarding does not end the song
};
volatile struct test_results __at(0xE000) test;

void main(void)
{
	u8 kit_readback[16];
	u8 kit_ok, i;
	volatile u16 wait;
	bool y8950_found;

	MSXAudio_Initialize();
	y8950_found = MSXAudio_Detect();

	MoonBlaster_Init();
	MoonBlaster_LoadKit(MB_KIT_EXAMPLE_FRAG, MB_KIT_FRAG_LEN);

	// the kit really landed in Y8950 sample RAM (checked before Start so the
	// readback cannot race the replayer's own ADPCM drum commands)
	MSXAudio_ADPCM_ReadMemory(0, kit_readback, 16);
	kit_ok = 1;
	for (i = 0; i < 16; ++i)
		if (kit_readback[i] != MB_KIT_EXAMPLE_FRAG[MOONBLASTER_KIT_HEADER + i])
			kit_ok = 0;

	MoonBlaster_Start(MB_SONG_EXAMP1, MOONBLASTER_MSXAUDIO);
	test.playing_after_start = MoonBlaster_IsPlaying();

	fast_forward();                      // leaves interrupts masked (see header)
	test.step_after_ff = MoonBlaster_Step();
	test.playing_after_ff = MoonBlaster_IsPlaying();

	// hand playback back to the BIOS: the 50 Hz interrupt drives H.TIMI itself
	__asm ei __endasm;
	for (wait = 0; wait < 60000; ++wait) ;
	test.step_after_bios = MoonBlaster_Step();

	MoonBlaster_Stop();
	test.playing_after_stop = MoonBlaster_IsPlaying();

	test.kit_ok = kit_ok;
	test.y8950_found = y8950_found ? 1 : 0;

	test.verdict = (y8950_found
	          && test.playing_after_start == 255
	          && test.step_after_ff != 0
	          && test.step_after_bios != test.step_after_ff
	          && test.playing_after_ff == 255
	          && kit_ok
	          && test.playing_after_stop == 0) ? 0xA5 : 0x00;

	__asm
		.globl _mark_end
	_mark_end:
		jr _mark_end
	__endasm;
}
