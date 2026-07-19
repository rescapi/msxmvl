// play_mwm.c -- play a MoonBlaster MoonSound Wave (.MWM) song via moonblaster_wave.
//
// Needs a MoonSound (OPL4) and runs under MSX-DOS (all-RAM: the 16 KB replayer is
// relocated to 0x4000, the song must be resident above it). Build/run it yourself:
//   ./run_example_dos.sh moonblaster_wave_01.c "moonblaster_wave.c" a5 8   (add -ext moonsound)
// It is NOT in the CI sweep: openMSX's MoonSound needs the (copyrighted) YRW801 wave ROM
// and MSX-DOS needs a boot disk, neither of which ships. The replayer is verified playing
// in experiments/mbwave-irqtime; this checks the C module drives it.
#include "moonblaster_wave.h"
#include "moonblaster_wave_song.h"

#define SONG_HOME ((u8*)0x8000)   // song lives above the replayer, stays resident

volatile u8 __at(0xB000) R[8];   // the DOS harness reads results from 0xB000
void main(void)
{
	const void* song = (void*)SONG_HOME;
	u8 ok;
	u16 j;

	// The song must survive out of this program's image, which the replayer's copy
	// (to 0x4000-0x7E8D) overwrites. Relocate it to 0x8000 first, then Init.
	for (j = 0; j < MBWAVE_SONG_LEN; j++)      // full 16-bit length (song is >255 B)
		SONG_HOME[j] = MBWAVE_SONG_EXAMPLE[j];

	*(volatile u8*)0x002D = 0;      // DOS: page 0 is RAM, set the MSX-version byte

	R[3] = SONG_HOME[0];            // 'M'  -- song relocated (the "MBMS" header)
	R[4] = SONG_HOME[3];            // 'S'

	MoonBlasterWave_Init();
	R[1] = *(volatile u8*)0x4000;   // 'A'  -- replayer relocated to 0x4000 ("AB" id)
	R[2] = *(volatile u8*)0x4001;   // 'B'
	R[5] = *(volatile u8*)0x4669;   // play_int present in RAM (0xF3 = DI, its 1st opcode)

	// Start inits the OPL4 + song and marks it playing (no BIOS hook -- see the module
	// header; playback is caller-driven). play_busy (0xDA00) is nonzero right after, with
	// no interrupt race. Then drive a couple of frames with Tick (di + call play_int).
	MoonBlasterWave_Start(song);
	R[6] = *(volatile u8*)0xDA00;   // play_busy: nonzero = start_music ran + playing
	MoonBlasterWave_Tick();
	MoonBlasterWave_Tick();
	MoonBlasterWave_Stop();

	// gate on the deterministic setup (copy-to-RAM + relocation) AND that Start started it
	ok = (R[1] == 'A' && R[2] == 'B' && R[3] == 'M' && R[4] == 'S' && R[5] == 0xF3 && R[6] != 0);
	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
