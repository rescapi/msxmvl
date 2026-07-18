# Sound (MoonBlaster)

Play **MoonBlaster 1.4** songs — real tracker music with FM melodics and ADPCM sample
drums — straight from C. The module ships the **original mbplay replayer** (MoonBlaster and
its replayer are by this library's author, Moonsoft) in an **interrupt-time-optimised**
build, plus a small C API that loads it, feeds it songs and kits, and drives it.

Songs are `.MBM` files; sample kits are `.MBK` files (the drum samples a song's Y8950 plays).
Both come straight off a MoonBlaster disk — no conversion step.

```c
void MoonBlaster_Init(void);                        // replayer -> its fixed home at 0xB000
void MoonBlaster_LoadKit(const u8* mbk, u16 len);   // .MBK -> Y8950 sample RAM (0 = full 32 KB)
void MoonBlaster_Start(const void* song, u8 chips); // install H.TIMI hook, play
void MoonBlaster_Stop(void);                        // silence + remove hook (also "pause")
void MoonBlaster_Continue(void);                    // resume where Stop left off
void MoonBlaster_Fade(u8 speed);                    // fade out; 1 = fastest, 255 = slowest
u8   MoonBlaster_IsPlaying(void);                   // 255 while playing, 0 after
u8   MoonBlaster_Position(void);                    // position in the song's position list
u8   MoonBlaster_Step(void);                        // step within the playing pattern
void MoonBlaster_Tick(void);                        // advance one interrupt by hand (see below)
```

The chip mode passed to `MoonBlaster_Start` is what the song was composed for:

| Mode | Hardware | Drums |
|---|---|---|
| `MOONBLASTER_MSXAUDIO` | Y8950 (Music Module) | ADPCM samples from the kit |
| `MOONBLASTER_MSXMUSIC` | OPLL (FM-PAC) | PSG drums |
| `MOONBLASTER_STEREO` | both chips at once | ADPCM samples |

## Playing a song

The song and the first kilobyte of its sample kit are embedded as C arrays here (a real
program loads both files from disk — see below). After `Start`, **the BIOS plays the song
by itself**: the replayer sits on the H.TIMI hook and does its work inside the 50/60 Hz
interrupt.

```c
// mbsong.c — play a MoonBlaster 1.4 song, sample drums and all, through the
// optimised mbplay replayer.
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
```

This runs to `R[] = a5 ff 0c 08 01 00 01 ff`: playing after `Start` (`ff`), **step 12 after the
fast-forward — exactly (400 interrupts ÷ tempo 9) mod 16**, then step 8 later with no ticking
at all (the BIOS interrupt drove it), the kit read back from Y8950 sample RAM intact, and 0
after `MoonBlaster_Stop`. The bundled song is *"Example Song"* by **C.v/d Geest** from the
original MoonBlaster 1.4 disk, with its kit `EXAMPLE.MBK` (both in `examples/assets/`, included
with permission). *(tested: `moonblaster_01_play.c`)*

`MoonBlaster_Tick` masks interrupts and **leaves them masked**, so a run of ticks is
deterministic; issue `ei` afterwards to hand playback back to the BIOS.

## Loading from disk

At runtime the two files come off disk unmodified: read the `.MBM` anywhere resident, read
the `.MBK` and pass `0` as the length (= the full 32 KB of ADPCM data):

```c
u8  song[2048];                       // EXAMP1.MBM is 430 bytes; size for your song
// dos.c / disk.c reads elided — any way of getting the file bytes into RAM works
MoonBlaster_LoadKit(kit, 0);          // whole kit -> Y8950 sample RAM
MoonBlaster_Start(song, MOONBLASTER_MSXAUDIO);
```

## The memory contract

The replayer keeps its original fixed layout, so its home is not negotiable:

- **`0xB000–0xC50F` belongs to the replayer** after `MoonBlaster_Init` — never load code or
  data there. Under MSX-DOS your `.COM` must end below `0xB000`.
- **The song stays resident while it plays.** The replayer reads pattern data from it inside
  the interrupt handler. If the song lives in a RAM-mapper segment, that segment must be
  mapped in whenever interrupts are enabled.
- **Under MSX-DOS, fix the version byte** before starting: page 0 is RAM there, so the BIOS
  byte the replayer reads at `0x002D` is garbage. `*(u8*)0x002D = 0;` (or the machine's real
  value) once at startup. ROM programs need nothing — page 0 *is* the BIOS.
- **turboR just works**: the replayer self-patches its I/O for the R800.
- `MoonBlaster_LoadKit` is the one call that needs another module: link `msx-audio.c`
  (and check `MSXAudio_Detect()` on machines that might lack the Y8950).

## About this replayer build

This is the original `mbplay` source, optimised for **interrupt time** — what matters when
music runs under a game: mean interrupt cost **−17%**, worst-case interrupt **−21%**,
measured across 25 songs. Every optimisation was gated by a regression oracle: first
byte-identical port traces, then chip-state equivalence (proving the chips receive identical
register state even where redundant writes were skipped). The pre-assembled bytes in
`moonblaster_bin.h` are sha256-gated by `tools/mb14blob.sh` to be **byte-identical to the
build that passed that suite**.

The full annotated source is `lib/ext/moonblaster_player.asm`. If you modify it, read its
`RULES FOR MODIFYING THIS FILE` header first (the data tables must not move, chip I/O has
minimum timing gaps, and the turboR patch table must stay in sync), re-run a regression
against your build, and update the gate hash in `tools/mb14blob.sh`.
