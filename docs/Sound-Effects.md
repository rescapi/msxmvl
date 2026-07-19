# Sound Effects (`ayfx`)

Layer **priority-ranked sound effects** on top of your music, on the PSG every MSX has.
`ayfx` is the ayFX replayer (SapphiRe's format, mvac7's SDCC port) integrated into
msxmvl: it mixes effects straight into the shared PSG shadow ([`psg`](Sound-PSG.md)),
so the single `PSG_Apply()` you already call each frame flushes the combined music +
SFX to the chip. Effects have priorities, so a laser can pre-empt a footstep but not
the other way around. Link `ayfx.c psg.c`, include `ayfx.h`.

> **Provenance:** unlike most of msxmvl, this is **not** clean-room — it is the
> MIT-licensed ayFX replayer, adapted (attribution in `ayfx.c` / `NOTICE.md`). The
> msxmvl value here is integration: it shares one PSG shadow with the music driver,
> uses msxmvl's active-high mixer convention, and ships with a verified interaction
> test — not a from-scratch rewrite.

## API

```c
#include "psg.h"
#include "ayfx.h"

void ayFX_Setup(u16 bank, u8 mode, u8 channel);  // arm over a sample bank (mode: ayFX_FIXED/CYCLE)
u8   ayFX_Play(u8 fx, u8 priority);              // start effect fx (0..255) at priority 0(high)..15(low)
void ayFX_Decode(void);                          // advance one frame; mix into g_PSG_Regs
void ayFX_SetChannel(u8 channel);                // change the output channel (fixed mode)
```

`channel` is `PSG_CHANNEL_A/B/C`. `ayFX_Play` returns `ayFX_OK`, `ayFX_ERR_PRIO` (a
higher-priority effect is still playing) or `ayFX_ERR_INDEX` (no such effect in the
bank).

## Usage

```c
extern const u8 g_SfxBank[];             // an ayFX bank (from the ayfxedit tool)

void init(void) {
	ayFX_Setup((u16)g_SfxBank, ayFX_FIXED, PSG_CHANNEL_C);  // SFX on channel C
}

void on_shoot(void) {
	ayFX_Play(SFX_LASER, 2);             // priority 2: important, but a boss hit (0) wins
}

void game_frame(void) {
	Music_Decode();                      // your music writes g_PSG_Regs (channels A/B)
	ayFX_Decode();                       // the SFX mixes into g_PSG_Regs (channel C)
	PSG_Apply();                         // one flush pushes music + SFX to the chip
}
```

## Sharing the PSG with music

`ayFX_Decode` writes **only** its channel's tone, volume and (optionally) noise, plus
that channel's two mixer bits — every other register is left exactly as your music
driver set it. So reserve one PSG channel for effects (don't play music notes on it)
and both coexist through the one shadow and the one `PSG_Apply`. `ayFX_Setup` does
**not** clear the shadow, so arming the SFX layer never interrupts the music.

## Correctness

`ayfx_01.c` sets up a music tone on channel A, fires an effect on channel C, and
asserts the effect plays (volume + tone on C, both channels enabled in the mixer)
**and** the music on A is untouched — deterministically, on C-BIOS MSX2. The standalone
`experiments/ayfx` rig additionally proves the effect reaches the real chip by reading
the PSG registers each frame (`verify.sh`), and measures the per-frame cost
(idle ~1009 T, active-SFX ~1460 T). *(tested: `ayfx_01.c`)*

## See also
- [Sound (PSG)](Sound-PSG.md) — the shared shadow (`g_PSG_Regs`) and `PSG_Apply`.
- [Sound (MoonBlaster)](Sound-MoonBlaster.md) — a music driver to layer effects over.
