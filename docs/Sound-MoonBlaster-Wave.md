# Sound (MoonBlaster Wave) (`moonblaster_wave`)

Play **MoonBlaster for MoonSound Wave** songs (`.MWM`) — OPL4 / YMF278B wavetable music —
on a MoonSound cartridge, through the original MBWave 1.01 replayer. The companion to the
[MoonBlaster 1.4](Sound-MoonBlaster.md) module, for the MoonSound instead of FM-PAC/
MSX-Audio. Link `moonblaster_wave.c`, include `moonblaster_wave.h`.

> **Provenance & framing:** MBWave and its replayer are by this library's author
> (Moonsoft), relicensed MIT (see `NOTICE.md`). Unlike the 1.4 module, this replayer is
> shipped **as-is** — it is already efficient (measured **~190–4514 T-states/interrupt**
> across the 32-song Jer Der Wave corpus; a 60 Hz frame is ~59 000 T), so there is no
> average-case slack to cut. The msxmvl contribution is integration + a C API +
> measurement — **match + integrate**, like the trackers and ayFX, not a T-state beat.

## API

```c
#include "moonblaster_wave.h"

void MoonBlasterWave_Init(void);              // relocate the replayer into RAM (once, first)
void MoonBlasterWave_Start(const void* song); // point at a .MWM, init OPL4 + song (no hook)
void MoonBlasterWave_Stop(void);              // stop/pause + silence
void MoonBlasterWave_Continue(void);          // resume
u8   MoonBlasterWave_IsPlaying(void);
u8   MoonBlasterWave_Position(void);
u8   MoonBlasterWave_Step(void);
void MoonBlasterWave_Tick(void);              // advance one interrupt's worth (you drive it)
```

## You drive playback with `Tick`

The replayer does **not** install a BIOS interrupt hook. Its own hook is an *interslot*
`RST 30h`, which needs the BIOS in page 0 — true in a ROM game, but not under MSX-DOS (page
0 is RAM), where it faults. So `Start` does init only, and **you** call
`MoonBlasterWave_Tick()` once per frame (it `DI`s, runs one replay pass, and returns). This
is reliable on DOS and ROM alike. A ROM game that wants fire-and-forget interrupt playback
can install its own H.TIMI hook that simply calls `MoonBlasterWave_Tick`.

## The copy-to-RAM contract (important)

The MBWave replayer is **16 KB** and is relocated by `MoonBlasterWave_Init` to its home
**0x4000–0x7E8D**, with RW workspace at **0xDA00** — exactly how a ROM game ships it (the
cart's boot code copies the replayer into RAM). Under MSX-DOS a `.COM` image already extends
*past* 0x4000, so the module keeps all of its own wrapper code **below** 0x4000 and copies
the blob overlap-safely; Init never overwrites the functions that drive it. (This is why the
replayer blob is defined at the very bottom of `moonblaster_wave.c` — leave it there.)

- **Song stays resident.** This is the resident-song build (mapper switches neutralised), so
  keep the `.MWM` in the visible 64 KB while it plays. `Start` skips the 6-byte `"MBMS"`
  file header for you.
- **Under MSX-DOS** set the MSX-version byte first: `*(u8*)0x002D = 0;` (page 0 is RAM).
- Needs a **MoonSound** (OPL4); the replayer initialises the chip itself.

## Usage

```c
MoonBlasterWave_Init();                 // copy the replayer into RAM (once)
MoonBlasterWave_Start(song);            // song = a resident .MWM in RAM
for (;;) {
    VDP_WaitVBlank();
    MoonBlasterWave_Tick();             // one frame of music; you drive it
}
// ...later:
MoonBlasterWave_Stop();
```

## Correctness

`moonblaster_wave_01.c` drives the module on emulated hardware (MSX-DOS + MoonSound): `Init`
relocates the 16 KB replayer to 0x4000 (it checks the `"AB"` id and `play_int`'s opcode in
RAM), the song is relocated resident, `Start` initialises the OPL4 and marks the song playing
(`play_busy` becomes nonzero), and `Tick` advances it. It is a **run-yourself** example —
openMSX's MoonSound needs the copyrighted YRW801 wave ROM and MSX-DOS needs a boot disk, so
it is not in the CI sweep:
```sh
EX_EXT=moonsound ./run_example_dos.sh moonblaster_wave_01.c "moonblaster_wave.c" a5 8
```
The replayer's sustained **playback** is verified separately in `experiments/mbwave-irqtime`
(it plays the Jer Der corpus, tick-driven). *(tested: `moonblaster_wave_01.c`)*

## See also
- [Sound (MoonBlaster)](Sound-MoonBlaster.md) — the 1.4 (FM-PAC / MSX-Audio) sibling.
