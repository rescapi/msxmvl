// msxmvl EXTENSION module: moonblaster -- the MoonBlaster 1.4 replayer, optimised.
//
// Plays MoonBlaster 1.4 songs (.MBM) with their sample kits (.MBK) through the
// original mbplay replayer -- here in an interrupt-time-optimised build (mean
// interrupt cost -17%, worst-case -21% versus the original, measured over 25
// songs). The replayer source ships in this repo (moonblaster_player.asm); the
// pre-assembled binary in moonblaster_bin.h is byte-verified against the build
// that passed a 25-song chip-state-equivalence regression suite.
//
// Provenance: mbplay and MoonBlaster are by this library's author (Moonsoft);
// original source, optimised and relicensed here under the repo's MIT license.
//
// THE MEMORY CONTRACT (read this before using the module):
//   - MoonBlaster_Init copies the replayer to its fixed home 0xB000-0xC50F.
//     That region is the replayer's -- code, data tables, channel state. Your
//     program must not load code or data there, and under MSX-DOS your .COM
//     (which loads at 0x0100) must end below 0xB000.
//   - The .MBM song must stay resident in the visible 64 KB address space the
//     whole time it plays: the replayer reads pattern data from it inside the
//     interrupt handler. If you keep songs in RAM-mapper segments, the song's
//     segment must be mapped in whenever interrupts are enabled (this build is
//     the resident-song configuration: it performs no mapper switching itself).
//   - Playback is driven by the BIOS 50/60 Hz interrupt through the H.TIMI hook
//     (0xFD9F). MoonBlaster_Start installs the hook (chaining to the previous
//     owner); MoonBlaster_Stop removes it.
//   - Under MSX-DOS, page 0 is RAM, so the BIOS MSX-version byte the replayer
//     reads at 0x002D contains garbage. Write the real value (or 0 for plain
//     Z80 machines) before MoonBlaster_Start:  *(u8*)0x002D = 0;
//     In ROM programs page 0 is the BIOS itself and nothing needs doing.
//   - turboR: the replayer self-patches for the R800's faster I/O; no host work.
//
// Sound hardware: the song's chip mode is chosen at Start. MSX-Audio (Y8950 /
// Philips Music Module) plays melodic FM + ADPCM sample drums; MSX-Music
// (OPLL / FM-PAC) plays melodic FM + PSG drums; stereo uses both at once.
// Sample drums need the song's kit uploaded first -- MoonBlaster_LoadKit.
// LoadKit is the one function that needs another module: link msx-audio.c.
#ifndef MSXMVL_EXT_MOONBLASTER_H
#define MSXMVL_EXT_MOONBLASTER_H

#include "types.h"

// chip modes for MoonBlaster_Start (what the song was composed for)
#define MOONBLASTER_MSXAUDIO  0   // Y8950: FM + ADPCM sample drums
#define MOONBLASTER_MSXMUSIC  1   // OPLL:  FM + PSG drums
#define MOONBLASTER_STEREO    2   // both chips at once (MoonBlaster's stereo mode)

// .MBK sample-kit layout: 56-byte block table, then raw Y8950 ADPCM data
#define MOONBLASTER_KIT_HEADER  56u
#define MOONBLASTER_KIT_FULL    32768u

// Function: MoonBlaster_Init
// Copy the replayer to its fixed home at 0xB000. Call once, before anything else.
void MoonBlaster_Init(void);

// Function: MoonBlaster_LoadKit
// Upload a song's sample kit (.MBK file contents) into the Y8950's sample RAM.
// Needed only for songs with ADPCM drums (MSX-Audio / stereo modes). Requires a
// Y8950 (check with MSXAudio_Detect) and links against msx-audio.c.
//
// Parameters:
//   mbk     - the .MBK file in memory (block table + ADPCM data)
//   dataLen - ADPCM bytes to upload; 0 = the full 32 KB kit
void MoonBlaster_LoadKit(const u8* mbk, u16 dataLen);

// Function: MoonBlaster_Start
// Start a song: stops any current one, selects the chip mode, installs the
// H.TIMI hook and begins playback on the next BIOS interrupt.
//
// Parameters:
//   song  - the .MBM file in memory (must stay resident while playing)
//   chips - MOONBLASTER_MSXAUDIO / MOONBLASTER_MSXMUSIC / MOONBLASTER_STEREO
void MoonBlaster_Start(const void* song, u8 chips);

// Function: MoonBlaster_Stop
// Stop playback: silences every channel and removes the H.TIMI hook.
// (This is the original stpmus/hltmus entry -- stop and pause are the same
// operation; MoonBlaster_Continue picks the song up where it stopped.)
void MoonBlaster_Stop(void);

// Function: MoonBlaster_Continue
// Resume a stopped/paused song where it left off (original cntmus entry).
void MoonBlaster_Continue(void);

// Function: MoonBlaster_Fade
// Begin a fade-out. The song keeps playing, stepping all volumes down each
// interrupt until silent. speed: 1 = fastest, 255 = slowest.
void MoonBlaster_Fade(u8 speed);

// Function: MoonBlaster_IsPlaying
// TRUE (255) while a song plays, 0 after it ends or is stopped.
u8 MoonBlaster_IsPlaying(void);

// Function: MoonBlaster_Position
// Current position in the song's position list (advances once per pattern).
u8 MoonBlaster_Position(void);

// Function: MoonBlaster_Step
// Current step within the playing pattern.
u8 MoonBlaster_Step(void);

// Function: MoonBlaster_Tick
// Advance playback by exactly one interrupt's worth, without the BIOS: masks
// interrupts (DI) and calls the H.TIMI hook directly. Interrupts STAY masked on
// return so a run of ticks is deterministic -- issue EI yourself afterwards to
// hand playback back to the BIOS interrupt. Useful for fast-forward, for
// rendering, and for driving music from a context that owns the interrupt.
void MoonBlaster_Tick(void);

#endif // MSXMVL_EXT_MOONBLASTER_H
